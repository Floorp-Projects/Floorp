// tools/utils/cli-args.ts
/**
 * CLI argument parsing, validation, and mapping for Noraneko Build System.
 */

import { parseArgs } from "@std/cli/parse-args";
import type { BuildOptions, ProductionMozbuildPhase } from "./types.ts";
import type { BuildMode } from "./types.ts";

/**
 * Raw CLI arguments structure.
 */
export interface RawCLIArgs {
  mode?: string;
  "init-git-for-patch"?: boolean;
  "production-mozbuild"?: string;
  help?: boolean;
  h?: boolean;
  [key: string]: unknown;
}

/**
 * Parse CLI arguments into a structured object.
 */
export function parseCLIArgs(args: string[]): RawCLIArgs {
  return parseArgs(args) as RawCLIArgs;
}

/**
 * Validate and normalize CLI arguments.
 * Throws on invalid combinations.
 */
export function validateCLIArgs(args: RawCLIArgs): void {
  if (args.help || args.h) {
    // Help is handled by CLI.showHelp
    return;
  }
  if (args["production-mozbuild"] && args.mode && args.mode !== "production") {
    throw new Error(
      "`--production-mozbuild` is not compatible with `--mode=`.",
    );
  }
}

/**
 * Map CLI arguments to BuildOptions.
 */
export function mapCLIArgsToBuildOptions(args: RawCLIArgs): BuildOptions {
  // Only allow valid BuildMode values
  const validModes: BuildMode[] = ["dev", "test", "stage", "production"];
  let mode: BuildMode = validModes.includes(args.mode as BuildMode)
    ? (args.mode as BuildMode)
    : "dev";
  let productionMozbuildPhase: ProductionMozbuildPhase | undefined;

  if (args["production-mozbuild"] === "before") {
    mode = "production";
    productionMozbuildPhase = "before";
  } else if (args["production-mozbuild"] === "after") {
    mode = "production";
    productionMozbuildPhase = "after";
  }

  const launchBrowser = mode === "dev" || mode === "test" || mode === "stage";

  return {
    mode,
    initGitForPatch: !!args["init-git-for-patch"],
    productionMozbuildPhase,
    launchBrowser,
  };
}
