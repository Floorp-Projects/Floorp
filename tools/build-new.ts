/**
 * Noraneko Build and Launch System
 *
 * This module provides a minimal build orchestration and browser launching interface.
 * It delegates the actual build work to the existing build system while providing
 * browser launch integration for development workflows.
 */

import { build } from "./build/index.ts";
import { log } from "./build/logger.ts";

/**
 * Available build modes
 */
export type BuildMode = "dev" | "production";

/**
 * Build phase options
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
  const { mode, phase = "full", launchBrowser: shouldLaunchBrowser = false } =
    options;

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
  console.log("CLI interface started");
  const args = Deno.args;
  console.log("Args:", args);

  // Parse command line arguments
  const mode: BuildMode = args.includes("--dev") ? "dev" : "production";
  const launchBrowser = args.includes("--launch-browser");

  let phase: BuildPhase = "full";
  if (args.includes("--before")) {
    phase = "before";
  } else if (args.includes("--after")) {
    phase = "after";
  }

  // Show help if requested
  if (args.includes("--help") || args.includes("-h")) {
    console.log(`
🏗️  Noraneko Build and Launch System

Usage:
  deno run -A tools/build.ts [options]

Build Phases:
  (default)               Full build process
  --before               Before phase: noranekoスクリプト実行
  --after                After phase: Inject処理

Build Flow:
  1. deno run -A tools/build.ts --before
  2. [External Mozilla build execution]
  3. deno run -A tools/build.ts --after

Options:
  --dev                   Development build (default: production)
  --before               Run only before phase (noranekoスクリプト実行)
  --after                Run only after phase (Inject処理)
  --launch-browser       Launch browser after build
  --help, -h             Show this help message

Examples:
  deno run -A tools/build.ts --dev --launch-browser
  deno run -A tools/build.ts --before --dev
  deno run -A tools/build.ts --after --dev --launch-browser
    `);
    Deno.exit(0);
  }

  await buildAndLaunch({ mode, phase, launchBrowser });
}
