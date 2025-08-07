import { log } from "../../utils/logger.ts";
import { initBin } from "../prepareDev/initBin.ts";
import {
  // applyPatches,
  checkPatchIsNeeded,
  initializeBinGit,
} from "../git-patches/git-patches-manager.ts";
import { setupBuildSymlinks } from "../setup/symlinks.ts";
import { runChildBuild, runChildDev } from "../../utils/utils.ts";
import type { PreBuildOptions } from "../../utils/types.ts";

export async function runPreBuildPhase(
  options: PreBuildOptions = {},
): Promise<void> {
  const { initGitForPatch = false, isDev = false, mode = "dev" } = options;

  log.info("🔧 Starting pre-build phase...");

  await initBin();

  if (initGitForPatch) {
    log.info("🔀 Initializing Git for patches...");
    await initializeBinGit();
  }

  if (await checkPatchIsNeeded()) {
    // log.info("🩹 Applying patches...");
    // await applyPatches();
  }

  log.info("🔗 Setting up build symlinks...");
  await setupBuildSymlinks();

  log.info("🔨 Running child-build process...");
  await runChildBuild(isDev);

  if (isDev) {
    log.info("🚀 Starting development server...");
    // Map "stage" to "production" for runChildDev
    await runChildDev(mode === "stage" ? "production" : mode);
  }

  log.success("Pre-build phase completed");
}
