# VRDeadPixelTest

A bad pixel can be surprisingly hard to confirm. A tiny dark or bright dot may
look like dust, a reflection, or simply part of the picture. Solid-color test
pages help, but after staring at an unmoving screen for a while your eyes can
start overlooking small stationary details.

VRDeadPixelTest gives your eyes a moving reference instead. The background has
soft detail and gentle brightness changes, while a real panel defect stays fixed
to the same physical pixel. That difference in movement can make the defect much
easier to notice.

There are two versions:

- [Open the 2D test in a browser](https://pixel-flow-display-test.yaroslavdm.chatgpt.site)
  for monitors and TVs.
- [Download the latest Windows release](https://github.com/yar/VRDeadPixelTest/releases/latest)
  for OpenXR headsets.

## Before you start

- Clean the screen or headset lenses first. Dust can look very much like a bad
  pixel.
- Use your normal viewing position and let the display warm up for a few
  minutes.
- In a headset, set the lens spacing and fit as carefully as you normally would.
  A poorly fitted headset can make the whole image look soft.
- Move slowly and take breaks. Stop if the moving pattern feels uncomfortable.

These tools can help you inspect a display, but they cannot repair pixels or
replace the manufacturer's own acceptance test.

## Using the 2D test

1. Open the [browser version](https://pixel-flow-display-test.yaroslavdm.chatgpt.site).
2. Press `F` for fullscreen.
3. Relax your gaze and let the pattern drift. Scan the display rather than
   staring hard at one spot.
4. Press `Space` or `Right Arrow` to try the next color. Different colors help
   reveal different failed subpixels.
5. Use `Up Arrow` and `Down Arrow` to change brightness. Start at 100%, then
   check both brighter and darker settings.
6. Press `P` if you want to pause and compare the moving and stationary views.

| Key | What it does |
| --- | --- |
| `Space` or `Right Arrow` | Next color |
| `Left Arrow` | Previous color |
| `Up Arrow` | Increase brightness by 10% |
| `Down Arrow` | Decrease brightness by 10% |
| `P` | Pause or resume movement |
| `F` | Enter or leave fullscreen |
| `Esc` | End the test session |

Brightness starts at 100% and is limited to 50–150%.

## Using the VR test

1. Make sure your headset's OpenXR runtime is active. For a Valve Index, this
   will normally be SteamVR.
2. Download and run `VRDeadPixelTest.exe` from the
   [latest release](https://github.com/yar/VRDeadPixelTest/releases/latest).
3. Put on the headset and turn your head slowly. The pattern stays in the
   virtual room, so it moves across the headset panels as your view changes.
4. Work through the colors and brightness levels. It can help to close one eye
   at a time, especially when checking a suspected spot.
5. Keep the small desktop companion window focused when using keyboard controls.

| Action | Keyboard | Index and other common controllers |
| --- | --- | --- |
| Next color | `Space` or `Right Arrow` | Right `A`, right trackpad, or right Select |
| Previous color | `Left Arrow` | Left `X`, left trackpad, or left Select |
| Increase brightness | `Up Arrow` | Keyboard only |
| Decrease brightness | `Down Arrow` | Keyboard only |
| Exit | `Esc` | Right `B` or right Menu |

Brightness uses the same 50–150% range and 10% steps as the browser version.

## What to look for

- A **dead pixel** usually remains dark when the surrounding pattern becomes
  bright.
- A **stuck pixel** may remain bright, or stay red, green, or blue when the
  surrounding colors change.
- A **partly failed pixel** may only stand out on certain colors, which is why it
  is worth checking every palette.
- A panel defect stays in the same place on the physical display. Pattern detail
  moves past it.
- If a mark might be dust, clean the surface and repeat the test before drawing
  a conclusion.

# Part 2: How it works under the hood

## The basic idea

Both programs place low-contrast detail behind the display pixels. The detail
gives your vision something to track. Background features move, while a physical
pixel defect cannot move, so the defect separates from the flow.

The bands have sharp edges to give clear reference points. Within each band are
small, irregular brightness changes and fine dithering. The changes are blended
from several different sizes so they do not settle into another obvious striped
pattern.

## The 2D version

The browser draws a wide repeating canvas tile and slides it horizontally. Its
in-band brightness variation is deliberately stronger than the VR version,
reaching about 12% at the most extreme possible points. Fine random dithering
helps prevent large patches from collapsing to exactly the same 8-bit color.

The detailed tile is prepared only when the page opens, the window changes size,
or a new palette is selected. Normal animation just copies that finished image,
which keeps the frame-by-frame workload small. Brightness adjustment is handled
separately, so pressing the arrow keys does not rebuild the pattern.

## The VR version

The native app uses OpenXR and Direct3D 11. It places a three-metre-radius sphere
around the headset's starting position. Each eye views the same physical points
on that sphere from its own tracked position, so the band boundaries match
properly between the eyes and have the expected depth.

The sphere stays fixed in OpenXR's local space. Instead of automatically moving
the texture, the app lets normal head movement sweep the pattern across the
headset panels. Brightness is applied after the pattern is drawn, using the same
50–150% range as the 2D version.

When the OpenXR runtime supports it, the app uses a 16-bit floating-point color
buffer to reduce visible color steps. It also adds very fine sphere-locked
dithering. If only an 8-bit buffer is available, the same dither helps soften
quantization without giving the two eyes unrelated noise.

## Colors

Both versions use the same fourteen palettes. They begin with subdued greys and
mixed colors, continue through red, green, and blue checks, and finish with dark
and bright options. Changing brightness does not change the selected palette or
the shape of the pattern.

## Building from source

The browser source is in `app/`. To run it locally:

```bash
npm install
npm run dev
```

The native headset source is in `openxr/`. It currently requires:

- Windows 10 or 11
- An active OpenXR runtime
- Visual Studio with **Desktop development with C++**
- CMake 3.24 or newer and Ninja

Build it from PowerShell:

```powershell
.\openxr\build.ps1
```

The resulting program is:

```text
out-openxr\bin\VRDeadPixelTest.exe
```

If the VR app closes during startup, check:

```text
%LOCALAPPDATA%\VRDeadPixelTest\VRDeadPixelTest.log
```

That log records which OpenXR or graphics step failed.
