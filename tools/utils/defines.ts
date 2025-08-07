import { join } from "@std/path";
import { arch, platform, projectRoot } from "./utils.ts";
import type { BinaryArchive, Branding, Paths } from "./types.ts";

/**
 * Core configuration
 */
export const branding: Branding = {
  baseName: "noraneko",
  displayName: "Noraneko",
  devSuffix: "noraneko-dev",
};

// Legacy exports for backward compatibility
export const brandingBaseName = branding.baseName;
export const brandingName = branding.displayName;

export const VERSION = (platform === "windows" || platform === "linux")
  ? "001"
  : "000";

/**
 * Build paths
 */
export const paths: Paths = {
  root: projectRoot,
  binRoot: join(projectRoot, "_dist", "bin"),
  buildid2: join(projectRoot, "_dist", "buildid2"),
  profileTest: join(projectRoot, "_dist", "profile", "test"),
  loaderFeatures: join(projectRoot, "src", "core", "glue", "loader-features"),
  featuresChrome: join(projectRoot, "chrome", "browser"),
  i18nFeaturesChrome: join(projectRoot, "i18n", "features-chrome"),
  loaderModules: join(projectRoot, "src", "core", "glue", "loader-modules"),
  modules: join(projectRoot, "modules"),
  actualModules: join(projectRoot, "src", "core", "modules"),
  mozbuildOutput: join(projectRoot, "obj-artifact-build-output", "dist"),
};

/**
 * Platform-specific binary paths
 */
export const binDir = platform !== "darwin"
  ? join(paths.binRoot, branding.baseName)
  : join(
    paths.binRoot,
    branding.baseName,
    `${branding.displayName}.app`,
    "Contents",
    "Resources",
  );

export const binRootDir = paths.binRoot;
export const binPath = join(binDir, branding.baseName);

export const binPathExe = platform !== "darwin"
  ? binPath + (platform === "windows" ? ".exe" : "-bin")
  : join(
    paths.binRoot,
    branding.baseName,
    `${branding.displayName}.app`,
    "Contents",
    "MacOS",
    branding.baseName,
  );

export const binVersion = join(binDir, "nora.version.txt");

/**
 * Development server configuration
 */
export const devServer = {
  readyString: "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev",
  defaultPort: 8080,
};

/**
 * Get binary archive information for current platform
 */
export function getBinArchive(): BinaryArchive {
  switch (platform) {
    case "windows":
      if (arch === "aarch64") {
        throw new Error("Windows ARM64 is not yet supported");
      }
      return {
        filename: `${branding.baseName}-win-amd64-moz-artifact.zip`,
        format: "zip",
        platform,
        architecture: "x86_64",
      };

    case "linux":
      const linuxArch = arch === "x86_64" ? "x86_64" : "arm64";
      return {
        filename: `${branding.baseName}-linux-${linuxArch}-moz-artifact.tar.xz`,
        format: "tar.xz",
        platform,
        architecture: arch,
      };

    case "darwin":
      return {
        filename: `${branding.baseName}-macOS-universal-moz-artifact.dmg`,
        format: "dmg",
        platform,
        architecture: "universal",
      };

    default:
      throw new Error(
        `Unsupported platform: ${platform}. ` +
          "Supported platforms are: windows (x86_64), linux (x86_64, aarch64), darwin (universal)",
      );
  }
}

/**
 * Check if current platform/architecture is supported
 */
export function isPlatformSupported(): boolean {
  try {
    getBinArchive();
    return true;
  } catch {
    return false;
  }
}
