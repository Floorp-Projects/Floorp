/**
 * Noraneko Build System - Main Entry Point
 *
 * This module provides build orchestration and browser launching interface.
 * It delegates to the modular build system in tools/build/ while providing
 * a clean CLI interface for development and production workflows.
 */

import { build } from "./build/index.ts";
import { log } from "./build/logger.ts";
import { resolve } from "jsr:@std/path@^1.0.8";
import { ensureDir } from "jsr:@std/fs@^1.0.18";

// Global type definitions for process monitoring
declare global {
  var noranekoDevProcess: Deno.ChildProcess | undefined;
  var noranekoBuildProcess: Deno.ChildProcess | undefined;
}

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
  initGitForPatch?: boolean; // Git initialization for patch creation (only needed by patch creators)
}

// Global process tracking for proper cleanup
let devServerProcess: Deno.ChildProcess | null = null;
let buildServerProcess: Deno.ChildProcess | null = null;

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

  // Monitor the browser process for automatic shutdown
  (async () => {
    try {
      const status = await browserProcess.status;
      log.info("🔄 Browser process ended");
      
      // Browser closed - shutdown all dev processes
      if (!status.success || status.code === 0) {
        log.info("🛑 Browser closed, shutting down development servers...");
        await shutdownDevProcesses();
        log.info("✅ Development environment shutdown complete");
        Deno.exit(0);
      }
    } catch (error) {
      log.error("💥 Browser process error:", error);
      await shutdownDevProcesses();
      Deno.exit(1);
    }
  })();

  // Handle graceful shutdown
  const handleShutdown = async () => {
    log.info("🛑 Shutting down browser and dev servers...");
    try {
      const writer = browserProcess.stdin.getWriter();
      await writer.write(new TextEncoder().encode("q"));
      writer.releaseLock();
      await shutdownDevProcesses();
    } catch (error) {
      log.error("Error during shutdown:", error);
    }
    Deno.exit(0);
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
 * Shutdown all development processes gracefully
 */
async function shutdownDevProcesses(): Promise<void> {
  const promises: Promise<void>[] = [];

  if (devServerProcess) {
    log.info("🔴 Stopping dev server...");
    promises.push(
      (async () => {
        try {
          const writer = devServerProcess!.stdin.getWriter();
          await writer.write(new TextEncoder().encode("q"));
          writer.releaseLock();
          await devServerProcess!.status;
        } catch (error) {
          log.warn("Error stopping dev server:", error);
          try {
            devServerProcess!.kill("SIGTERM");
          } catch (killError) {
            log.warn("Error killing dev server:", killError);
          }
        }
        devServerProcess = null;
      })()
    );
  }

  if (buildServerProcess) {
    log.info("🔴 Stopping build server...");
    promises.push(
      (async () => {
        try {
          buildServerProcess!.kill("SIGTERM");
          await buildServerProcess!.status;
        } catch (error) {
          log.warn("Error stopping build server:", error);
        }
        buildServerProcess = null;
      })()
    );
  }

  if (promises.length > 0) {
    await Promise.allSettled(promises);
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
 * Set development server process reference for monitoring
 */
export function setDevServerProcess(process: Deno.ChildProcess): void {
  devServerProcess = process;
}

/**
 * Set build server process reference for monitoring  
 */
export function setBuildServerProcess(process: Deno.ChildProcess): void {
  buildServerProcess = process;
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
🏗️  Noraneko Build System

Development Usage:
  deno run -A tools/build.ts --dev              # Development build with browser launch

Production/CI Usage (recommended for CI):
  deno run -A tools/build.ts --production-before    # Phase 1: Prepare Noraneko scripts
  [Run external Mozilla build: ./mach build]
  deno run -A tools/build.ts --production-after     # Phase 2: Apply injections

Patch Development (for patch creators only):
  deno run -A tools/build.ts --dev --init-git-for-patch        # Dev build with Git init
  deno run -A tools/build.ts --production --init-git-for-patch # Prod build with Git init

Legacy Options:
  deno run -A tools/build.ts --production       # Full production build (not recommended for CI)

CI Aliases:
  --release-build-before    # Alias for --production-before
  --release-build-after     # Alias for --production-after

Note: 
  --init-git-for-patch is only needed when creating new patches for Firefox integration.
  Regular Noraneko developers do not need this option.
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
