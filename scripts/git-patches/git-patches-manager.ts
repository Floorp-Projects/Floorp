/// <reference lib="deno.ns" />
import { brandingBaseName, brandingName } from "../../build.ts";
import * as fs from "node:fs/promises";
import * as path from "node:path";
import { $ } from "zx";
import process from "node:process";
import { resolve } from "pathe";
import { existsSync } from "@std/fs";

function getBinDir() {
  return process.platform !== "darwin"
    ? `_dist/bin/${brandingBaseName}`
    : `_dist/bin/${brandingBaseName}/${brandingName}.app/Contents/Resources`;
}

const PATCHES_DIR = "scripts/git-patches/patches";
const PATCHES_TMP = "_dist/bin/applied_patches";

async function isGitInitialized(dir: string): Promise<boolean> {
  try {
    await fs.access(path.join(dir, ".git"));
    return true;
  } catch {
    return false;
  }
}

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
  await fs.writeFile(
    path.join(BIN_DIR, ".gitignore"),
    [
      `./noraneko-dev/*`,
      `./browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*`,
    ].join("\n"),
  );

  // Initialize git repository
  await $({ cwd: BIN_DIR })`git init`;
  await $({ cwd: BIN_DIR })`git add .`;
  await $({ cwd: BIN_DIR })`git commit -m initialize`;

  console.log("[git-patches] Git repository initialized in _dist/bin");
}

export function checkPatchIsNeeded() {
  if (!existsSync(PATCHES_TMP, { "isDirectory": true })) {
    return true;
  }
  const patches_tmp = Deno.readDirSync(PATCHES_TMP);
  const patches_dir = Deno.readDirSync(PATCHES_DIR);

  const patches_tmp_filenames = Array.from(patches_tmp.map((p) => p.name));
  const filenames_eq = patches_dir.map((p) => p.name).every((p) => {
    return patches_tmp_filenames.includes(p);
  });

  if (!filenames_eq) {
    // if filenames are not equal, need to apply patches
    return true;
  }

  const files_eq = patches_dir.every((patch) => {
    const patch_in_dir = Deno.readTextFileSync(
      resolve(PATCHES_DIR, patch.name),
    );
    const patch_in_tmp = Deno.readTextFileSync(
      resolve(PATCHES_TMP, patch.name),
    );
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
    await fs.access(PATCHES_TMP);
    // Already the bin is patched
    // Need reverse patch
    const reverse_patches = await fs.readdir(PATCHES_TMP);
    for (const patch of reverse_patches) {
      const patchPath = path.join(PATCHES_TMP, patch);
      try {
        await $`git apply -R --reject --whitespace=fix --unsafe-paths --directory ${binDir} ./${patchPath}`;
      } catch (e) {
        console.warn(`[git-patches] Failed to reverse patch: ${patchPath}`);
        console.warn(e);
        reverse_is_aborted = true;
      }
    }
    if (!reverse_is_aborted) {
      await fs.rm(PATCHES_TMP, { "recursive": true });
    }
  } catch {}
  if (reverse_is_aborted) {
    throw new Error("[git-patches] Reverse Patch Failed: aborted");
  }
  const patches = await fs.readdir(PATCHES_DIR);
  await fs.mkdir(PATCHES_TMP, { "recursive": true });
  let aborted = false;
  for (const patch of patches) {
    if (!patch.endsWith(".patch")) continue;

    const patchPath = path.join(PATCHES_DIR, patch);
    console.log(`Applying patch: ${patchPath}`);
    try {
      await $`git apply --reject --whitespace=fix --unsafe-paths --directory ${binDir} ./${patchPath}`;
      await fs.cp(patchPath, PATCHES_TMP + "/" + patch);
    } catch (e) {
      console.warn(`[git-patches] Failed to apply patch: ${patchPath}`);
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
  const { stdout: diffNameOnly } = await $({
    cwd: BIN_DIR,
  })`git diff --name-only`;
  const changedFiles = diffNameOnly.split("\n").filter(Boolean);

  if (changedFiles.length === 0) {
    console.log("[git-patches] No changes detected");
    return;
  }

  // Create patches directory if it doesn't exist
  await fs.mkdir(PATCHES_DIR, { recursive: true });

  // Create patch for each changed file
  for (const file of changedFiles) {
    try {
      const { stdout: diff } = await $({ cwd: BIN_DIR })`git diff ${file}`;

      const modifiedDiff = `${
        `${diff}`
          .replace(/^--- a\//gm, "--- ./")
          .replace(/^\+\+\+ b\//gm, "+++ ./")
          .trim()
      }\n`;

      const patchName = `${
        file
          .replace(/\//g, "-")
          .replace(/\.[^/.]+$/, "")
      }.patch`;

      const patchPath = path.join(PATCHES_DIR, patchName);

      await fs.writeFile(patchPath, modifiedDiff);
      console.log(`[git-patches] Created/Updated patch: ${patchPath}`);
    } catch (e) {
      console.warn(`[git-patches] Failed to create patch: ${file}`);
      console.warn(e);
    }
  }
}

if (process.argv[2]) {
  switch (process.argv[2]) {
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
