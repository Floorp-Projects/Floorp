// SPDX-License-Identifier: MPL-2.0

export type NodeBuildMode = "dev" | "test" | "stage" | "production";

export interface NodeBuildCommand {
  readonly name: string;
  readonly command: string;
  readonly args: readonly string[];
  readonly cwd: string;
}

export interface NodeBuildPlanOptions {
  readonly mode: string;
  readonly buildid2: string;
  readonly version: string;
  readonly projectRoot: string;
  readonly nodeExecutable: string;
  readonly tsdownExecutable: string;
  readonly packagedQa?: boolean;
}

export interface NodeBuildPlan {
  readonly mode: NodeBuildMode;
  readonly commands: readonly NodeBuildCommand[];
  readonly usesPackagedAssets: boolean;
  readonly packagedQa: boolean;
}

export interface NodeBuildCliOptions {
  mode: string;
  buildid2?: string;
  version?: string;
  help: boolean;
}
