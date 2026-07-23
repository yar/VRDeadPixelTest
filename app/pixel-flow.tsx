"use client";

import {
  type CSSProperties,
  useCallback,
  useEffect,
  useRef,
  useState,
} from "react";

type Palette = {
  name: string;
  note: string;
  base: string;
  bands: string[];
  contour: string;
  fleck: string;
  ink: "light" | "dark";
};

const palettes: Palette[] = [
  {
    name: "Balanced grey",
    note: "Mid neutral",
    base: "#777b78",
    bands: ["#818581", "#707572", "#898c87", "#6e7371", "#7f837f"],
    contour: "rgba(231, 235, 229, 0.13)",
    fleck: "rgba(239, 242, 235, 0.10)",
    ink: "light",
  },
  {
    name: "Cool grey",
    note: "Blue-neutral",
    base: "#737b82",
    bands: ["#7d858c", "#69737b", "#858c91", "#6c757c", "#7b8389"],
    contour: "rgba(226, 234, 238, 0.14)",
    fleck: "rgba(237, 242, 244, 0.10)",
    ink: "light",
  },
  {
    name: "Warm sand",
    note: "Muted beige",
    base: "#91806b",
    bands: ["#9c8a74", "#857461", "#a08e78", "#82715f", "#978570"],
    contour: "rgba(246, 234, 215, 0.14)",
    fleck: "rgba(249, 238, 221, 0.10)",
    ink: "light",
  },
  {
    name: "Mist blue",
    note: "Muted cool",
    base: "#657f8b",
    bands: ["#708b96", "#5b7480", "#78929c", "#58717c", "#6c8690"],
    contour: "rgba(224, 239, 243, 0.15)",
    fleck: "rgba(233, 245, 247, 0.10)",
    ink: "light",
  },
  {
    name: "Soft sage",
    note: "Muted green",
    base: "#708173",
    bands: ["#7b8c7d", "#657668", "#829184", "#627466", "#76877a"],
    contour: "rgba(231, 241, 229, 0.14)",
    fleck: "rgba(238, 245, 234, 0.10)",
    ink: "light",
  },
  {
    name: "Dusty rose",
    note: "Muted warm",
    base: "#8d6f72",
    bands: ["#99797c", "#816568", "#9e8081", "#7e6265", "#947477"],
    contour: "rgba(246, 230, 229, 0.14)",
    fleck: "rgba(249, 236, 233, 0.10)",
    ink: "light",
  },
  {
    name: "Mid red",
    note: "Red subpixel",
    base: "#9a4c4d",
    bands: ["#a95756", "#8b4345", "#ae5b59", "#873f42", "#a05251"],
    contour: "rgba(255, 223, 214, 0.14)",
    fleck: "rgba(255, 230, 220, 0.10)",
    ink: "light",
  },
  {
    name: "Mid green",
    note: "Green subpixel",
    base: "#4f8256",
    bands: ["#5a8e62", "#46764d", "#609467", "#437349", "#55895c"],
    contour: "rgba(222, 247, 220, 0.14)",
    fleck: "rgba(231, 250, 227, 0.10)",
    ink: "light",
  },
  {
    name: "Mid blue",
    note: "Blue subpixel",
    base: "#456b99",
    bands: ["#5278a5", "#3d608c", "#587eaa", "#3a5d88", "#4b719f"],
    contour: "rgba(220, 234, 255, 0.15)",
    fleck: "rgba(229, 240, 255, 0.10)",
    ink: "light",
  },
  {
    name: "Deep graphite",
    note: "Dark neutral",
    base: "#292d2e",
    bands: ["#323738", "#242829", "#383c3c", "#222627", "#2e3334"],
    contour: "rgba(216, 226, 224, 0.10)",
    fleck: "rgba(226, 234, 232, 0.08)",
    ink: "light",
  },
  {
    name: "Deep ocean",
    note: "Dark cool",
    base: "#263943",
    bands: ["#304650", "#203139", "#354b54", "#1e2f37", "#2b4049"],
    contour: "rgba(211, 234, 240, 0.11)",
    fleck: "rgba(222, 240, 244, 0.08)",
    ink: "light",
  },
  {
    name: "Bright ivory",
    note: "Bright warm",
    base: "#d5d1c5",
    bands: ["#dedacf", "#c8c4b9", "#e2ded3", "#c5c1b6", "#d0ccc0"],
    contour: "rgba(67, 65, 59, 0.11)",
    fleck: "rgba(62, 60, 55, 0.08)",
    ink: "dark",
  },
  {
    name: "Bright cloud",
    note: "Bright cool",
    base: "#cbd1d3",
    bands: ["#d6dcdd", "#bec5c8", "#dbe0e1", "#bbc2c5", "#c6cdcf"],
    contour: "rgba(51, 61, 65, 0.11)",
    fleck: "rgba(48, 58, 62, 0.08)",
    ink: "dark",
  },
  {
    name: "Near black",
    note: "Stuck-pixel check",
    base: "#0d0f10",
    bands: ["#151819", "#090b0c", "#191c1d", "#080a0b", "#111415"],
    contour: "rgba(206, 218, 220, 0.08)",
    fleck: "rgba(221, 230, 232, 0.06)",
    ink: "light",
  },
];

