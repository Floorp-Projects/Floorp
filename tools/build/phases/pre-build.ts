/**
 * Pre-build phase: Setup and preparation before Mozilla build
 */

import { log } from "../logger.ts";
import { initBin } from "../../dev/prepareDev/initBin.ts";
import {
  applyPatches,
  initializeBinGit,
} from "../tasks/git-patches/git-patches-manager.ts";

export async function runPreBuildPhase(): Promise<void> {
  log.info("🔧 Starting pre-build phase...");

  try {
    // Initialize binary setup
    log.info("📦 Initializing binary setup...");
    await initBin();

    // Initialize Git for patches
    log.info("🔀 Initializing Git for patches...");
    await initializeBinGit();

    // Apply patches
    log.info("🩹 Applying patches...");
    await applyPatches();

    log.info("✅ Pre-build phase completed successfully");
  } catch (error) {
    log.error(`❌ Pre-build phase failed: ${error}`);
    throw error;
  }
}
