import { runPreBuildPhase } from "./phases/pre-build.ts";
import { runPostBuildPhase } from "./phases/post-build.ts";
import { log } from "./logger.ts";
import { initializeDistDirectory } from "./utils.ts";
import type { BuildOptions } from "./types.ts";

export async function build(options: BuildOptions): Promise<void> {
  const { mode, phase = "full", initGitForPatch = false } = options;

  log.info(`🏗️ Starting ${mode} build (${phase} phase)`);

  try {
    await initializeDistDirectory();

    if (phase === "before" || phase === "full") {
      await runPreBuildPhase({
        initGitForPatch,
        isDev: (mode === "dev" || mode === "test"),
        mode,
      });

      if (phase === "before") {
        log.info(
          "🔄 Ready for Mozilla build. Run with --after flag after mozbuild",
        );
        return;
      }
    }

    if (phase === "full") {
      log.info("🦊 For split builds, use --before and --after options");
    }

    if (phase === "after" || phase === "full") {
      await runPostBuildPhase(mode === "dev" || mode === "test");
    }

    log.success("🎉 Build completed successfully!");
  } catch (error) {
    log.error(`💥 Build failed: ${error}`);
    throw error;
  }
}
