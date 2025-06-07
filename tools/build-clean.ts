/**
 * Noraneko Build and Launch System
 *
 * Simplified build system for development and production workflows.
 */

import { build } from "./build/index.ts";
import { log } from "./build/logger.ts";

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
 * Main build and launch function
 */
export async function buildAndLaunch(options: BuildOptions): Promise<void> {
  const { mode, phase = "full" } = options;

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
    // Run the build process based on phase
    await build({ mode, phase });

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

Legacy Options:
  deno run -A tools/build.ts --production       # Full production build (not recommended for CI)

CI Aliases:
  --release-build-before    # Same as --production-before
  --release-build-after     # Same as --production-after
    `);
    Deno.exit(0);
  }

  // Parse arguments
  if (args.includes("--dev")) {
    await buildAndLaunch({ mode: "dev" });
  } else if (
    args.includes("--production-before") ||
    args.includes("--release-build-before")
  ) {
    await buildAndLaunch({ mode: "production", phase: "before" });
  } else if (
    args.includes("--production-after") ||
    args.includes("--release-build-after")
  ) {
    await buildAndLaunch({ mode: "production", phase: "after" });
  } else if (args.includes("--production")) {
    await buildAndLaunch({ mode: "production" });
  } else {
    // Default to development build
    await buildAndLaunch({ mode: "dev" });
  }
}