const speedOptions = [18, 28, 42];
const TAU = Math.PI * 2;

function waveY(
  x: number,
  width: number,
  base: number,
  amplitude: number,
  layer: number,
) {
  const phase = layer * 0.91;
  return (
    base +
    Math.sin((x / width) * TAU * 2 + phase) * amplitude +
    Math.sin((x / width) * TAU * 3 - phase * 0.63) * amplitude * 0.34 +
    Math.sin((x / width) * TAU * 5 + phase * 1.31) * amplitude * 0.12
  );
}

function drawWave(
  context: CanvasRenderingContext2D,
  width: number,
  height: number,
  base: number,
  amplitude: number,
  layer: number,
  fill: string,
  stroke: string,
) {
  context.beginPath();
  context.moveTo(0, height);
  context.lineTo(0, waveY(0, width, base, amplitude, layer));
  for (let x = 12; x <= width; x += 12) {
    context.lineTo(x, waveY(x, width, base, amplitude, layer));
  }
  context.lineTo(width, height);
  context.closePath();
  context.fillStyle = fill;
  context.fill();

  context.beginPath();
  context.moveTo(0, waveY(0, width, base, amplitude, layer));
  for (let x = 12; x <= width; x += 12) {
    context.lineTo(x, waveY(x, width, base, amplitude, layer));
  }
  context.strokeStyle = stroke;
  context.lineWidth = 1;
  context.stroke();
}

function createPatternTile(
  width: number,
  height: number,
  dpr: number,
  palette: Palette,
) {
  const tile = document.createElement("canvas");
  tile.width = Math.round(width * dpr);
  tile.height = Math.round(height * dpr);
  const context = tile.getContext("2d");
  if (!context) return tile;

  context.setTransform(dpr, 0, 0, dpr, 0, 0);
  context.fillStyle = palette.base;
  context.fillRect(0, 0, width, height);

  const bands = [
    { base: 0.22, amp: 0.055 },
    { base: 0.35, amp: 0.075 },
    { base: 0.49, amp: 0.06 },
    { base: 0.64, amp: 0.085 },
    { base: 0.8, amp: 0.055 },
  ];

  bands.forEach((band, index) => {
    drawWave(
      context,
      width,
      height,
      height * band.base,
      Math.min(70, height * band.amp),
      index + 1,
      palette.bands[index],
      palette.contour,
    );
  });

  // A few low-contrast, soft details give the eye landmarks without visual noise.
  const flecks = [
    [0.08, 0.17, 34, 9, -0.08],
    [0.19, 0.72, 22, 6, 0.12],
    [0.31, 0.39, 48, 11, -0.05],
    [0.46, 0.88, 31, 8, 0.09],
    [0.58, 0.14, 19, 5, 0.04],
    [0.68, 0.57, 43, 10, -0.11],
    [0.82, 0.31, 27, 7, 0.07],
    [0.94, 0.76, 38, 9, -0.04],
  ] as const;

  context.fillStyle = palette.fleck;
  for (const [xRatio, yRatio, radiusX, radiusY, rotation] of flecks) {
    const x = xRatio * width;
    const y = yRatio * height;
    for (const offset of [-width, 0, width]) {
      context.beginPath();
      context.ellipse(x + offset, y, radiusX, radiusY, rotation, 0, TAU);
      context.fill();
    }
  }

  return tile;
}

