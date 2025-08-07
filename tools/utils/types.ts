/**
 * Core build system types
 */

export type Platform = "windows" | "linux" | "darwin";
export type Architecture = "x86_64" | "aarch64";
export type BuildMode = "dev" | "test" | "stage" | "production";
export type ProductionMozbuildPhase = "before" | "after";

export interface Branding {
  baseName: string;
  displayName: string;
  devSuffix: string;
}

export interface BinaryArchive {
  filename: string;
  format: "zip" | "tar.xz" | "dmg";
  platform: Platform;
  architecture: Architecture | "universal";
}

export interface BuildOptions {
  mode: BuildMode;
  productionMozbuildPhase: ProductionMozbuildPhase;
  clean?: boolean;
  initGitForPatch?: boolean;
  launchBrowser?: boolean;
}

export interface PreBuildOptions {
  initGitForPatch?: boolean;
  isDev?: boolean;
  mode?: BuildMode;
}

export interface Paths {
  root: string;
  binRoot: string;
  buildid2: string;
  profileTest: string;
  loaderFeatures: string;
  featuresChrome: string;
  i18nFeaturesChrome: string;
  loaderModules: string;
  modules: string;
  actualModules: string;
  mozbuildOutput: string;
}
