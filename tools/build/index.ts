/**
 * Main build orchestrator for Noraneko
 * Coordinates pre-build, Mozilla build, and post-build phases
 */

import { runPreBuildPhase } from "./phases/pre-build.ts";
import { runPostBuildPhase } from "./phases/post-build.ts";
import { log } from "./logger.ts";

type BuildMode = "dev" | "production";
type BuildPhase = "full" | "before" | "after";

interface BuildOptions {
  mode: BuildMode;
  phase?: BuildPhase;
  _clean?: boolean;
  initGitForPatch?: boolean; // パッチ作成用のGit初期化フラグ
}

/**
 * Main build function
 */
export async function build(options: BuildOptions): Promise<void> {
  const { mode, phase = "full", _clean = false, initGitForPatch = false } =
    options;

  log.info(`🏗️  Starting Noraneko build (${mode} mode, ${phase} phase)`);

  try {
    // Before phase: noranekoスクリプト実行
    if (phase === "before" || phase === "full") {
      await runPreBuildPhase({ initGitForPatch, isDev: mode === "dev" });
      log.info("✅ Before phase completed (noranekoスクリプト実行)");

      if (phase === "before") {
        log.info(
          "🔄 Ready for Mozilla build. Run 'deno run -A tools/build.ts --after' after mozbuild",
        );
        return;
      }
    }

    // Mozilla build phase (external)
    if (phase === "full") {
      log.info("🦊 Mozilla build should be executed externally");
      log.info(
        "💡 Use --before and --after options to split the build process",
      );
    }

    // After phase: Inject処理
    if (phase === "after" || phase === "full") {
      await runPostBuildPhase(mode === "dev");
      log.info("✅ After phase completed (Inject処理)");
    }

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
  const _clean = args.includes("--clean");

  let phase: BuildPhase = "full";
  if (args.includes("--before")) {
    phase = "before";
  } else if (args.includes("--after")) {
    phase = "after";
  }

  await build({ mode, phase, _clean });
}
