// SPDX-License-Identifier: MPL-2.0
export interface ViteBuildConfig {
  base?: string;
  configFile?: string;
  build: { watch: null };
}
export type ViteBuildArgs = { kind: "help" } | {
  kind: "build";
  config: ViteBuildConfig;
};
export type ViteBuildInvoker = (config: ViteBuildConfig) => Promise<unknown>;
export interface ViteBuildDependencies {
  loadBuild: () => Promise<ViteBuildInvoker>;
  info: (message: string) => void;
  error: (error: unknown) => void;
}
