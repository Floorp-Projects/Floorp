/**
 * Noraneko Build System - Main Entry Point (CLI Refactored)
 */

import { CLI } from "./cli.ts";
import { build } from "./utils/index.ts";
import { BuildOptions } from "./utils/types.ts";
import { log } from "./utils/logger.ts";

if (import.meta.main) {
  const options = CLI.parseArgs(Deno.args);

  log.info("Parsed CLI options:", options);

  // Map CLI options to BuildOptions for orchestration
  const buildOptions: BuildOptions = {
    mode: options.mode,
    productionMozbuildPhase: options.productionMozbuildPhase,
    initGitForPatch: !!options.initGitForPatch,
    launchBrowser: !!options.launchBrowser,
  };

  await build(buildOptions);
}
