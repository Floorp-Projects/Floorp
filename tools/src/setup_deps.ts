// SPDX-License-Identifier: MPL-2.0

/**
 * npm dependency step of the FDH local setup (IMP-5).
 *
 * Kept apart from `setup.ts` on purpose: `setup.ts` reaches `npm:fflate`
 * through `runtime_archive.ts`, so importing it in a fresh worktree fails at
 * module load, before `deno install` could run. `fdh task prepare` runs this
 * step first and only then loads `setup.ts` (W-05).
 *
 * This module must not import anything that resolves an npm package.
 */

import * as path from "@std/path";
import { PATHS } from "./defines.ts";
import { exists, runCommandChecked } from "./utils.ts";

export type PrepareStatus =
  | "already"
  | "prepared"
  | "removed"
  | "missing"
  | "ci-skip";

// `deno.json` uses nodeModulesDir = manual.

export interface PrepareDepsOptions {
  projectRoot?: string;
  nodeModulesDir?: string;
  /** Caller-recorded CI flag; falls back to a permission-safe `Deno.env` read. */
  isCI?: boolean;
  /** DI: returns the `deno install` exit code (tests pass a fake). */
  runInstall?: () => number;
}

export interface PrepareDepsResult {
  status: PrepareStatus;
  nodeModulesDir: string;
  /** Whether this call actually ran `deno install`. */
  installed: boolean;
  isCI: boolean;
}

function envFlag(name: string): boolean {
  try {
    return Deno.env.get(name) === "true";
  } catch {
    // Tests run without --allow-env; treat an unreadable flag as not-CI.
    return false;
  }
}

/** The npm package the host tests and runtime archive import (`fflate`). */
export function fflatePresent(nodeModulesDir: string): boolean {
  return exists(path.join(nodeModulesDir, "fflate", "package.json"));
}

export function prepareDeps(
  options: PrepareDepsOptions = {},
): PrepareDepsResult {
  const projectRoot = options.projectRoot ?? PATHS.root;
  const nodeModulesDir = options.nodeModulesDir ??
    path.join(projectRoot, "node_modules");
  const isCI = options.isCI ?? envFlag("CI");
  if (isCI) {
    return { status: "ci-skip", nodeModulesDir, installed: false, isCI };
  }
  if (fflatePresent(nodeModulesDir)) {
    return { status: "already", nodeModulesDir, installed: false, isCI };
  }
  const runInstall = options.runInstall ??
    (() => runCommandChecked(Deno.execPath(), ["install"], projectRoot).code);
  const code = runInstall();
  if (code !== 0) {
    throw new Error(`deno install failed with exit ${code}.`);
  }
  if (!fflatePresent(nodeModulesDir)) {
    throw new Error(
      `deno install completed but ${
        path.join(nodeModulesDir, "fflate")
      } is missing.`,
    );
  }
  return { status: "prepared", nodeModulesDir, installed: true, isCI };
}
