# Pixel Flow

Pixel Flow is an experimental display-inspection tool for finding dead, stuck,
or partially stuck pixels. Its low-contrast organic pattern gives the eye a
moving reference field, making a defect that remains fixed to the display easier
to notice.

The repository contains two applications:

- **2D calibration app** — the browser prototype in `app/`, deployed at
  [pixel-flow-display-test.yaroslavdm.chatgpt.site](https://pixel-flow-display-test.yaroslavdm.chatgpt.site).
- **OpenXR app** — the native Windows/Direct3D 11 headset application in
  `openxr/`.

## OpenXR behavior

The VR pattern is stationary in the OpenXR `LOCAL` reference space. At the first
valid tracked frame, the app centres a three-metre-radius inspection sphere on
the headset. Each eye is rendered from its own tracked position by intersecting
its viewing rays with that finite sphere, producing geometrically correct stereo
disparity. Turn your head slowly to move the background across the headset
panels; a panel-fixed defect should not move with it.

The XR field uses broad, curved, constant-color ribbons with deliberately sharp
boundaries. Those boundaries are defined directly on the physical sphere, giving
the two eyes clear matching features for depth fusion without introducing a
texture seam. This diagnostic pattern intentionally prioritizes stereo clarity;
the 2D prototype retains its smooth moving pattern.
Inside each XR ribbon, a low-amplitude, multi-scale stochastic field adds subtle
brightness variation. Its scales are non-harmonic and rotated away from the
tracking axes to avoid forming secondary stripes or periodic banding.
When supported by the runtime, the app renders through a 16-bit floating-point
swapchain. A sub-LSB dither is attached to physical points on the sphere surface,
so quantization reduction remains stereo-coherent between the eyes.

The VR app carries over all 14 calibrated palettes from the 2D prototype:
subdued neutrals and mixed tones, RGB-focused mid-tones, dark checks, and bright
checks. Color changes use the same order in both applications.

### Controls

| Action | Keyboard | Common VR controllers |
| --- | --- | --- |
| Next color | `Space` or `Right Arrow` | Right `A`, right trackpad, or right Select |
| Previous color | `Left Arrow` | Left `X`, left trackpad, or left Select |
| Exit | `Esc` | Right `B` or right Menu |

The small desktop companion window must have keyboard focus for keyboard input.
No interface is drawn inside the headset, so the inspection field remains
unobstructed.

## Build the OpenXR app

Requirements:

- Windows 10 or 11
- An active OpenXR runtime for the connected headset
- Visual Studio with **Desktop development with C++**
- CMake 3.24+ and Ninja

From PowerShell:

```powershell
.\openxr\build.ps1
```

The build downloads the official Khronos OpenXR loader at the pinned 1.1.58
release. Run the result with the headset connected:

```powershell
.\out-openxr\bin\PixelFlowXR.exe
```

If Windows reports that no OpenXR runtime is available, select or install the
runtime supplied by the headset platform, then start Pixel Flow XR again.

If the application returns to the home environment during startup, inspect
`%LOCALAPPDATA%\PixelFlowXR\PixelFlowXR.log`. The log records each initialization
stage and the exact OpenXR or Direct3D error without requiring the headset to
remain in the application.

## Develop the 2D calibration app

```bash
npm install
npm run dev
```

Use `Space`/`Right Arrow` for the next palette, `Left Arrow` for the previous
palette, and `Esc` to end the test session.

Pixel Flow is an inspection aid, not a display certification or medical tool.
