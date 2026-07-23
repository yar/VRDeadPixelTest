import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";

async function render() {
  const workerUrl = new URL("../dist/server/index.js", import.meta.url);
  workerUrl.searchParams.set("test", `${process.pid}-${Date.now()}`);
  const { default: worker } = await import(workerUrl.href);

  return worker.fetch(
    new Request("http://localhost/", {
      headers: { accept: "text/html" },
    }),
    {
      ASSETS: {
        fetch: async () => new Response("Not found", { status: 404 }),
      },
    },
    {
      waitUntil() {},
      passThroughOnException() {},
    },
  );
}

test("server-renders the Pixel Flow display test", async () => {
  const response = await render();
  assert.equal(response.status, 200);
  assert.match(response.headers.get("content-type") ?? "", /^text\/html\b/i);

  const html = await response.text();
  assert.match(html, /<title>Pixel Flow — Display Inspection<\/title>/i);
  assert.match(html, /Pixel Flow/);
  assert.match(html, /Display inspection/);
  assert.match(html, /Balanced grey/);
  assert.match(html, /Next color/);
  assert.match(html, /Fullscreen/);
  assert.doesNotMatch(html, /codex-preview/i);
});

test("keeps requested keyboard controls and a disposable interface", async () => {
  const [component, css, page, layout, packageJson] = await Promise.all([
    readFile(new URL("../app/pixel-flow.tsx", import.meta.url), "utf8"),
    readFile(new URL("../app/globals.css", import.meta.url), "utf8"),
    readFile(new URL("../app/page.tsx", import.meta.url), "utf8"),
    readFile(new URL("../app/layout.tsx", import.meta.url), "utf8"),
    readFile(new URL("../package.json", import.meta.url), "utf8"),
  ]);

  assert.match(component, /event\.key === " "/);
  assert.match(component, /event\.key === "ArrowRight"/);
  assert.match(component, /event\.key === "ArrowLeft"/);
  assert.match(component, /event\.key === "ArrowUp"/);
  assert.match(component, /event\.key === "ArrowDown"/);
  assert.match(component, /const BRIGHTNESS_MIN = 50/);
  assert.match(component, /const BRIGHTNESS_MAX = 150/);
  assert.match(component, /useState\(100\)/);
  assert.match(component, /event\.key === "Escape"/);
  assert.match(component, /requestAnimationFrame/);
  assert.match(component, /requestFullscreen/);
  assert.match(component, /applyInBandVariation/);
  assert.match(component, /tileableValueNoise/);
  assert.match(css, /interface-hidden/);
  assert.match(css, /brightness\(var\(--pattern-brightness/);
  assert.match(css, /prefers-reduced-motion:\s*reduce/);
  assert.match(page, /<PixelFlow \/>/);
  assert.match(layout, /Pixel Flow — Display Inspection/);
  assert.match(layout, /new URL\("\/og\.png", base\)/);
  assert.doesNotMatch(packageJson, /react-loading-skeleton/);
});
