import * as fs from "node:fs/promises";
import * as path from "node:path";
import { execa } from "execa";

const BIN_DIR = "_dist/bin/noraneko";
const PATCHES_DIR = "scripts/git-patches/patches";
const PATCHES_TMP = "_dist/bin/applied_patches"

async function isGitInitialized(dir: string): Promise<boolean> {
  try {
    await fs.access(path.join(dir, ".git"));
    return true;
  } catch {
    return false;
  }
}

export async function initializeBinGit() {
  if (await isGitInitialized(BIN_DIR)) {
    console.log("Git repository already initialized in _dist/bin");
    return;
  }

  console.log("Initializing git repository in _dist/bin");

  // create .gitignore
  await fs.writeFile(
    path.join(BIN_DIR, ".gitignore"),
    `
./noraneko-dev/*
./browser/chrome/browser/res/activity-stream/data/content/abouthomecache/*
`,
  );

  // Initialize git repository
  await execa("git", ["init"], { cwd: BIN_DIR });
  await execa("git", ["add", "."], { cwd: BIN_DIR });
  await execa("git", ["commit", "-m", "initialize"], { cwd: BIN_DIR });

  console.log("Git repository initialized in _dist/bin");
}

export async function applyPatches(binDir = BIN_DIR) {
  let reverse_is_aborted = false;
  try {
    await fs.access(PATCHES_TMP);
    // Already the bin is patched
    // Need reverse patch
    const reverse_patches = await fs.readdir(PATCHES_TMP);
    for (const patch of reverse_patches) {
      const patchPath = path.join(PATCHES_TMP, patch);
      try {
        await execa(
          "git",
          ["apply", "-R", "--reject", "--whitespace=fix","--unsafe-paths", "--directory",`${binDir}`,`./${patchPath}`],
          {
            shell: true,
          },
        );
      } catch (e) {
        console.warn(`Failed to reverse patch: ${patchPath}`);
        console.warn(e);
        reverse_is_aborted = true;
      }
    }
    if (!reverse_is_aborted) {
      await fs.rm(PATCHES_TMP,{"recursive":true});
    }
   
  } catch {};
  if (reverse_is_aborted) {
    throw new Error("reverse_patch failed")
  }
  const patches = await fs.readdir(PATCHES_DIR);
  await fs.mkdir(PATCHES_TMP,{"recursive":true});
  let aborted = false;
  for (const patch of patches) {
    if (!patch.endsWith(".patch")) continue;

    const patchPath = path.join(PATCHES_DIR, patch);
    console.log(`Applying patch: ${patchPath}`);
    try {
      await execa(
        "git",
        ["apply", "--reject", "--whitespace=fix","--unsafe-paths", "--directory",`${binDir}`,`./${patchPath}`],
        {
          shell: true,
        },
      );
      await fs.cp(patchPath,PATCHES_TMP+"/"+patch);
    } catch (e) {
      console.warn(`Failed to apply patch: ${patchPath}`);
      console.warn(e);
      aborted = true;
    }
  }
  if (aborted) {
    throw new Error("patch failed")
  }
}

export async function createPatches() {
  if (!(await isGitInitialized(BIN_DIR))) {
    throw new Error(
      "Git repository not initialized. Run initializeBinGit first.",
    );
  }

  // Get list of changed files
  const { stdout: diffNameOnly } = await execa("git", ["diff", "--name-only"], {
    cwd: BIN_DIR,
  });
  const changedFiles = diffNameOnly.split("\n").filter(Boolean);

  if (changedFiles.length === 0) {
    console.log("No changes detected");
    return;
  }

  // Create patches directory if it doesn't exist
  await fs.mkdir(PATCHES_DIR, { recursive: true });

  // Create patch for each changed file
  for (const file of changedFiles) {
    try {
      const { stdout: diff } = await execa("git", ["diff", file], {
        cwd: BIN_DIR,
      });

      const modifiedDiff = `${`${diff}`
        .replace(/^--- a\//gm, "--- ./")
        .replace(/^\+\+\+ b\//gm, "+++ ./")
        .trim()}\n`;

      const patchName = `${file
        .replace(/\//g, "-")
        .replace(/\.[^/.]+$/, "")}.patch`;

      const patchPath = path.join(PATCHES_DIR, patchName);

      await fs.writeFile(patchPath, modifiedDiff);
      console.log(`Created/Updated patch: ${patchPath}`);
    } catch (e) {
      console.warn(`Failed to create patch: ${file}`);
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
