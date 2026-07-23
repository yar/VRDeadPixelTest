#include <windows.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "palettes.h"
#include "shaders.h"

namespace vr_dead_pixel_test {

using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

namespace {

constexpr wchar_t kWindowClassName[] = L"VRDeadPixelTestCompanion";
constexpr wchar_t kWindowTitle[] = L"VRDeadPixelTest";
constexpr float kSphereRadiusMeters = 3.0F;
constexpr float kBrightnessMinimum = 0.5F;
constexpr float kBrightnessMaximum = 1.5F;
constexpr float kBrightnessStep = 0.1F;

std::ofstream gLog;
std::filesystem::path gLogPath;

void Log(const std::string& message) {
    SYSTEMTIME time{};
    GetLocalTime(&time);
    char prefix[32]{};
    sprintf_s(prefix, "[%02u:%02u:%02u.%03u] ", time.wHour, time.wMinute,
              time.wSecond, time.wMilliseconds);
    OutputDebugStringA((std::string(prefix) + message + "\n").c_str());
    if (gLog) {
        gLog << prefix << message << '\n';
        gLog.flush();
    }
}

void InitializeLog() {
    wchar_t localAppData[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
    std::filesystem::path directory =
        length > 0 && length < MAX_PATH
            ? std::filesystem::path(localAppData) / L"VRDeadPixelTest"
            : std::filesystem::temp_directory_path() / L"VRDeadPixelTest";
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    gLogPath = directory / L"VRDeadPixelTest.log";
    gLog.open(gLogPath, std::ios::out | std::ios::trunc);
    Log("VRDeadPixelTest startup");
}

void CheckXr(XrResult result, const char* operation) {
    if (XR_FAILED(result)) {
        const std::string message =
            std::string(operation) + " failed with OpenXR result " +
            std::to_string(static_cast<int>(result)) + ".";
        Log(message);
        throw std::runtime_error(message);
    }
}

void CheckHr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        char code[16]{};
        sprintf_s(code, "0x%08X", static_cast<unsigned int>(result));
        const std::string message =
            std::string(operation) + " failed with HRESULT " + code + ".";
        Log(message);
        throw std::runtime_error(message);
    }
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), size);
    wide.resize(wide.size() - 1);
    return wide;
}

const wchar_t* SessionStateName(XrSessionState state) {
    switch (state) {
        case XR_SESSION_STATE_UNKNOWN:
            return L"Starting OpenXR";
        case XR_SESSION_STATE_IDLE:
            return L"Waiting for headset";
        case XR_SESSION_STATE_READY:
            return L"Headset ready";
        case XR_SESSION_STATE_SYNCHRONIZED:
            return L"Synchronized";
        case XR_SESSION_STATE_VISIBLE:
            return L"Visible in headset";
        case XR_SESSION_STATE_FOCUSED:
            return L"Inspection active";
        case XR_SESSION_STATE_STOPPING:
            return L"Stopping";
        case XR_SESSION_STATE_LOSS_PENDING:
            return L"Runtime connection lost";
        case XR_SESSION_STATE_EXITING:
            return L"Exiting";
        default:
            return L"OpenXR active";
    }
}

bool ShaderUsesLinearColor(DXGI_FORMAT format) {
    return format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
           format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

float SrgbToLinear(float value) {
    return value <= 0.04045F ? value / 12.92F
                            : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

std::array<float, 4> ConvertColor(std::uint32_t rgb, float alpha, bool srgbTarget) {
    auto component = [srgbTarget](std::uint32_t value) {
        const float normalized = static_cast<float>(value) / 255.0F;
        return srgbTarget ? SrgbToLinear(normalized) : normalized;
    };
    return {
        component((rgb >> 16U) & 0xFFU),
        component((rgb >> 8U) & 0xFFU),
        component(rgb & 0xFFU),
        alpha,
    };
}

std::array<float, 4> LerpColor(const std::array<float, 4>& from,
                               const std::array<float, 4>& to, float amount) {
    std::array<float, 4> result{};
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = from[index] + (to[index] - from[index]) * amount;
    }
    return result;
}

}  // namespace

class Application {
  public:
    explicit Application(HINSTANCE module) : module_(module) {}

    ~Application() {
        Shutdown();
    }

    int Run() {
        CreateCompanionWindow();
        InitializeOpenXr();

        while (!exitRequested_) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    RequestQuit();
                    break;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            PollOpenXrEvents();
            if (sessionRunning_) {
                RenderFrame();
            } else {
                Sleep(10);
            }
        }

