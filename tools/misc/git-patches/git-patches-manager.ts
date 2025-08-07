import { brandingBaseName, brandingName } from "../../utils/defines.ts";
import { join } from "@std/path";
import {
  ensureDir,
  isExistsSync,
  isGitInitialized,
  runCommandChecked,
} from "../../utils/utils.ts";

// Helper to execute git commands
async function runGitCommand(
  args: string[],
  options: { cwd?: string } = {},
): Promise<{ stdout: string; stderr: string }> {
  return await runCommandChecked("git", args, options);
}

function getBinDir(): string {
  return Deno.build.os !== "darwin"
    ? `_dist/bin/${brandingBaseName}`
    : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;
}

const PATCHES_DIR = "tools/build/tasks/git-patches/patches";
const PATCHES_TMP = "_dist/bin/applied_patches";

export async function initializeBinGit() {
  const BIN_DIR = getBinDir();
  if (await isGitInitialized(BIN_DIR)) {
    console.log(
      "[git-patches] Git repository is already initialized in _dist/bin.",
    );
    return;
  }
  console.log("[git-patches] Initializing Git repository in _dist/bin.");
  // Create .gitignore
  await ensureDir(BIN_DIR);
  await Deno.writeTextFile(
    join(BIN_DIR, ".gitignore"),
    [
      `./noraneko-dev/*`,
      `./browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*`,
    ].join("\n"),
  );
  // Initialize Git
  await runGitCommand(["init"], { cwd: BIN_DIR });
  await runGitCommand(["add", "."], { cwd: BIN_DIR });
  await runGitCommand(["commit", "-m", "initialize"], { cwd: BIN_DIR });
  console.log("[git-patches] Git repository initialization complete.");
}

export function checkPatchIsNeeded(): boolean {
  // Check if patch directory exists
  if (!isExistsSync(PATCHES_DIR, { isDirectory: true })) return false;
  if (!isExistsSync(PATCHES_TMP, { isDirectory: true })) return true;
  // Additional patch application logic can be added here
  return false;
}

// Additional patch management functions (applyPatches, createPatches) should be migrated here as needed.
