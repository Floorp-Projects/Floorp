/**
 * Main build orchestrator for Noraneko
 * Coordinates pre-build, Mozilla build, and post-build phases
 */

import { runPreBuildPhase } from "./phases/pre-build.ts";
import { runPostBuildPhase } from "./phases/post-build.ts";
import { log } from "./logger.ts";

type BuildMode = "dev" | "production";

interface BuildOptions {
  mode: BuildMode;
  skipMozbuild?: boolean;
  _clean?: boolean;
}

/**
 * Main build function
 */
export async function build(options: BuildOptions): Promise<void> {
  const { mode, skipMozbuild = false, _clean = false } = options;

  log.info(`🏗️  Starting Noraneko build (${mode} mode)`);

  try {
    // Pre-build phase
    await runPreBuildPhase();

    // Mozilla build phase
    if (!skipMozbuild) {
      log.info("🦊 Running Mozilla build...");
      // This would call the actual Mozilla build system
      // Implementation depends on the specific build requirements
    } else {
      log.info("⏭️  Skipping Mozilla build");
    }

    // Post-build phase
    await runPostBuildPhase(mode === "dev");

    log.info("🎉 Build completed successfully!");
  } catch (error) {
    log.error(`💥 Build failed: ${error}`);
    Deno.exit(1);
  }
}

// CLI interface
if (import.meta.main) {
  const args = Deno.args;
  const mode: BuildMode = args.includes("--dev") ? "dev" : "production";
  const skipMozbuild = args.includes("--skip-mozbuild");
  const _clean = args.includes("--clean");

  await build({ mode, skipMozbuild, _clean });
}
