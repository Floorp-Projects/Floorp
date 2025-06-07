/**
 * Noraneko Build and Launch System
 *
 * Simplified build system for development and production workflows.
 */

import { build } from "./build/index.ts";
import { log } from "./build/logger.ts";
import { resolve } from "jsr:@std/path@^1.0.8";
import { ensureDir } from "jsr:@std/fs@^1.0.18";

/**
 * Available build modes
 */
export type BuildMode = "dev" | "production";

/**
 * Build phase options (only needed for production/CI)
 */
export type BuildPhase = "full" | "before" | "after";

/**
 * Build options interface
 */
export interface BuildOptions {
  mode: BuildMode;
  phase?: BuildPhase;
  launchBrowser?: boolean;
  initGitForPatch?: boolean; // パッチ作成用のGit初期化（一般開発者には不要）
}

/**
 * Launch browser using Deno.Command
 */
function launchBrowser(): void {
  log.info("🚀 Launching browser...");

  const command = new Deno.Command(Deno.execPath(), {
    args: ["-A", "./tools/dev/launchDev/child-browser.ts"],
    stdin: "piped",
    stdout: "inherit",
    stderr: "inherit",
  });

  const browserProcess = command.spawn();

  // Monitor the browser process
  (async () => {
    try {
      await browserProcess.status;
      log.info("🔄 Browser process ended");
    } catch (error) {
      log.error("💥 Browser process error:", error);
    }
  })();

  // Handle graceful shutdown
  const handleShutdown = async () => {
    log.info("🛑 Shutting down browser...");
    try {
      const writer = browserProcess.stdin.getWriter();
      await writer.write(new TextEncoder().encode("q"));
      writer.releaseLock();
    } catch (error) {
      log.error("Error during shutdown:", error);
    }
  };

  // Listen for interrupt signals (Windows compatibility)
  try {
    Deno.addSignalListener("SIGINT", handleShutdown);
    // Only add SIGTERM on non-Windows platforms
    if (Deno.build.os !== "windows") {
      Deno.addSignalListener("SIGTERM", handleShutdown);
    }
  } catch (error) {
    log.warn("Could not register signal listeners:", error);
  }
}

/**
 * Initialize _dist directory structure
 */
async function initializeDistDirectory(): Promise<void> {
  log.info("📁 Initializing _dist directory structure...");

  const projectRoot = resolve(import.meta.dirname!, "..");
  const distDir = resolve(projectRoot, "_dist");
  const binDir = resolve(distDir, "bin");
  const profileDir = resolve(distDir, "profile");

  try {
    // Ensure all necessary directories exist
    await ensureDir(distDir);
    await ensureDir(binDir);
    await ensureDir(profileDir);

    log.info("✅ _dist directory structure initialized");
  } catch (error) {
    log.error(`Failed to initialize _dist directory: ${error}`);
    throw error;
  }
}

/**
 * Main build and launch function
 */
export async function buildAndLaunch(options: BuildOptions): Promise<void> {
  const { mode, phase = "full", initGitForPatch = false } = options;

  // Auto-determine browser launch based on mode and phase
  let shouldLaunchBrowser = false;
  if (mode === "dev" && phase === "full") {
    shouldLaunchBrowser = true;
  }
  // Allow manual override
  if (options.launchBrowser !== undefined) {
    shouldLaunchBrowser = options.launchBrowser;
  }

  try {
    // Initialize _dist directory structure first
    await initializeDistDirectory();

    // Run the build process based on phase
    await build({ mode, phase, initGitForPatch });

    // Launch browser if requested
    if (shouldLaunchBrowser) {
      launchBrowser();
    }
  } catch (error) {
    log.error(`💥 Build and launch failed: ${error}`);
    Deno.exit(1);
  }
}

/**
 * CLI interface for direct execution
 */
if (import.meta.main) {
  const args = Deno.args;

  if (args.includes("--help") || args.includes("-h")) {
    console.log(`
🏗️  Noraneko Build and Launch System

Development Usage:
  deno run -A tools/build.ts --dev              # Development build with browser launch

Production/CI Usage (split build process):
  deno run -A tools/build.ts --production-before    # Before phase: noranekoスクリプト実行
  [External Mozilla build execution with ./mach build]
  deno run -A tools/build.ts --production-after     # After phase: Inject処理

Patch Development (for patch creators only):
  deno run -A tools/build.ts --dev --init-git-for-patch      # Development build with Git initialization for patch creation
  deno run -A tools/build.ts --production --init-git-for-patch # Production build with Git initialization for patch creation

Legacy Options:
  deno run -A tools/build.ts --production       # Full production build (not recommended for CI)

CI Aliases:
  --release-build-before    # Same as --production-before
  --release-build-after     # Same as --production-after

Note: --init-git-for-patch is only needed when creating new patches, not for regular Noraneko development.
    `);
    Deno.exit(0);
  }

  // Parse arguments
  const initGitForPatch = args.includes("--init-git-for-patch");

  if (args.includes("--dev")) {
    await buildAndLaunch({ mode: "dev", initGitForPatch });
  } else if (
    args.includes("--production-before") ||
    args.includes("--release-build-before")
  ) {
    await buildAndLaunch({
      mode: "production",
      phase: "before",
      initGitForPatch,
    });
  } else if (
    args.includes("--production-after") ||
    args.includes("--release-build-after")
  ) {
    await buildAndLaunch({
      mode: "production",
      phase: "after",
      initGitForPatch,
    });
  } else if (args.includes("--production")) {
    await buildAndLaunch({ mode: "production", initGitForPatch });
  } else {
    // Default to development build
    await buildAndLaunch({ mode: "dev", initGitForPatch });
  }
}
