import pathe from "pathe";

//? branding
export const brandingBaseName = "noraneko";
export const brandingName = "Noraneko";

//? when the linux binary has published, I'll sync linux bin version
export const VERSION = Deno.build.os === "windows" ? "001" : "000";
export const binExtractDir = "_dist/bin";
export const binDir = Deno.build.os !== "darwin"
  ? `_dist/bin/${brandingBaseName}`
  : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;

export const binPath = pathe.join(binDir, brandingBaseName);
export const binPathExe = Deno.build.os !== "darwin"
  ? binPath + (Deno.build.os === "windows" ? ".exe" : "")
  : `./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName}`;

export const binVersion = pathe.join(binDir, "nora.version.txt");

export function getBinArchive() {
  switch (Deno.build.os) {
    case "windows": {
      if (Deno.build.arch === "aarch64") break;
      return `${brandingBaseName}-win-amd64-moz-artifact.zip`;
    }
    case "linux": {
      return `${brandingBaseName}-linux-${
        Deno.build.arch === "x86_64" ? "amd64" : "arm64"
      }-moz-artifact.tar.xz`;
    }
    case "darwin": {
      return `${brandingBaseName}-macOS-universal-moz-artifact.dmg`;
    }
    default:
  }
  throw new Error(
    "Unsupported platform/architecture : " + Deno.build.os + " " +
      Deno.build.arch,
  );
}
