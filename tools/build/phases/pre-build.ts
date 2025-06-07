/**
 * Pre-build phase: Setup and preparation before Mozilla build
 */

import { log } from "../logger.ts";
import { initBin } from "../../dev/prepareDev/initBin.ts";
import {
  applyPatches,
  checkPatchIsNeeded,
  initializeBinGit,
} from "../tasks/git-patches/git-patches-manager.ts";
import { readBuildid2 } from "../tasks/update/buildid2.ts";
import { setupBuildSymlinks } from "../tasks/setup/symlinks.ts";
import { resolve } from "pathe";

// Global type definitions for process monitoring
declare global {
  var noranekoDevProcess: Deno.ChildProcess | undefined;
  var noranekoBuildProcess: Deno.ChildProcess | undefined;
}

export async function runPreBuildPhase(
  options?: { initGitForPatch?: boolean; isDev?: boolean },
): Promise<void> {
  log.info("🔧 Starting pre-build phase...");
  const { initGitForPatch = false, isDev = false } = options || {};

  try {
    // Initialize binary setup
    log.info("📦 Initializing binary setup...");
    await initBin();

    // Initialize Git for patches (only when explicitly requested for patch creation)
    if (initGitForPatch) {
      log.info("🔀 Initializing Git for patches (patch creation mode)...");
      await initializeBinGit();
    } else {
      log.info(
        "⏭️  Skipping Git initialization (not needed for regular development)",
      );
    }

    // Apply patches (only if patches exist and are needed)
    const patchesNeeded = await checkPatchIsNeeded();
    if (patchesNeeded) {
      log.info("🩹 Applying patches...");
      await applyPatches();
    } else {
      log.info("⏭️  No patches needed to apply");
    }

    // Setup symlinks for build dependencies
    log.info("🔗 Setting up build symlinks...");
    await setupBuildSymlinks();

    // Run child-build.ts to compile components
    log.info("🔨 Running child-build process...");
    await runChildBuild(isDev);

    // Run child-dev.ts if in development mode
    if (isDev) {
      log.info("🚀 Starting development server...");
      await runChildDev();
    }

    log.info("✅ Pre-build phase completed successfully");
  } catch (error) {
    log.error(`❌ Pre-build phase failed: ${error}`);
    throw error;
  }
}

async function runChildBuild(isDev: boolean): Promise<void> {
  try {
    const projectRoot = resolve(import.meta.dirname!, "../../../");
    const buildid2 = await readBuildid2(resolve(projectRoot, "_dist")) ||
      "default-build-id";
    const mode = isDev ? "dev" : "prod";

    const childBuildPath = resolve(
      projectRoot,
      "tools/dev/launchDev/child-build.ts",
    );

    const command = new Deno.Command(Deno.execPath(), {
      args: ["run", "-A", childBuildPath, mode, buildid2],
      cwd: projectRoot,
      stdout: "piped",
      stderr: "piped",
    });

    const process = command.spawn();
    const { code, stdout, stderr } = await process.output();

    if (code !== 0) {
      const errorOutput = new TextDecoder().decode(stderr);
      throw new Error(`Child build failed with code ${code}: ${errorOutput}`);
    }

    const output = new TextDecoder().decode(stdout);
    if (output.trim()) {
      log.info(`Child build output: ${output}`);
    }
  } catch (error) {
    log.error(`Failed to run child-build: ${error}`);
    throw error;
  }
}

async function runChildDev(): Promise<void> {
  try {
    const projectRoot = resolve(import.meta.dirname!, "../../../");
    const buildid2 = await readBuildid2(resolve(projectRoot, "_dist")) ||
      "default-build-id";
    const mode = "dev";

    const childDevPath = resolve(
      projectRoot,
      "tools/dev/launchDev/child-dev.ts",
    );

    const command = new Deno.Command(Deno.execPath(), {
      args: ["run", "-A", childDevPath, mode, buildid2],
      cwd: projectRoot,
      stdin: "piped", // Add stdin for shutdown communication
      stdout: "piped",
      stderr: "piped",
    });

    const process = command.spawn();
    
    // Store the process reference for monitoring from browser
    // We'll use a global storage mechanism since we can't import from build.ts here
    // due to circular dependency. Instead, we'll store it in a way that the main
    // build process can access it.
    globalThis.noranekoDevProcess = process;

    log.info("Development server started in background");

    // Monitor the dev server output in background
    (async () => {
      try {
        const decoder = new TextDecoder();
        if (process.stdout) {
          const reader = process.stdout.getReader();
          try {
            while (true) {
              const { done, value } = await reader.read();
              if (done) break;
              const output = decoder.decode(value);
              if (output.trim()) {
                log.info(`[Dev Server]: ${output.trim()}`);
              }
            }
          } finally {
            reader.releaseLock();
          }
        }
      } catch (error) {
        log.warn(`Dev server output monitoring error: ${error}`);
      }
    })();

    // Monitor stderr separately
    (async () => {
      try {
        const decoder = new TextDecoder();
        if (process.stderr) {
          const reader = process.stderr.getReader();
          try {
            while (true) {
              const { done, value } = await reader.read();
              if (done) break;
              const output = decoder.decode(value);
              if (output.trim()) {
                log.warn(`[Dev Server Error]: ${output.trim()}`);
              }
            }
          } finally {
            reader.releaseLock();
          }
        }
      } catch (error) {
        log.warn(`Dev server error monitoring error: ${error}`);
      }
    })();

    // Wait a bit to see if there are immediate errors
    await new Promise((resolve) => setTimeout(resolve, 2000));
    log.info("Development server appears to have started successfully");

  } catch (error) {
    log.error(`Failed to run child-dev: ${error}`);
    throw error;
  }
}
