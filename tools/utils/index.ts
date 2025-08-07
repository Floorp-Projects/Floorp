import { runPreBuildPhase } from "../misc/phases/pre-build.ts";
import { runPostBuildPhase } from "../misc/phases/post-build.ts";
import { log } from "./logger.ts";
import { initializeDistDirectory } from "./utils.ts";
import type { BuildOptions } from "./types.ts";

export async function build(options: BuildOptions): Promise<void> {
  const { mode, productionMozbuildPhase, initGitForPatch = false } = options;

  // Determine phase for logging
  let phaseLabel: string;
  if (productionMozbuildPhase === "before") phaseLabel = "before";
  else if (productionMozbuildPhase === "after") phaseLabel = "after";
  else phaseLabel = "full";

  log.info(`🏗️ Starting ${mode} build (${phaseLabel} phase)`);

  try {
    await initializeDistDirectory();

    // "before" or "full" phase: run pre-build
    if (productionMozbuildPhase !== "after") {
      await runPreBuildPhase({
        initGitForPatch,
        isDev: (mode === "dev" || mode === "test"),
        mode,
      });

      if (productionMozbuildPhase === "before") {
        log.info(
          "🔄 Ready for Mozilla build. Run with --after flag after mozbuild",
        );
        return;
      }
    }

    // "full" phase: show split build info
    if (!productionMozbuildPhase) {
      log.info("🦊 For split builds, use --before and --after options");
    }

    // "after" or "full" phase: run post-build
    if (productionMozbuildPhase === "after" || !productionMozbuildPhase) {
      await runPostBuildPhase(mode === "dev" || mode === "test");
    }

    log.success("🎉 Build completed successfully!");
  } catch (error) {
    log.error(`💥 Build failed: ${error}`);
    throw error;
  }
}
