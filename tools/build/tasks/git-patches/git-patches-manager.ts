/// <reference lib="deno.ns" />
import { brandingBaseName, brandingName } from "../../defines.ts";
import { join, relative, resolve } from "@std/path";
import {
  ensureDir,
  isExistsSync,
  isGitInitialized,
  runCommandChecked,
  safeRemove,
} from "../../utils.ts";

// Helper function to run git commands
async function runGitCommand(
  args: string[],
  options: { cwd?: string } = {},
): Promise<{ stdout: string; stderr: string }> {
  return await runCommandChecked("git", args, options);
}

function getBinDir() {
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
      "[git-patches] Git repository already initialized in _dist/bin",
    );
    return;
  }

  console.log("[git-patches] Initializing git repository in _dist/bin");

  // create .gitignore
  await Deno.writeTextFile(
    join(BIN_DIR, ".gitignore"),
    [
      `./noraneko-dev/*`,
      `./browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*`,
    ].join("\n"),
  );

  // Initialize git repository
  await runGitCommand(["init"], { cwd: BIN_DIR });
  await runGitCommand(["add", "."], { cwd: BIN_DIR });
  await runGitCommand(["commit", "-m", "initialize"], { cwd: BIN_DIR });

  console.log("[git-patches] Git repository initialized in _dist/bin");
}

export function checkPatchIsNeeded() {
  // Check if patches directory exists
  if (!isExistsSync(PATCHES_DIR, { isDirectory: true })) {
    return false;
  }

  if (!isExistsSync(PATCHES_TMP, { isDirectory: true })) {
    return true;
  }

  const patches_tmp = Deno.readDirSync(PATCHES_TMP);
  const patches_dir = Deno.readDirSync(PATCHES_DIR);

  const patches_tmp_filenames = Array.from(patches_tmp.map((p) => p.name));
  const filenames_eq = patches_dir
    .map((p) => p.name)
    .filter((v) => v.endsWith(".patch"))
    .every((p) => {
      return patches_tmp_filenames.includes(p);
    });

  if (!filenames_eq) {
    // if filenames are not equal, need to apply patches
    return true;
  }

  const files_eq = patches_dir.every((patch) => {
    const patch_in_dir = Deno.readTextFileSync(join(PATCHES_DIR, patch.name));
    const patch_in_tmp = Deno.readTextFileSync(join(PATCHES_TMP, patch.name));
    return patch_in_dir === patch_in_tmp;
  });
  if (!files_eq) {
    // if Files are not equal, need to apply patches
    return true;
  }

  // filenames are all equal and files are all equal
  // so the all patches are applied
  // no need to apply patches
  return false;
}

export async function applyPatches(binDir = getBinDir()) {
  if (!checkPatchIsNeeded()) {
    console.log(`[git-patches] No patches needed to apply`);
    return;
  }
  let reverse_is_aborted = false;
  try {
    await Deno.stat(PATCHES_TMP);
    // Already the bin is patched
    // Need reverse patch
    const patches_tmp_entries = [];
    for await (const entry of Deno.readDir(PATCHES_TMP)) {
      patches_tmp_entries.push(entry.name);
    }

    for (const patch of patches_tmp_entries) {
      try {
        // Use absolute paths to avoid path resolution issues on Windows
        const patchPath = resolve(PATCHES_TMP, patch);
        const relativeBinDir = relative(Deno.cwd(), binDir);

        await runGitCommand(
          [
            "apply",
            "-R",
            "--reject",
            "--whitespace=fix",
            "--unsafe-paths",
            `--directory=${relativeBinDir}`,
            patchPath,
          ],
          { cwd: Deno.cwd() },
        );
      } catch (e) {
        console.warn(`[git-patches] Failed to reverse patch: ${patch}`);
        console.warn(e);
        reverse_is_aborted = true;
      }
    }
    if (!reverse_is_aborted) {
      await safeRemove(PATCHES_TMP);
    }
  } catch {
    // Ignore if PATCHES_TMP doesn't exist - means no patches were applied before
    console.log("[git-patches] No previous patches to reverse");
  }
  if (reverse_is_aborted) {
    throw new Error("[git-patches] Reverse Patch Failed: aborted");
  }

  const patches_entries = [];
  for await (const entry of Deno.readDir(PATCHES_DIR)) {
    patches_entries.push(entry.name);
  }

  await ensureDir(PATCHES_TMP);
  let aborted = false;

  for (const patch of patches_entries) {
    if (!patch.endsWith(".patch")) continue;

    console.log(`Applying patch: ${patch}`);
    try {
      // Use absolute paths to avoid path resolution issues on Windows
      const patchPath = resolve(PATCHES_DIR, patch);
      const relativeBinDir = relative(Deno.cwd(), binDir);

      await runGitCommand(
        [
          "apply",
          "--reject",
          "--whitespace=fix",
          "--unsafe-paths",
          `--directory=${relativeBinDir}`,
          patchPath,
        ],
        { cwd: Deno.cwd() },
      );

      await Deno.copyFile(join(PATCHES_DIR, patch), join(PATCHES_TMP, patch));
    } catch (e) {
      console.warn(`[git-patches] Failed to apply patch: ${patch}`);
      console.warn(e);
      aborted = true;
    }
  }
  if (aborted) {
    throw new Error(`[git-patches] Patch failed: aborted`);
  }
}

export async function createPatches() {
  const BIN_DIR = getBinDir();
  if (!(await isGitInitialized(BIN_DIR))) {
    throw new Error(
      "[git-patches] Git repository not initialized. Run initializeBinGit first.",
    );
  }

  // Get list of changed files
  const { stdout: diffNameOnly } = await runGitCommand(
    ["diff", "--name-only"],
    { cwd: BIN_DIR },
  );
  const changedFiles = diffNameOnly.split("\n").filter(Boolean);

  if (changedFiles.length === 0) {
    console.log("[git-patches] No changes detected");
    return;
  }

  // Create patches directory if it doesn't exist
  await ensureDir(PATCHES_DIR);

  // Create patch for each changed file
  for (const file of changedFiles) {
    try {
      const { stdout: diff } = await runGitCommand(["diff", file], {
        cwd: BIN_DIR,
      });

      const modifiedDiff = `${`${diff}`
        .replace(/^--- a\//gm, "--- ./")
        .replace(/^\+\+\+ b\//gm, "+++ ./")
        .trim()}\n`;

      const patchName = `${file
        .replace(/\//g, "-")
        .replace(/\.[^/.]+$/, "")}.patch`;

      const patchPath = join(PATCHES_DIR, patchName);

      await Deno.writeTextFile(patchPath, modifiedDiff);
      console.log(`[git-patches] Created/Updated patch: ${patchPath}`);
    } catch (e) {
      console.warn(`[git-patches] Failed to create patch: ${file}`);
      console.warn(e);
    }
  }
}

if (Deno.args[0]) {
  switch (Deno.args[0]) {
    case "--apply":
      applyPatches();
      break;
    case "--create":
      createPatches();
      break;
    case "--init":
      initializeBinGit();
      break;
  }
}
