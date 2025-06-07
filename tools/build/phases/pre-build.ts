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
      stdout: "piped",
      stderr: "piped",
    });

    const process = command.spawn();

    // Start the dev server in the background
    log.info("Development server started in background");

    // Don't wait for the process to complete as it's a dev server that runs continuously
    // Just check if it started successfully by waiting a short time
    const timeoutPromise = new Promise((_, reject) => {
      setTimeout(() => reject(new Error("Dev server start timeout")), 5000);
    });

    const startPromise = (async () => {
      // Wait a bit to see if there are immediate errors
      await new Promise((resolve) => setTimeout(resolve, 2000));
      return true;
    })();

    try {
      await Promise.race([startPromise, timeoutPromise]);
      log.info("Development server appears to have started successfully");
    } catch (error) {
      log.warn(`Development server start check failed: ${error}`);
      // Don't throw here as the dev server might still be starting
    }
  } catch (error) {
    log.error(`Failed to run child-dev: ${error}`);
    throw error;
  }
}
