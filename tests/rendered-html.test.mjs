import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

test("builds a static Pixel Flow page for the repository subpath", async () => {
  const html = await readFile(
    new URL("../dist/index.html", import.meta.url),
    "utf8",
  );
  assert.match(html, /<title>Pixel Flow — Display Inspection<\/title>/i);
  assert.match(html, /<div id="root"><\/div>/);
  assert.match(html, /\/VRDeadPixelTest\/assets\//);
  assert.match(html, /\/VRDeadPixelTest\/favicon\.svg/);
  assert.doesNotMatch(html, /yaroslavdm/i);
});

test("keeps requested keyboard controls and a disposable interface", async () => {
  const [component, css, main, packageJson, workflow] = await Promise.all([
    readFile(new URL("../app/pixel-flow.tsx", import.meta.url), "utf8"),
    readFile(new URL("../app/globals.css", import.meta.url), "utf8"),
    readFile(new URL("../app/main.tsx", import.meta.url), "utf8"),
    readFile(new URL("../package.json", import.meta.url), "utf8"),
    readFile(
      new URL("../.github/workflows/pages.yml", import.meta.url),
      "utf8",
    ),
  ]);

  assert.match(component, /event\.key === " "/);
  assert.match(component, /event\.key === "ArrowRight"/);
  assert.match(component, /event\.key === "ArrowLeft"/);
  assert.match(component, /event\.key === "ArrowUp"/);
  assert.match(component, /event\.key === "ArrowDown"/);
  assert.match(component, /const BRIGHTNESS_MIN = 50/);
  assert.match(component, /const BRIGHTNESS_MAX = 200/);
  assert.match(component, /const BRIGHTNESS_REFERENCE_SCALE = 1\.3/);
  assert.match(component, /useState\(100\)/);
  assert.match(component, /event\.repeat && !isBrightnessKey/);
  assert.match(component, /event\.key === "Escape"/);
  assert.match(component, /requestAnimationFrame/);
  assert.match(component, /requestFullscreen/);
  assert.match(component, /applyInBandVariation/);
  assert.match(component, /tileableValueNoise/);
  assert.match(css, /interface-hidden/);
  assert.match(css, /brightness\(var\(--pattern-brightness/);
  assert.match(css, /prefers-reduced-motion:\s*reduce/);
  assert.match(main, /<PixelFlow \/>/);
  assert.match(main, /createRoot/);
  assert.match(packageJson, /"build:pages"/);
  assert.match(workflow, /npm run build:pages/);
  assert.match(workflow, /actions\/deploy-pages@/);
  assert.doesNotMatch(packageJson, /react-loading-skeleton/);
});
