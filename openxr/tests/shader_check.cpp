#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>
#include <iostream>

#include "shaders.h"

using Microsoft::WRL::ComPtr;

bool Compile(const char* entryPoint, const char* profile) {
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3DCompile(
        vr_dead_pixel_test::kShaderSource,
        std::strlen(vr_dead_pixel_test::kShaderSource),
        "VRDeadPixelTestPattern.hlsl", nullptr, nullptr, entryPoint, profile,
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS, 0, &shader,
        &errors);
    if (FAILED(result)) {
        if (errors) {
            std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()),
                            static_cast<std::streamsize>(errors->GetBufferSize()));
        }
        return false;
    }
    return shader && shader->GetBufferSize() > 0;
}

int main() {
    const bool vertexValid = Compile("VertexMain", "vs_5_0");
    const bool pixelValid = Compile("PixelMain", "ps_5_0");
    if (!vertexValid || !pixelValid) {
        return 1;
    }
    std::cout << "VRDeadPixelTest shaders compiled successfully.\n";
    return 0;
}
