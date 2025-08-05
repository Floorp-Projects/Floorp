/**
 * Noraneko Build System - Main Entry Point
 */

import { build } from "./build/index.ts";
import { log } from "./build/logger.ts";
import { ProcessManager } from "./build/process-manager.ts";
import { CLI } from "./build/cli.ts";

export type BuildMode = "dev" | "production" | "test";
export type BuildPhase = "full" | "before" | "after";

export interface BuildOptions {
  mode: BuildMode;
  phase?: BuildPhase;
  launchBrowser?: boolean;
  initGitForPatch?: boolean;
}

const processManager = new ProcessManager();

export async function buildAndLaunch(options: BuildOptions): Promise<void> {
  const {
    mode,
    phase = "full",
    initGitForPatch = false,
    launchBrowser = false,
  } = options;

  try {
    await build({ mode, phase, initGitForPatch });

    if (launchBrowser) {
      await processManager.launchBrowser();
    }
  } catch (error) {
    log.error(`💥 Build failed: ${error}`);
    Deno.exit(1);
  }
}

// CLI interface
if (import.meta.main) {
  const options = CLI.parseArgs(Deno.args);
  await buildAndLaunch(options);
}
