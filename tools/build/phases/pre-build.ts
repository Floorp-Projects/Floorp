import { log } from "../logger.ts";
import { initBin } from "../../dev/prepareDev/initBin.ts";
import {
  applyPatches,
  checkPatchIsNeeded,
  initializeBinGit,
} from "../tasks/git-patches/git-patches-manager.ts";
import { setupBuildSymlinks } from "../tasks/setup/symlinks.ts";
import { runChildBuild, runChildDev } from "../utils.ts";
import type { PreBuildOptions } from "../types.ts";

export async function runPreBuildPhase(
  options: PreBuildOptions = {},
): Promise<void> {
  const { initGitForPatch = false, isDev = false } = options;

  log.info("🔧 Starting pre-build phase...");

  await initBin();

  if (initGitForPatch) {
    log.info("🔀 Initializing Git for patches...");
    await initializeBinGit();
  }

  if (await checkPatchIsNeeded()) {
    log.info("🩹 Applying patches...");
    await applyPatches();
  }

  log.info("🔗 Setting up build symlinks...");
  await setupBuildSymlinks();

  log.info("🔨 Running child-build process...");
  await runChildBuild(isDev);

  if (isDev) {
    log.info("🚀 Starting development server...");
    await runChildDev();
  }

  log.success("Pre-build phase completed");
}