        return 0;
    }

  private:
    struct Swapchain {
        XrSwapchain handle{XR_NULL_HANDLE};
        int32_t width{};
        int32_t height{};
        std::vector<XrSwapchainImageD3D11KHR> images;
        std::vector<ComPtr<ID3D11RenderTargetView>> renderTargets;
    };

    struct alignas(16) PatternConstants {
        std::array<std::array<float, 4>, 6> colors{};
        std::array<float, 4> contour{};
        std::array<float, 4> fleck{};
        std::array<float, 4> orientation{};
        std::array<float, 4> fov{};
        std::array<float, 4> eyePosition{};
        std::array<float, 4> sphereCenterRadius{};
        std::array<float, 4> outputParameters{};
    };

    static_assert(sizeof(PatternConstants) % 16 == 0);

    static LRESULT CALLBACK StaticWindowProcedure(HWND window, UINT message, WPARAM wParam,
                                                   LPARAM lParam) {
        Application* application = reinterpret_cast<Application*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            application = static_cast<Application*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(application));
            application->window_ = window;
        }

        if (application != nullptr) {
            return application->WindowProcedure(message, wParam, lParam);
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    LRESULT WindowProcedure(UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
            case WM_KEYDOWN:
                if ((lParam & (1LL << 30)) != 0) {
                    return 0;
                }
                if (wParam == VK_SPACE || wParam == VK_RIGHT) {
                    MovePalette(1);
                    return 0;
                }
                if (wParam == VK_LEFT) {
                    MovePalette(-1);
                    return 0;
                }
                if (wParam == VK_UP) {
                    AdjustBrightness(1);
                    return 0;
                }
                if (wParam == VK_DOWN) {
                    AdjustBrightness(-1);
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    RequestQuit();
                    return 0;
                }
                break;
            case WM_CLOSE:
                RequestQuit();
                return 0;
            case WM_PAINT:
                PaintCompanionWindow();
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_DESTROY:
                window_ = nullptr;
                PostQuitMessage(0);
                return 0;
            default:
                break;
        }
        return DefWindowProcW(window_, message, wParam, lParam);
    }

    void CreateCompanionWindow() {
        WNDCLASSEXW windowClass{sizeof(WNDCLASSEXW)};
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = StaticWindowProcedure;
        windowClass.hInstance = module_;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.lpszClassName = kWindowClassName;
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hIconSm = windowClass.hIcon;

        if (RegisterClassExW(&windowClass) == 0 &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            CheckHr(HRESULT_FROM_WIN32(GetLastError()), "RegisterClassExW");
        }

        RECT bounds{0, 0, 560, 280};
        constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectEx(&bounds, style, FALSE, 0);
        const int width = bounds.right - bounds.left;
        const int height = bounds.bottom - bounds.top;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

        window_ = CreateWindowExW(0, kWindowClassName, kWindowTitle, style, x, y, width,
                                  height, nullptr, nullptr, module_, this);
        if (window_ == nullptr) {
            CheckHr(HRESULT_FROM_WIN32(GetLastError()), "CreateWindowExW");
        }

        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);
    }

    void PaintCompanionWindow() {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window_, &paint);
        RECT client{};
        GetClientRect(window_, &client);

        const Palette& palette = kPalettes[paletteIndex_];
        const COLORREF background = RGB((palette.base >> 16U) & 0xFFU,
                                        (palette.base >> 8U) & 0xFFU,
                                        palette.base & 0xFFU);
        const COLORREF foreground = palette.darkInk ? RGB(31, 34, 33) : RGB(246, 248, 244);
        const COLORREF muted = palette.darkInk ? RGB(75, 78, 76) : RGB(214, 219, 214);

        HBRUSH backgroundBrush = CreateSolidBrush(background);
        FillRect(dc, &client, backgroundBrush);
        DeleteObject(backgroundBrush);
        SetBkMode(dc, TRANSPARENT);

        HFONT eyebrowFont = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH, L"Segoe UI");
        HFONT titleFont = CreateFontW(-30, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, L"Segoe UI");
        HFONT bodyFont = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH, L"Segoe UI");

        SetTextColor(dc, muted);
        SelectObject(dc, eyebrowFont);
        constexpr wchar_t eyebrow[] = L"VR DEAD PIXEL TEST  /  OPENXR";
        TextOutW(dc, 30, 25, eyebrow, static_cast<int>(std::size(eyebrow) - 1));

        SetTextColor(dc, foreground);
        SelectObject(dc, titleFont);
        TextOutW(dc, 30, 55, palette.name, static_cast<int>(wcslen(palette.name)));

        SelectObject(dc, bodyFont);
        SetTextColor(dc, muted);
        const int brightnessPercent =
            static_cast<int>(std::lround(brightness_ * 100.0F));
        std::wstring position = std::to_wstring(paletteIndex_ + 1) + L" of " +
                                std::to_wstring(kPalettes.size()) + L"  ·  " +
                                palette.note + L"  ·  " +
                                std::to_wstring(brightnessPercent) + L"% brightness";
        TextOutW(dc, 31, 96, position.c_str(), static_cast<int>(position.size()));

        RECT rule{30, 128, client.right - 30, 129};
        HBRUSH ruleBrush = CreateSolidBrush(
            palette.darkInk ? RGB(162, 164, 158) : RGB(132, 140, 136));
        FillRect(dc, &rule, ruleBrush);
        DeleteObject(ruleBrush);

        SetTextColor(dc, foreground);
        TextOutW(dc, 30, 151, L"SPACE / →   Next color", 22);
        TextOutW(dc, 286, 151, L"←   Previous color", 18);
        TextOutW(dc, 30, 181, L"ESC   Exit", 10);
        TextOutW(dc, 286, 181, L"↑ / ↓   Brightness", 18);

        SetTextColor(dc, muted);
        std::wstring status = std::wstring(L"STATUS   ") +
                              SessionStateName(sessionState_) +
                              L"   ·   SPHERE   3.0 m radius";
        TextOutW(dc, 30, 224, status.c_str(), static_cast<int>(status.size()));

        DeleteObject(eyebrowFont);
        DeleteObject(titleFont);
        DeleteObject(bodyFont);
        EndPaint(window_, &paint);
    }

    void InitializeOpenXr() {
        Log("Creating OpenXR instance");
        CreateInstance();
        Log("Selecting headset system");
        SelectSystem();
        Log("Creating runtime-selected D3D11 device");
        CreateD3DDevice();
        Log("Creating OpenXR session");
        CreateSession();
        Log("Creating LOCAL reference space");
        CreateReferenceSpace();
        Log("Creating controller actions");
        CreateActions();
        Log("Creating per-eye swapchains");
        CreateSwapchains();
        Log("Compiling render pipeline");
        CreateRenderPipeline();
        Log("Initialization complete; waiting for the session to become ready");
        InvalidateRect(window_, nullptr, FALSE);
    }

    void CreateInstance() {
        uint32_t extensionCount = 0;
        CheckXr(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr),
                "xrEnumerateInstanceExtensionProperties");
        std::vector<XrExtensionProperties> extensions(extensionCount);
        for (auto& extension : extensions) {
            extension.type = XR_TYPE_EXTENSION_PROPERTIES;
        }
        CheckXr(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount,
                                                       extensions.data()),
                "xrEnumerateInstanceExtensionProperties");

        const bool supportsD3D11 = std::any_of(
            extensions.begin(), extensions.end(), [](const XrExtensionProperties& extension) {
                return std::strcmp(extension.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0;
            });
        if (!supportsD3D11) {
            throw std::runtime_error(
                "The active OpenXR runtime does not expose XR_KHR_D3D11_enable.");
        }

        const char* enabledExtensions[] = {XR_KHR_D3D11_ENABLE_EXTENSION_NAME};
        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(createInfo.applicationInfo.applicationName, "VRDeadPixelTest");
        createInfo.applicationInfo.applicationVersion = 1;
        strcpy_s(createInfo.applicationInfo.engineName, "VRDeadPixelTest Native");
        createInfo.applicationInfo.engineVersion = 1;
        createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        createInfo.enabledExtensionCount = 1;
        createInfo.enabledExtensionNames = enabledExtensions;
        CheckXr(xrCreateInstance(&createInfo, &instance_), "xrCreateInstance");
    }

    void SelectSystem() {
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        CheckXr(xrGetSystem(instance_, &systemInfo, &systemId_), "xrGetSystem");

        uint32_t viewTypeCount = 0;
        CheckXr(xrEnumerateViewConfigurations(instance_, systemId_, 0, &viewTypeCount, nullptr),
                "xrEnumerateViewConfigurations");
        std::vector<XrViewConfigurationType> viewTypes(viewTypeCount);
        CheckXr(xrEnumerateViewConfigurations(instance_, systemId_, viewTypeCount, &viewTypeCount,
                                              viewTypes.data()),
                "xrEnumerateViewConfigurations");
        if (std::find(viewTypes.begin(), viewTypes.end(),
                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) == viewTypes.end()) {
            throw std::runtime_error("The connected OpenXR system does not support stereo views.");
        }

        uint32_t blendModeCount = 0;
        CheckXr(xrEnumerateEnvironmentBlendModes(
                    instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                    &blendModeCount, nullptr),
                "xrEnumerateEnvironmentBlendModes");
        std::vector<XrEnvironmentBlendMode> blendModes(blendModeCount);
        CheckXr(xrEnumerateEnvironmentBlendModes(
                    instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                    blendModeCount, &blendModeCount, blendModes.data()),
                "xrEnumerateEnvironmentBlendModes");
        const auto opaque = std::find(blendModes.begin(), blendModes.end(),
                                      XR_ENVIRONMENT_BLEND_MODE_OPAQUE);
        blendMode_ = opaque != blendModes.end() ? *opaque : blendModes.front();
    }

    void CreateD3DDevice() {
        PFN_xrGetD3D11GraphicsRequirementsKHR getRequirements = nullptr;
        CheckXr(xrGetInstanceProcAddr(
                    instance_, "xrGetD3D11GraphicsRequirementsKHR",
                    reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)),
                "xrGetInstanceProcAddr(xrGetD3D11GraphicsRequirementsKHR)");

        XrGraphicsRequirementsD3D11KHR requirements{
            XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};
        CheckXr(getRequirements(instance_, systemId_, &requirements),
                "xrGetD3D11GraphicsRequirementsKHR");

        ComPtr<IDXGIFactory1> factory;
        CheckHr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "CreateDXGIFactory1");

        ComPtr<IDXGIAdapter1> selectedAdapter;
        for (UINT adapterIndex = 0;; ++adapterIndex) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_ADAPTER_DESC1 description{};
            CheckHr(adapter->GetDesc1(&description), "IDXGIAdapter1::GetDesc1");
            if (std::memcmp(&description.AdapterLuid, &requirements.adapterLuid,
                            sizeof(LUID)) == 0) {
                selectedAdapter = adapter;
                break;
            }
        }
        if (!selectedAdapter) {
            throw std::runtime_error("The graphics adapter requested by OpenXR was not found.");
        }

        constexpr std::array featureLevels{
            D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
        D3D_FEATURE_LEVEL createdFeatureLevel{};
        CheckHr(D3D11CreateDevice(
                    selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT, featureLevels.data(),
                    static_cast<UINT>(featureLevels.size()), D3D11_SDK_VERSION, &device_,
                    &createdFeatureLevel, &context_),
                "D3D11CreateDevice");
        if (createdFeatureLevel < requirements.minFeatureLevel) {
            throw std::runtime_error(
                "The selected graphics adapter does not meet the OpenXR feature-level requirement.");
        }
    }

    void CreateSession() {
        XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
        graphicsBinding.device = device_.Get();

        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
        sessionInfo.next = &graphicsBinding;
        sessionInfo.systemId = systemId_;
        CheckXr(xrCreateSession(instance_, &sessionInfo, &session_), "xrCreateSession");
    }

    void CreateReferenceSpace() {
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0F;
        CheckXr(xrCreateReferenceSpace(session_, &spaceInfo, &localSpace_),
                "xrCreateReferenceSpace");
    }

    XrAction CreateBooleanAction(const char* name, const char* localizedName) {
        XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
        actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
        strcpy_s(actionInfo.actionName, name);
        strcpy_s(actionInfo.localizedActionName, localizedName);
        XrAction action{XR_NULL_HANDLE};
        CheckXr(xrCreateAction(actionSet_, &actionInfo, &action), "xrCreateAction");
        return action;
    }

    void SuggestBindings(
        const char* profile,
        std::initializer_list<std::pair<XrAction, const char*>> bindingDescriptions) {
        XrPath profilePath{XR_NULL_PATH};
        if (XR_FAILED(xrStringToPath(instance_, profile, &profilePath))) {
            return;
        }

        std::vector<XrActionSuggestedBinding> bindings;
        bindings.reserve(bindingDescriptions.size());
        for (const auto& [action, pathText] : bindingDescriptions) {
            XrPath bindingPath{XR_NULL_PATH};
            if (XR_SUCCEEDED(xrStringToPath(instance_, pathText, &bindingPath))) {
                bindings.push_back({action, bindingPath});
            }
        }

        XrInteractionProfileSuggestedBinding suggestion{
            XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggestion.interactionProfile = profilePath;
        suggestion.countSuggestedBindings = static_cast<uint32_t>(bindings.size());
        suggestion.suggestedBindings = bindings.data();
        const XrResult result = xrSuggestInteractionProfileBindings(instance_, &suggestion);
        if (XR_FAILED(result)) {
            OutputDebugStringA((std::string("VRDeadPixelTest: runtime declined bindings for ") +
                                profile + "\n")
                                   .c_str());
        }
    }

    void CreateActions() {
        XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy_s(setInfo.actionSetName, "inspection");
        strcpy_s(setInfo.localizedActionSetName, "Display inspection");
        setInfo.priority = 0;
        CheckXr(xrCreateActionSet(instance_, &setInfo, &actionSet_), "xrCreateActionSet");

        nextAction_ = CreateBooleanAction("next_color", "Next color");
        previousAction_ = CreateBooleanAction("previous_color", "Previous color");
        quitAction_ = CreateBooleanAction("exit_test", "Exit test");

        SuggestBindings("/interaction_profiles/oculus/touch_controller",
                        {{nextAction_, "/user/hand/right/input/a/click"},
                         {previousAction_, "/user/hand/left/input/x/click"},
                         {quitAction_, "/user/hand/right/input/b/click"}});
        SuggestBindings("/interaction_profiles/valve/index_controller",
                        {{nextAction_, "/user/hand/right/input/a/click"},
                         {previousAction_, "/user/hand/left/input/a/click"},
                         {quitAction_, "/user/hand/right/input/b/click"}});
        SuggestBindings("/interaction_profiles/microsoft/motion_controller",
                        {{nextAction_, "/user/hand/right/input/trackpad/click"},
                         {previousAction_, "/user/hand/left/input/trackpad/click"},
                         {quitAction_, "/user/hand/right/input/menu/click"}});
        SuggestBindings("/interaction_profiles/htc/vive_controller",
                        {{nextAction_, "/user/hand/right/input/trackpad/click"},
                         {previousAction_, "/user/hand/left/input/trackpad/click"},
                         {quitAction_, "/user/hand/right/input/menu/click"}});
        SuggestBindings("/interaction_profiles/khr/simple_controller",
                        {{nextAction_, "/user/hand/right/input/select/click"},
                         {previousAction_, "/user/hand/left/input/select/click"},
                         {quitAction_, "/user/hand/right/input/menu/click"}});

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &actionSet_;
        CheckXr(xrAttachSessionActionSets(session_, &attachInfo),
                "xrAttachSessionActionSets");
    }

    void CreateSwapchains() {
        uint32_t configurationViewCount = 0;
        CheckXr(xrEnumerateViewConfigurationViews(
                    instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0,
                    &configurationViewCount, nullptr),
                "xrEnumerateViewConfigurationViews");
        configurationViews_.resize(configurationViewCount);
        for (auto& view : configurationViews_) {
            view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        }
        CheckXr(xrEnumerateViewConfigurationViews(
                    instance_, systemId_, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                    configurationViewCount, &configurationViewCount,
                    configurationViews_.data()),
                "xrEnumerateViewConfigurationViews");

        uint32_t formatCount = 0;
        CheckXr(xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr),
                "xrEnumerateSwapchainFormats");
        std::vector<int64_t> runtimeFormats(formatCount);
        CheckXr(xrEnumerateSwapchainFormats(session_, formatCount, &formatCount,
                                            runtimeFormats.data()),
                "xrEnumerateSwapchainFormats");

        constexpr std::array preferredFormats{
            DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM};
        auto selectedFormat = runtimeFormats.end();
        for (DXGI_FORMAT preferred : preferredFormats) {
            selectedFormat = std::find(runtimeFormats.begin(), runtimeFormats.end(),
                                       static_cast<int64_t>(preferred));
            if (selectedFormat != runtimeFormats.end()) {
                break;
            }
        }
        if (selectedFormat == runtimeFormats.end()) {
            throw std::runtime_error("The runtime has no compatible color swapchain format.");
        }
        swapchainFormat_ = static_cast<DXGI_FORMAT>(*selectedFormat);
        shaderUsesLinearColor_ = ShaderUsesLinearColor(swapchainFormat_);
        Log("Selected swapchain format " +
            std::to_string(static_cast<int>(swapchainFormat_)) +
            (swapchainFormat_ == DXGI_FORMAT_R16G16B16A16_FLOAT
                 ? " (16-bit floating point)"
                 : " (8-bit fallback)"));

        swapchains_.resize(configurationViewCount);
        views_.resize(configurationViewCount);
        projectionViews_.resize(configurationViewCount);
        for (uint32_t viewIndex = 0; viewIndex < configurationViewCount; ++viewIndex) {
            views_[viewIndex].type = XR_TYPE_VIEW;
            projectionViews_[viewIndex].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;

            Swapchain& swapchain = swapchains_[viewIndex];
            const XrViewConfigurationView& configuration = configurationViews_[viewIndex];
            swapchain.width = static_cast<int32_t>(configuration.recommendedImageRectWidth);
            swapchain.height = static_cast<int32_t>(configuration.recommendedImageRectHeight);

            XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            createInfo.format = *selectedFormat;
            // D3D11 rendering in the Khronos sample uses a supported sample count
            // of one. The runtime performs its own final panel composition.
            createInfo.sampleCount = 1;
            createInfo.width = configuration.recommendedImageRectWidth;
            createInfo.height = configuration.recommendedImageRectHeight;
            createInfo.faceCount = 1;
            createInfo.arraySize = 1;
            createInfo.mipCount = 1;
            CheckXr(xrCreateSwapchain(session_, &createInfo, &swapchain.handle),
                    "xrCreateSwapchain");

            uint32_t imageCount = 0;
            CheckXr(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr),
                    "xrEnumerateSwapchainImages");
            swapchain.images.resize(imageCount);
            for (auto& image : swapchain.images) {
                image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
            }
            CheckXr(xrEnumerateSwapchainImages(
                        swapchain.handle, imageCount, &imageCount,
                        reinterpret_cast<XrSwapchainImageBaseHeader*>(swapchain.images.data())),
                    "xrEnumerateSwapchainImages");

            swapchain.renderTargets.resize(imageCount);
            for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
                // OpenXR D3D11 swapchain textures are typeless. An explicit view
                // format is therefore required; asking D3D11 to infer it fails on
                // SteamVR during initialization.
                D3D11_RENDER_TARGET_VIEW_DESC renderTargetDescription{};
                renderTargetDescription.Format = swapchainFormat_;
                renderTargetDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                renderTargetDescription.Texture2DArray.MipSlice = 0;
                renderTargetDescription.Texture2DArray.FirstArraySlice = 0;
                renderTargetDescription.Texture2DArray.ArraySize = 1;
                CheckHr(device_->CreateRenderTargetView(
                            swapchain.images[imageIndex].texture, &renderTargetDescription,
                            &swapchain.renderTargets[imageIndex]),
                        "ID3D11Device::CreateRenderTargetView");
            }
            Log("Created eye " + std::to_string(viewIndex) + " swapchain: " +
                std::to_string(swapchain.width) + "x" +
                std::to_string(swapchain.height) + ", " +
                std::to_string(imageCount) + " images");
        }
    }

    ComPtr<ID3DBlob> CompileShader(const char* entryPoint, const char* profile) {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(_DEBUG)
        flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG |
                D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> shader;
        ComPtr<ID3DBlob> errors;
        const HRESULT result = D3DCompile(kShaderSource, std::strlen(kShaderSource),
                                          "VRDeadPixelTestPattern.hlsl", nullptr, nullptr,
                                          entryPoint, profile, flags, 0, &shader, &errors);
        if (FAILED(result)) {
            const std::string detail = errors
                                           ? std::string(static_cast<const char*>(
                                                             errors->GetBufferPointer()),
                                                         errors->GetBufferSize())
                                           : "No shader compiler detail was provided.";
            throw std::runtime_error("Shader compilation failed: " + detail);
        }
        return shader;
    }

    void CreateRenderPipeline() {
        const ComPtr<ID3DBlob> vertexBytecode = CompileShader("VertexMain", "vs_5_0");
        const ComPtr<ID3DBlob> pixelBytecode = CompileShader("PixelMain", "ps_5_0");
        CheckHr(device_->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                                            vertexBytecode->GetBufferSize(), nullptr,
                                            &vertexShader_),
                "ID3D11Device::CreateVertexShader");
        CheckHr(device_->CreatePixelShader(pixelBytecode->GetBufferPointer(),
                                           pixelBytecode->GetBufferSize(), nullptr,
                                           &pixelShader_),
                "ID3D11Device::CreatePixelShader");

        D3D11_BUFFER_DESC bufferDescription{};
        bufferDescription.ByteWidth = sizeof(PatternConstants);
        bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
        bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        CheckHr(device_->CreateBuffer(&bufferDescription, nullptr, &constantBuffer_),
                "ID3D11Device::CreateBuffer");

        D3D11_RASTERIZER_DESC rasterizerDescription{};
        rasterizerDescription.FillMode = D3D11_FILL_SOLID;
        rasterizerDescription.CullMode = D3D11_CULL_NONE;
        rasterizerDescription.DepthClipEnable = TRUE;
        CheckHr(device_->CreateRasterizerState(&rasterizerDescription, &rasterizerState_),
                "ID3D11Device::CreateRasterizerState");
    }

    void PollOpenXrEvents() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult result = XR_SUCCESS;
        while ((result = xrPollEvent(instance_, &event)) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                exitRequested_ = true;
            } else if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto& stateEvent =
                    *reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                sessionState_ = stateEvent.state;
                Log("Session state changed to " +
                    std::to_string(static_cast<int>(sessionState_)));
                InvalidateRect(window_, nullptr, FALSE);

                switch (sessionState_) {
                    case XR_SESSION_STATE_READY: {
                        sphereCenterInitialized_ = false;
                        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                        beginInfo.primaryViewConfigurationType =
                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        CheckXr(xrBeginSession(session_, &beginInfo), "xrBeginSession");
                        sessionRunning_ = true;
                        break;
                    }
                    case XR_SESSION_STATE_STOPPING:
                        sessionRunning_ = false;
                        sphereCenterInitialized_ = false;
                        CheckXr(xrEndSession(session_), "xrEndSession");
                        if (quitRequested_) {
                            exitRequested_ = true;
                        }
                        break;
                    case XR_SESSION_STATE_EXITING:
                    case XR_SESSION_STATE_LOSS_PENDING:
                        exitRequested_ = true;
                        break;
                    default:
                        break;
                }
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if (XR_FAILED(result)) {
            CheckXr(result, "xrPollEvent");
        }
    }

    bool WasPressed(XrAction action) const {
        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        getInfo.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        const XrResult result = xrGetActionStateBoolean(session_, &getInfo, &state);
        return XR_SUCCEEDED(result) && state.isActive == XR_TRUE &&
               state.changedSinceLastSync == XR_TRUE && state.currentState == XR_TRUE;
    }

    void SyncActions() {
        XrActiveActionSet activeActionSet{actionSet_, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        const XrResult result = xrSyncActions(session_, &syncInfo);
        if (XR_FAILED(result)) {
            return;
        }
        if (WasPressed(nextAction_)) {
            MovePalette(1);
        }
        if (WasPressed(previousAction_)) {
            MovePalette(-1);
        }
        if (WasPressed(quitAction_)) {
            RequestQuit();
        }
    }

    void RenderFrame() {
        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        CheckXr(xrWaitFrame(session_, &waitInfo, &frameState), "xrWaitFrame");

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        CheckXr(xrBeginFrame(session_, &beginInfo), "xrBeginFrame");
        SyncActions();

        std::vector<XrCompositionLayerBaseHeader*> layers;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};

        if (frameState.shouldRender == XR_TRUE) {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.space = localSpace_;

            XrViewState viewState{XR_TYPE_VIEW_STATE};
            uint32_t viewCount = 0;
            CheckXr(xrLocateViews(session_, &locateInfo, &viewState,
                                  static_cast<uint32_t>(views_.size()), &viewCount,
                                  views_.data()),
                    "xrLocateViews");

            constexpr XrViewStateFlags requiredFlags = XR_VIEW_STATE_ORIENTATION_VALID_BIT |
                                                       XR_VIEW_STATE_POSITION_VALID_BIT;
            if (viewCount == views_.size() &&
                (viewState.viewStateFlags & requiredFlags) == requiredFlags) {
                if (!sphereCenterInitialized_) {
                    sphereCenter_ = {0.0F, 0.0F, 0.0F};
                    for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
                        sphereCenter_[0] += views_[viewIndex].pose.position.x;
                        sphereCenter_[1] += views_[viewIndex].pose.position.y;
                        sphereCenter_[2] += views_[viewIndex].pose.position.z;
                    }
                    const float inverseViewCount = 1.0F / static_cast<float>(viewCount);
                    for (float& component : sphereCenter_) {
                        component *= inverseViewCount;
                    }
                    sphereCenterInitialized_ = true;
                    Log("Captured inspection sphere center at [" +
                        std::to_string(sphereCenter_[0]) + ", " +
                        std::to_string(sphereCenter_[1]) + ", " +
                        std::to_string(sphereCenter_[2]) + "] with radius " +
                        std::to_string(kSphereRadiusMeters) + " m");
                }
                for (uint32_t viewIndex = 0; viewIndex < viewCount; ++viewIndex) {
                    RenderView(viewIndex, views_[viewIndex]);
                    XrCompositionLayerProjectionView& projection = projectionViews_[viewIndex];
                    projection.pose = views_[viewIndex].pose;
                    projection.fov = views_[viewIndex].fov;
                    projection.subImage.swapchain = swapchains_[viewIndex].handle;
                    projection.subImage.imageRect.offset = {0, 0};
                    projection.subImage.imageRect.extent = {swapchains_[viewIndex].width,
                                                            swapchains_[viewIndex].height};
                    projection.subImage.imageArrayIndex = 0;
                }

                layer.space = localSpace_;
                layer.viewCount = viewCount;
                layer.views = projectionViews_.data();
                layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer));
            }
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = blendMode_;
        endInfo.layerCount = static_cast<uint32_t>(layers.size());
        endInfo.layers = layers.empty() ? nullptr : layers.data();
        CheckXr(xrEndFrame(session_, &endInfo), "xrEndFrame");
    }

    PatternConstants BuildConstants(const XrView& view) const {
        const float elapsedMilliseconds = static_cast<float>(
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() -
                                                                  paletteChangeTime_)
                .count());
        const float blend = std::clamp(elapsedMilliseconds / 360.0F, 0.0F, 1.0F);
        const Palette& previous = kPalettes[previousPaletteIndex_];
        const Palette& current = kPalettes[paletteIndex_];

        PatternConstants constants{};
        constants.colors[0] = LerpColor(
            ConvertColor(previous.base, 1.0F, shaderUsesLinearColor_),
            ConvertColor(current.base, 1.0F, shaderUsesLinearColor_), blend);
        for (std::size_t index = 0; index < current.bands.size(); ++index) {
            constants.colors[index + 1] = LerpColor(
                ConvertColor(previous.bands[index], 1.0F, shaderUsesLinearColor_),
                ConvertColor(current.bands[index], 1.0F, shaderUsesLinearColor_), blend);
        }
        constants.contour = LerpColor(
            ConvertColor(previous.contour.rgb, previous.contour.opacity,
                         shaderUsesLinearColor_),
            ConvertColor(current.contour.rgb, current.contour.opacity,
                         shaderUsesLinearColor_),
            blend);
        constants.fleck = LerpColor(
            ConvertColor(previous.fleck.rgb, previous.fleck.opacity,
                         shaderUsesLinearColor_),
            ConvertColor(current.fleck.rgb, current.fleck.opacity,
                         shaderUsesLinearColor_), blend);

        constants.orientation = {view.pose.orientation.x, view.pose.orientation.y,
                                 view.pose.orientation.z, view.pose.orientation.w};
        constants.fov = {std::tan(view.fov.angleLeft), std::tan(view.fov.angleRight),
                         std::tan(view.fov.angleUp), std::tan(view.fov.angleDown)};
        constants.eyePosition = {view.pose.position.x, view.pose.position.y,
                                 view.pose.position.z, 1.0F};
        constants.sphereCenterRadius = {sphereCenter_[0], sphereCenter_[1],
                                        sphereCenter_[2], kSphereRadiusMeters};
        constants.outputParameters = {
            shaderUsesLinearColor_ ? 1.0F : 0.0F,
            0.9F / 255.0F,
            brightness_,
            0.0F,
        };
        return constants;
    }

    void RenderView(uint32_t viewIndex, const XrView& view) {
        Swapchain& swapchain = swapchains_[viewIndex];
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        uint32_t imageIndex = 0;
        CheckXr(xrAcquireSwapchainImage(swapchain.handle, &acquireInfo, &imageIndex),
                "xrAcquireSwapchainImage");

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        CheckXr(xrWaitSwapchainImage(swapchain.handle, &waitInfo),
                "xrWaitSwapchainImage");

        const PatternConstants constants = BuildConstants(view);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        CheckHr(context_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped),
                "ID3D11DeviceContext::Map");
        std::memcpy(mapped.pData, &constants, sizeof(constants));
        context_->Unmap(constantBuffer_.Get(), 0);

        D3D11_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(swapchain.width);
        viewport.Height = static_cast<float>(swapchain.height);
        viewport.MinDepth = 0.0F;
        viewport.MaxDepth = 1.0F;

        ID3D11RenderTargetView* target = swapchain.renderTargets[imageIndex].Get();
        ID3D11Buffer* constantBuffer = constantBuffer_.Get();
        context_->OMSetRenderTargets(1, &target, nullptr);
        context_->RSSetViewports(1, &viewport);
        context_->RSSetState(rasterizerState_.Get());
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer);
        context_->Draw(3, 0);
        context_->OMSetRenderTargets(0, nullptr, nullptr);

        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        CheckXr(xrReleaseSwapchainImage(swapchain.handle, &releaseInfo),
                "xrReleaseSwapchainImage");
    }

    void MovePalette(int direction) {
        previousPaletteIndex_ = paletteIndex_;
        const int count = static_cast<int>(kPalettes.size());
        paletteIndex_ = static_cast<std::size_t>(
            (static_cast<int>(paletteIndex_) + direction + count) % count);
        paletteChangeTime_ = Clock::now();
        if (window_ != nullptr) {
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    void AdjustBrightness(int direction) {
        const float next = brightness_ + static_cast<float>(direction) * kBrightnessStep;
        brightness_ = std::clamp(std::round(next * 10.0F) / 10.0F,
                                 kBrightnessMinimum, kBrightnessMaximum);
        Log("Brightness set to " +
            std::to_string(static_cast<int>(std::lround(brightness_ * 100.0F))) +
            "%");
        if (window_ != nullptr) {
            InvalidateRect(window_, nullptr, FALSE);
        }
    }

    void RequestQuit() {
        if (quitRequested_) {
            return;
        }
        quitRequested_ = true;
        if (sessionRunning_ && session_ != XR_NULL_HANDLE) {
            const XrResult result = xrRequestExitSession(session_);
            if (XR_FAILED(result)) {
                exitRequested_ = true;
            }
        } else {
            exitRequested_ = true;
        }
    }

    void Shutdown() noexcept {
        if (context_) {
            context_->ClearState();
            context_->Flush();
        }

        for (Swapchain& swapchain : swapchains_) {
            swapchain.renderTargets.clear();
            if (swapchain.handle != XR_NULL_HANDLE) {
                xrDestroySwapchain(swapchain.handle);
                swapchain.handle = XR_NULL_HANDLE;
            }
        }
        swapchains_.clear();

        if (localSpace_ != XR_NULL_HANDLE) {
            xrDestroySpace(localSpace_);
            localSpace_ = XR_NULL_HANDLE;
        }
        if (session_ != XR_NULL_HANDLE) {
            xrDestroySession(session_);
            session_ = XR_NULL_HANDLE;
        }
        if (actionSet_ != XR_NULL_HANDLE) {
            xrDestroyActionSet(actionSet_);
            actionSet_ = XR_NULL_HANDLE;
        }
        if (instance_ != XR_NULL_HANDLE) {
            xrDestroyInstance(instance_);
            instance_ = XR_NULL_HANDLE;
        }
        if (window_ != nullptr) {
            DestroyWindow(window_);
            window_ = nullptr;
        }
        UnregisterClassW(kWindowClassName, module_);
    }

    HINSTANCE module_{};
    HWND window_{};

    XrInstance instance_{XR_NULL_HANDLE};
    XrSystemId systemId_{XR_NULL_SYSTEM_ID};
    XrSession session_{XR_NULL_HANDLE};
    XrSpace localSpace_{XR_NULL_HANDLE};
    XrSessionState sessionState_{XR_SESSION_STATE_UNKNOWN};
    XrEnvironmentBlendMode blendMode_{XR_ENVIRONMENT_BLEND_MODE_OPAQUE};
    bool sessionRunning_{};
    bool quitRequested_{};
    bool exitRequested_{};

    XrActionSet actionSet_{XR_NULL_HANDLE};
    XrAction nextAction_{XR_NULL_HANDLE};
    XrAction previousAction_{XR_NULL_HANDLE};
    XrAction quitAction_{XR_NULL_HANDLE};

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11Buffer> constantBuffer_;
    ComPtr<ID3D11RasterizerState> rasterizerState_;
    DXGI_FORMAT swapchainFormat_{DXGI_FORMAT_UNKNOWN};
    bool shaderUsesLinearColor_{};

    std::vector<XrViewConfigurationView> configurationViews_;
    std::vector<XrView> views_;
    std::vector<XrCompositionLayerProjectionView> projectionViews_;
    std::vector<Swapchain> swapchains_;

    std::size_t paletteIndex_{};
    std::size_t previousPaletteIndex_{};
    Clock::time_point paletteChangeTime_{Clock::now() - std::chrono::seconds(1)};
    float brightness_{1.0F};
    std::array<float, 3> sphereCenter_{};
    bool sphereCenterInitialized_{};
};

}  // namespace vr_dead_pixel_test

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    vr_dead_pixel_test::InitializeLog();
    try {
        vr_dead_pixel_test::Application application(instance);
        return application.Run();
    } catch (const std::exception& error) {
        vr_dead_pixel_test::Log(std::string("Fatal error: ") + error.what());
        std::wstring message = vr_dead_pixel_test::Utf8ToWide(error.what());
        if (!vr_dead_pixel_test::gLogPath.empty()) {
            message += L"\n\nDiagnostic log:\n" +
                       vr_dead_pixel_test::gLogPath.wstring();
        }
        MessageBoxW(nullptr, message.c_str(), L"VRDeadPixelTest could not start",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}
