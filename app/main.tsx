import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./globals.css";
import { PixelFlow } from "./pixel-flow";

const root = document.getElementById("root");

if (!root) {
  throw new Error("Pixel Flow could not find its page root.");
}

createRoot(root).render(
  <StrictMode>
    <PixelFlow />
  </StrictMode>,
);