export function PixelFlow() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const currentTileRef = useRef<HTMLCanvasElement | null>(null);
  const previousTileRef = useRef<HTMLCanvasElement | null>(null);
  const transitionStartRef = useRef(0);
  const offsetRef = useRef(0);
  const lastFrameRef = useRef(0);
  const pausedRef = useRef(false);
  const speedRef = useRef(speedOptions[1]);
  const hideTimerRef = useRef<number | null>(null);

  const [paletteIndex, setPaletteIndex] = useState(0);
  const [speedIndex, setSpeedIndex] = useState(1);
  const [paused, setPaused] = useState(false);
  const [interfaceVisible, setInterfaceVisible] = useState(true);
  const [isFullscreen, setIsFullscreen] = useState(false);
  const [sessionEnded, setSessionEnded] = useState(false);

  const palette = palettes[paletteIndex];

  const revealInterface = useCallback((duration = 2400) => {
    setInterfaceVisible(true);
    if (hideTimerRef.current !== null) {
      window.clearTimeout(hideTimerRef.current);
    }
    hideTimerRef.current = window.setTimeout(() => {
      setInterfaceVisible(false);
    }, duration);
  }, []);

  const movePalette = useCallback(
    (direction: 1 | -1) => {
      setPaletteIndex(
        (current) => (current + direction + palettes.length) % palettes.length,
      );
      revealInterface(1800);
    },
    [revealInterface],
  );

  const toggleFullscreen = useCallback(async () => {
    try {
      if (document.fullscreenElement) {
        await document.exitFullscreen();
      } else {
        await document.documentElement.requestFullscreen();
      }
    } catch {
      // Fullscreen availability is controlled by the browser and embedding host.
    }
    revealInterface();
  }, [revealInterface]);

  const endSession = useCallback(() => {
    setSessionEnded(true);
    setPaused(true);
    if (document.fullscreenElement) {
      void document.exitFullscreen();
    }
  }, []);

  const resumeSession = useCallback(async () => {
    setSessionEnded(false);
    setPaused(false);
    revealInterface(3200);
    try {
      await document.documentElement.requestFullscreen();
    } catch {
      // The test still fills the browser viewport when fullscreen is unavailable.
    }
  }, [revealInterface]);

  useEffect(() => {
    pausedRef.current = paused;
  }, [paused]);

  useEffect(() => {
    speedRef.current = speedOptions[speedIndex];
  }, [speedIndex]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const rebuild = (animateChange: boolean) => {
      const dpr = Math.min(window.devicePixelRatio || 1, 2);
      const width = window.innerWidth;
      const height = window.innerHeight;
      const tileWidth = Math.max(1080, Math.min(1500, width * 0.88));

      canvas.width = Math.round(width * dpr);
      canvas.height = Math.round(height * dpr);
      const nextTile = createPatternTile(tileWidth, height, dpr, palette);

      if (animateChange && currentTileRef.current) {
        previousTileRef.current = currentTileRef.current;
        transitionStartRef.current = performance.now();
      } else {
        previousTileRef.current = null;
      }
      currentTileRef.current = nextTile;
    };

    rebuild(true);
    const handleResize = () => rebuild(false);
    window.addEventListener("resize", handleResize);
    return () => window.removeEventListener("resize", handleResize);
  }, [palette]);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const context = canvas.getContext("2d", { alpha: false });
    if (!context) return;
    let animationFrame = 0;

    const drawTiled = (
      tile: HTMLCanvasElement,
      offset: number,
      opacity: number,
    ) => {
      context.globalAlpha = opacity;
      let x = (offset % tile.width) - tile.width;
      while (x < canvas.width) {
        context.drawImage(tile, x, 0);
        x += tile.width;
      }
    };

    const animate = (time: number) => {
      const elapsed = lastFrameRef.current
        ? Math.min(50, time - lastFrameRef.current)
        : 0;
      lastFrameRef.current = time;

      if (!pausedRef.current) {
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        offsetRef.current += (speedRef.current * elapsed * dpr) / 1000;
      }

      const current = currentTileRef.current;
      const previous = previousTileRef.current;
      if (current) {
        context.globalAlpha = 1;
        context.fillStyle = palette.base;
        context.fillRect(0, 0, canvas.width, canvas.height);

        const transition = Math.min(
          1,
          Math.max(0, (time - transitionStartRef.current) / 360),
        );
        if (previous && transition < 1) {
          drawTiled(previous, offsetRef.current, 1);
          drawTiled(current, offsetRef.current, transition);
        } else {
          previousTileRef.current = null;
          drawTiled(current, offsetRef.current, 1);
        }
        context.globalAlpha = 1;
      }

      animationFrame = window.requestAnimationFrame(animate);
    };

    animationFrame = window.requestAnimationFrame(animate);
    return () => window.cancelAnimationFrame(animationFrame);
  }, [palette.base]);

  useEffect(() => {
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.repeat) return;

      if (event.key === " " || event.key === "ArrowRight") {
        event.preventDefault();
        movePalette(1);
      } else if (event.key === "ArrowLeft") {
        event.preventDefault();
        movePalette(-1);
      } else if (event.key === "Escape") {
        event.preventDefault();
        endSession();
      } else if (event.key.toLowerCase() === "p") {
        setPaused((current) => !current);
        revealInterface();
      } else if (event.key.toLowerCase() === "f") {
        void toggleFullscreen();
      }
    };

    const handleFullscreenChange = () => {
      setIsFullscreen(Boolean(document.fullscreenElement));
      revealInterface();
    };

    const handlePointerMove = () => revealInterface(1800);
    window.addEventListener("keydown", handleKeyDown);
    window.addEventListener("pointermove", handlePointerMove, { passive: true });
    document.addEventListener("fullscreenchange", handleFullscreenChange);
    revealInterface(6200);

    return () => {
      window.removeEventListener("keydown", handleKeyDown);
      window.removeEventListener("pointermove", handlePointerMove);
      document.removeEventListener("fullscreenchange", handleFullscreenChange);
      if (hideTimerRef.current !== null) {
        window.clearTimeout(hideTimerRef.current);
      }
    };
  }, [endSession, movePalette, revealInterface, toggleFullscreen]);

  return (
    <main
      className={`pixel-flow ${palette.ink} ${
        interfaceVisible ? "interface-visible" : "interface-hidden"
      }`}
      style={{ "--palette-base": palette.base } as CSSProperties}
      onPointerDown={(event) => {
        if (event.target === event.currentTarget) movePalette(1);
      }}
    >
      <canvas ref={canvasRef} className="pattern-canvas" aria-hidden="true" />

      <div className="vignette" aria-hidden="true" />

      <header className="test-header">
        <div className="brand-lockup">
          <span className="pixel-mark" aria-hidden="true">
            <i />
            <i />
            <i />
            <i />
          </span>
          <span>
            <strong>Pixel Flow</strong>
            <small>Display inspection</small>
          </span>
        </div>

        <div className="palette-readout" aria-live="polite">
          <span
            className="palette-swatch"
            style={{ backgroundColor: palette.base }}
            aria-hidden="true"
          />
          <span className="palette-copy">
            <strong>{palette.name}</strong>
            <small>{palette.note}</small>
          </span>
          <span className="palette-count">
            {String(paletteIndex + 1).padStart(2, "0")}
            <i />
            {String(palettes.length).padStart(2, "0")}
          </span>
        </div>
      </header>

      <p className="inspection-note">
        Relax your gaze. Let stationary defects separate from the flow.
      </p>

      <nav className="control-dock" aria-label="Test controls">
        <button
          className="step-button"
          type="button"
          onClick={() => movePalette(-1)}
          aria-label="Previous color"
        >
          <span aria-hidden="true">←</span>
        </button>

        <div className="key-guide" aria-hidden="true">
          <span>
            <kbd>Space</kbd>
            <kbd>→</kbd>
            Next color
          </span>
          <span className="divider" />
          <span>
            <kbd>←</kbd>
            Previous
          </span>
          <span className="divider wide-only" />
          <span className="wide-only">
            <kbd>Esc</kbd>
            Exit
          </span>
        </div>

        <button
          className="text-control"
          type="button"
          onClick={() => {
            setSpeedIndex((current) => (current + 1) % speedOptions.length);
            revealInterface();
          }}
          aria-label={`Change drift speed. Current speed ${speedOptions[speedIndex]} pixels per second`}
        >
          Drift&nbsp; {speedOptions[speedIndex]}
        </button>

        <button
          className="text-control"
          type="button"
          onClick={() => {
            setPaused((current) => !current);
            revealInterface();
          }}
        >
          {paused ? "Resume" : "Pause"}
        </button>

        <button
          className="text-control fullscreen-control"
          type="button"
          onClick={() => void toggleFullscreen()}
        >
          {isFullscreen ? "Window" : "Fullscreen"}
        </button>

        <button
          className="step-button"
          type="button"
          onClick={() => movePalette(1)}
          aria-label="Next color"
        >
          <span aria-hidden="true">→</span>
        </button>
      </nav>

      <div className="edge-hint" aria-hidden="true">
        Move the pointer to reveal controls
      </div>

      {sessionEnded && (
        <section className="session-ended" role="dialog" aria-modal="true">
          <div className="end-card">
            <span className="end-mark" aria-hidden="true">
              <i />
              <i />
              <i />
              <i />
            </span>
            <p className="eyebrow">Inspection paused</p>
            <h1>Test session ended</h1>
            <p>
              This browser prototype has released fullscreen and stopped the
              pattern.
            </p>
            <button type="button" onClick={() => void resumeSession()}>
              Resume test
              <span aria-hidden="true">→</span>
            </button>
          </div>
        </section>
      )}
    </main>
  );
}
