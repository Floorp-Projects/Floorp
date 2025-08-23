import * as path from "@std/path";

/**
 * Branding and platform-related constants used by build tools.
 * Modernized for ESNext + latest TypeScript while preserving Deno APIs.
 */

export const BRANDING = {
  base_name: "noraneko",
  display_name: "Noraneko",
  dev_suffix: "noraneko-dev",
} as const;

export type Platform = "windows" | "darwin" | "linux";

/** Resolve runtime platform (Deno-only) */
const detectPlatform = (): Platform => {
  const raw = Deno.build.os;
  switch (raw) {
    case "windows":
      return "windows";
    case "darwin":
      return "darwin";
    default:
      return "linux";
  }
};

export const PLATFORM: Platform = detectPlatform();

export const VERSION = ["windows", "linux"].includes(PLATFORM) ? "002" : "000";

export const PROJECT_ROOT = path.resolve(
  path.dirname(path.fromFileUrl(import.meta.url)),
  "..",
  "..",
);

export const PATHS = {
  root: PROJECT_ROOT,
  bin_root: path.join(PROJECT_ROOT, "_dist", "bin"),
  buildid2: path.join(PROJECT_ROOT, "_dist", "buildid2"),
  profile_test: path.join(PROJECT_ROOT, "_dist", "profile", "test"),
  loader_features: path.join(PROJECT_ROOT, "bridge/loader-features"),
  features_chrome: path.join(PROJECT_ROOT, "browser-features/chrome"),
  i18n: path.join(PROJECT_ROOT, "i18n"),
  loader_modules: path.join(PROJECT_ROOT, "bridge/loader-modules"),
  modules: path.join(PROJECT_ROOT, "browser-features/modules"),
  mozbuild_output: path.join(PROJECT_ROOT, "obj-artifact-build-output/dist"),
} as const;

export const BIN_DIR =
  PLATFORM !== "darwin"
    ? path.join(PATHS.bin_root, BRANDING.base_name)
    : path.join(
        PATHS.bin_root,
        BRANDING.base_name,
        `${BRANDING.display_name}.app`,
        "Contents",
        "Resources",
      );

export const BIN_ROOT_DIR = PATHS.bin_root;
export const BIN_PATH = path.join(BIN_DIR, BRANDING.base_name);

export const BIN_PATH_EXE =
  PLATFORM !== "darwin"
    ? BIN_PATH + (PLATFORM === "windows" ? ".exe" : "-bin")
    : path.join(
        PATHS.bin_root,
        BRANDING.base_name,
        `${BRANDING.display_name}.app`,
        "Contents",
        "MacOS",
        BRANDING.base_name,
      );

export const BIN_VERSION = path.join(BIN_DIR, "nora.version.txt");

export const DEV_SERVER = {
  ready_string: "nora-{bbd11c51-3be9-4676-b912-ca4c0bdcab94}-dev",
  default_port: 8080,
} as const;

export type BinArchive =
  | {
      filename: string;
      format: "zip";
      platform: "windows";
      architecture: "x86_64";
    }
  | {
      filename: string;
      format: "tar.xz";
      platform: "linux";
      architecture: "x86_64" | "arm64";
    }
  | {
      filename: string;
      format: "dmg";
      platform: "darwin";
      architecture: "universal";
    };

export function getBinArchive(): BinArchive {
  if (PLATFORM === "windows") {
    return {
      filename: `${BRANDING.base_name}-win-amd64-moz-artifact.zip`,
      format: "zip",
      platform: "windows",
      architecture: "x86_64",
    };
  }

  if (PLATFORM === "linux") {
    // Map Deno arch to expected strings
    const denoArch = Deno.build.arch;
    const linuxArch = denoArch === "aarch64" ? "arm64" : "x86_64";
    return {
      filename: `${BRANDING.base_name}-linux-${linuxArch}-moz-artifact.tar.xz`,
      format: "tar.xz",
      platform: "linux",
      architecture: linuxArch as "x86_64" | "arm64",
    };
  }

  if (PLATFORM === "darwin") {
    return {
      filename: `${BRANDING.base_name}-macOS-universal-moz-artifact.dmg`,
      format: "dmg",
      platform: "darwin",
      architecture: "universal",
    };
  }

  throw new Error(
    `Unsupported platform: ${PLATFORM}. Supported: windows (x86_64), linux (x86_64, aarch64), darwin (universal)`,
  );
}

export function platformSupported(): boolean {
  try {
    return !!getBinArchive();
  } catch {
    return false;
  }
}
