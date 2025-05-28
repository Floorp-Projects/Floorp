import pathe from "pathe";

//? branding
export const brandingBaseName = "noraneko";
export const brandingName = "Noraneko";

//? when the linux binary has published, I'll sync linux bin version
export const VERSION = Deno.build.os === "windows" ? "001" : "000";
export const binRootDir = "_dist/bin";
export const binDir = Deno.build.os !== "darwin"
  ? `_dist/bin/${brandingBaseName}`
  : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;

export const binPath = pathe.join(binDir, brandingBaseName);
export const binPathExe = Deno.build.os !== "darwin"
  ? binPath + (Deno.build.os === "windows" ? ".exe" : "")
  : `./_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/MacOS/${brandingBaseName}`;

export const binVersion = pathe.join(binDir, "nora.version.txt");

//? Build-related paths
export const buildid2Path = "_dist/buildid2";
export const profileTestPath = "./_dist/profile/test";
export const loaderFeaturesPath = "apps/system/loader-features";
export const featuresChromePath = "apps/features-chrome";
export const i18nFeaturesChromePath = "i18n/features-chrome";
export const loaderModulesPath = "apps/system/loader-modules"; // Added this line
export const modulesPath = "apps/modules";
export const devServerReadyString =
  "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev";
export const devBrandingSuffix = "noraneko-dev";
export const mozbuildOutputDir = "../obj-artifact-build-output/dist";

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
