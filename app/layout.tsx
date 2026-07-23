import type { Metadata, Viewport } from "next";
import { headers } from "next/headers";
import "./globals.css";

export async function generateMetadata(): Promise<Metadata> {
  const requestHeaders = await headers();
  const host =
    requestHeaders.get("x-forwarded-host") ??
    requestHeaders.get("host") ??
    "localhost:3000";
  const protocol =
    requestHeaders.get("x-forwarded-proto")?.split(",")[0] ??
    (host.startsWith("localhost") ? "http" : "https");
  const base = new URL(`${protocol}://${host}`);
  const description =
    "A calm moving-field test for spotting stationary pixel defects on displays and headsets.";

  return {
    metadataBase: base,
    title: "Pixel Flow — Display Inspection",
    description,
    openGraph: {
      title: "Pixel Flow",
      description,
      type: "website",
      images: [
        {
          url: new URL("/og.png", base).toString(),
          width: 1672,
          height: 941,
          alt: "Pixel Flow display inspection",
        },
      ],
    },
    twitter: {
      card: "summary_large_image",
      title: "Pixel Flow",
      description,
      images: [new URL("/og.png", base).toString()],
    },
  };
}

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  themeColor: "#777b78",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
