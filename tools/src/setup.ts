// SPDX-License-Identifier: MPL-2.0

/**
 * Local setup preparation for the FDH loop (IMP-5).
 *
 * One worktree's `_dist/bin` and `_dist/profile/test` are expensive (~1.1 GiB)
 * and not self-contained: a copied runtime keeps absolute `noraneko-devdir`
 * symlinks and a copied profile keeps another worktree's sessionstore. This
 * module provides idempotent steps that a future `fdh task prepare` command can
 * compose: npm deps, Debug runtime tree, profile isolation and the stale Vite
 * dependency cache. It records a plain setup record for the task state owner;
 * it does not write task state itself.
 *
 * All steps are dependency-injected so host tests can run without a real
 * runtime registry, network, browser or `deno install`.
 */

import * as path from "@std/path";
import { BRANDING, PATHS } from "./defines.ts";
import { run as runInjector } from "./injector.ts";
import { copyDirectoryTreeSafely } from "./runtime_archive.ts";
import { exists, runCommandChecked, safeRemove } from "./utils.ts";

export const DEBUG_RUNTIME_MARKER_FILE = ".floorp-runtime-debug.json";
export const GUARD_ENV_NAME = "FLOORP_SKIP_PATCHER";

/** Marker written by the Debug FTP runtime install (`initializer.ts:142`). */
export interface RuntimeMarker {
  schemaVersion: 1;
  distribution: "debug";
  platform: "windows" | "linux" | "macos";
  architecture: "x86_64" | "aarch64" | "universal";
  artifactName: string;
  artifactSize: number;
  artifactSha256: string;
  version: string;
  buildId: string;
}

export interface RuntimeTarget {
  platform: RuntimeMarker["platform"];
  architecture: RuntimeMarker["architecture"];
}

export type PrepareStatus =
  | "already"
  | "prepared"
  | "removed"
  | "missing"
  | "ci-skip";

// ---------------------------------------------------------------------------
// 1. npm dependencies (`deno.json` nodeModulesDir = manual)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 2. Debug runtime tree (`_dist/bin`)
// ---------------------------------------------------------------------------

export interface PrepareRuntimeOptions {
  /** Registry path to a prepared `_dist/bin` tree (may contain devdir symlinks). */
  sourcePath?: string;
  binRootDir?: string;
  projectRoot?: string;
  /** DI copy hook: copies a runtime tree; tests may pass a fake. */
  copy?: (sourceRoot: string, destinationRoot: string) => Promise<CopyMode>;
  /** DI relink hook: creates relative `noraneko-devdir` symlinks. */
  relink?: (binRootDir: string, projectRoot: string) => void;
}

export type CopyMode = "safe-copy" | "symlink-fallback";

export interface PrepareRuntimeResult {
  status: PrepareStatus;
  binRootDir: string;
  sourcePath?: string;
  marker?: RuntimeMarker;
  copyMode?: CopyMode;
  relinked: boolean;
}

/**
 * Runtime directory inside a copied `_dist/bin` tree, i.e. the actual
 * `BIN_DIR` used by Injector/feles-build (on macOS the nested Resources dir).
 * The marker lives at the `_dist/bin` root, but the devdir symlinks live in
 * this nested directory.
 */
export function runtimeDirFor(binRootDir: string): string {
  const base = path.join(binRootDir, BRANDING.base_name);
  if (Deno.build.os !== "darwin") return base;
  return path.join(
    base,
    `${BRANDING.display_name}.app`,
    "Contents",
    "Resources",
  );
}

function defaultRelink(binRootDir: string, projectRoot: string): void {
  runInjector("dev", "noraneko-devdir", {
    binRootDir: runtimeDirFor(binRootDir),
    projectRoot,
  });
}

export function expectedRuntimeTarget(): RuntimeTarget {
  if (Deno.build.os === "darwin") {
    return { platform: "macos", architecture: "universal" };
  }
  if (Deno.build.os === "windows") {
    return { platform: "windows", architecture: "x86_64" };
  }
  return {
    platform: "linux",
    architecture: Deno.build.arch === "aarch64" ? "aarch64" : "x86_64",
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function requiredString(value: Record<string, unknown>, key: string): string {
  const field = value[key];
  if (typeof field !== "string" || field.length === 0) {
    throw new Error(
      `Runtime marker field '${key}' must be a non-empty string.`,
    );
  }
  return field;
}

/** Read + shape-validate the Debug runtime marker; null when absent/unreadable. */
export function readRuntimeMarker(binRootDir: string): RuntimeMarker | null {
  const markerPath = path.join(binRootDir, DEBUG_RUNTIME_MARKER_FILE);
  if (!exists(markerPath)) return null;
  try {
    const raw: unknown = JSON.parse(Deno.readTextFileSync(markerPath));
    if (!isRecord(raw)) return null;
    if (raw.schemaVersion !== 1) return null;
    if (raw.distribution !== "debug") return null;
    const platform = requiredString(raw, "platform");
    const architecture = requiredString(raw, "architecture");
    const size = raw.artifactSize;
    if (typeof size !== "number" || !Number.isFinite(size) || size <= 0) {
      return null;
    }
    return {
      schemaVersion: 1,
      distribution: "debug",
      platform: platform as RuntimeMarker["platform"],
      architecture: architecture as RuntimeMarker["architecture"],
      artifactName: requiredString(raw, "artifactName"),
      artifactSize: size,
      artifactSha256: requiredString(raw, "artifactSha256"),
      version: requiredString(raw, "version"),
      buildId: requiredString(raw, "buildId"),
    };
  } catch {
    return null;
  }
}

/** Validate the marker against this host's expected Debug runtime target. */
export function assertRuntimeMarker(
  marker: RuntimeMarker,
  target: RuntimeTarget = expectedRuntimeTarget(),
): RuntimeMarker {
  if (marker.platform !== target.platform) {
    throw new Error(
      `Runtime marker platform ${marker.platform} does not match ${target.platform}.`,
    );
  }
  if (marker.architecture !== target.architecture) {
    throw new Error(
      `Runtime marker architecture ${marker.architecture} does not match ${target.architecture}.`,
    );
  }
  return marker;
}

async function copyTreeWithSymlinks(
  sourceRoot: string,
  destinationRoot: string,
): Promise<void> {
  await Deno.mkdir(destinationRoot, { recursive: true });
  for await (const entry of Deno.readDir(sourceRoot)) {
    const source = path.join(sourceRoot, entry.name);
    const destination = path.join(destinationRoot, entry.name);
    const info = await Deno.lstat(source);
    if (info.isSymlink) {
      // Preserve the link text; a later Injector.run relinks devdir mounts.
      Deno.symlinkSync(await Deno.readLink(source), destination);
    } else if (info.isDirectory) {
      await copyTreeWithSymlinks(source, destination);
    } else if (info.isFile) {
      await Deno.copyFile(source, destination);
    } else {
      throw new Error(`Runtime copy rejects special entry: ${source}`);
    }
  }
}

/**
 * Copy a prepared runtime tree. `copyDirectoryTreeSafely` is the strict path,
 * but it rejects symlinks; a registry tree normally carries `noraneko-devdir`
 * links, so fall back to a symlink-preserving copy and relink afterwards.
 */
export async function copyRuntimeTree(
  sourceRoot: string,
  destinationRoot: string,
): Promise<CopyMode> {
  // copyDirectoryTreeSafely creates only the destination root; a `_dist/bin`
  // parent may not exist yet in a fresh worktree.
  await Deno.mkdir(path.dirname(destinationRoot), { recursive: true });
  try {
    await copyDirectoryTreeSafely(sourceRoot, destinationRoot);
    return "safe-copy";
  } catch (error) {
    if (
      !(error instanceof Error) ||
      !error.message.includes("special filesystem entry")
    ) {
      throw error;
    }
    safeRemove(destinationRoot);
    await copyTreeWithSymlinks(sourceRoot, destinationRoot);
    return "symlink-fallback";
  }
}

export async function prepareRuntime(
  options: PrepareRuntimeOptions = {},
): Promise<PrepareRuntimeResult> {
  const binRootDir = options.binRootDir ?? PATHS.bin_root;
  const projectRoot = options.projectRoot ?? PATHS.root;

  const existing = readRuntimeMarker(binRootDir);
  if (existing) {
    try {
      assertRuntimeMarker(existing);
      // Re-run the relink even when the tree is already present: a copied
      // runtime can carry stale absolute devdir links from another worktree.
      const relink = options.relink ?? defaultRelink;
      relink(binRootDir, projectRoot);
      return {
        status: "already",
        binRootDir,
        marker: existing,
        relinked: true,
      };
    } catch {
      // Wrong platform/architecture marker: replace the tree.
      safeRemove(binRootDir);
    }
  }

  const sourcePath = options.sourcePath;
  if (!sourcePath) {
    return { status: "missing", binRootDir, relinked: false };
  }
  if (!exists(sourcePath)) {
    throw new Error(`Runtime registry source is missing: ${sourcePath}.`);
  }

  safeRemove(binRootDir);
  const copy = options.copy ?? copyRuntimeTree;
  const copyMode = await copy(sourcePath, binRootDir);

  const marker = readRuntimeMarker(binRootDir);
  if (!marker) {
    throw new Error(
      `Copied runtime lacks ${DEBUG_RUNTIME_MARKER_FILE} under ${binRootDir}.`,
    );
  }
  assertRuntimeMarker(marker);

  const relink = options.relink ?? defaultRelink;
  relink(binRootDir, projectRoot);

  return {
    status: "prepared",
    binRootDir,
    sourcePath,
    marker,
    copyMode,
    relinked: true,
  };
}

// ---------------------------------------------------------------------------
// 3. Profile isolation (`_dist/profile/test` sessionstore)
// ---------------------------------------------------------------------------

export const SESSIONSTORE_FILE = "sessionstore.jsonlz4";
export const SESSIONSTORE_BACKUP_DIR = "sessionstore-backups";

export interface PrepareProfileOptions {
  profileDir?: string;
}

export interface PrepareProfileResult {
  status: PrepareStatus;
  profileDir: string;
  /** Paths removed by this call (empty when already clean). */
  removed: string[];
}

export function prepareProfile(
  options: PrepareProfileOptions = {},
): PrepareProfileResult {
  const profileDir = options.profileDir ?? PATHS.profile_test;
  const candidates = [
    path.join(profileDir, SESSIONSTORE_BACKUP_DIR),
    path.join(profileDir, SESSIONSTORE_FILE),
  ];
  const removed = candidates.filter((candidate) => exists(candidate));
  for (const candidate of removed) {
    safeRemove(candidate);
  }
  return {
    status: removed.length > 0 ? "removed" : "already",
    profileDir,
    removed,
  };
}

// ---------------------------------------------------------------------------
// 4. Vite dependency cache (`node_modules/.vite`)
// ---------------------------------------------------------------------------

export interface PrepareViteOptions {
  projectRoot?: string;
  cacheDir?: string;
  /** DI: true when a dev server still owns the cache. */
  devRunning?: boolean;
}

export interface PrepareViteResult {
  status: PrepareStatus;
  cacheDir: string;
  removed: boolean;
}

export function devServerPidFile(projectRoot: string): string {
  return path.join(projectRoot, "_dist", "dev-server.pid");
}

/** Conservative check: a PID file means the cache may be owned by a dev server. */
export function devServerRunning(projectRoot: string): boolean {
  return exists(devServerPidFile(projectRoot));
}

export function prepareVite(
  options: PrepareViteOptions = {},
): PrepareViteResult {
  const projectRoot = options.projectRoot ?? PATHS.root;
  const cacheDir = options.cacheDir ??
    path.join(projectRoot, "node_modules", ".vite");
  const devRunning = options.devRunning ?? devServerRunning(projectRoot);
  if (devRunning) {
    throw new Error("Dev server must be stopped before clearing .vite.");
  }
  if (!exists(cacheDir)) {
    return { status: "already", cacheDir, removed: false };
  }
  safeRemove(cacheDir);
  return { status: "removed", cacheDir, removed: true };
}

// ---------------------------------------------------------------------------
// 5. Setup record for the task state owner (`fdh_task.ts`)
// ---------------------------------------------------------------------------

export interface SetupRecord {
  schemaVersion: 1;
  /** Recorded so `fdh dev up` can skip Patcher without a manual export. */
  guardEnv: { [GUARD_ENV_NAME]: "1" };
  preparedAt: string;
  deps: PrepareDepsResult;
  runtime: PrepareRuntimeResult;
  profile: PrepareProfileResult;
  vite: PrepareViteResult;
}

export interface PrepareSetupOptions {
  projectRoot?: string;
  deps?: PrepareDepsOptions;
  runtime?: PrepareRuntimeOptions;
  profile?: PrepareProfileOptions;
  vite?: PrepareViteOptions;
  now?: () => string;
}

export function buildSetupRecord(
  results: {
    deps: PrepareDepsResult;
    runtime: PrepareRuntimeResult;
    profile: PrepareProfileResult;
    vite: PrepareViteResult;
  },
  now: () => string = () => new Date().toISOString(),
): SetupRecord {
  return {
    schemaVersion: 1,
    guardEnv: { [GUARD_ENV_NAME]: "1" },
    preparedAt: now(),
    deps: results.deps,
    runtime: results.runtime,
    profile: results.profile,
    vite: results.vite,
  };
}

/**
 * Merge a newer setup record into the previous one. Re-running an idempotent
 * prepare keeps the original `preparedAt` and a known registry sourcePath, so
 * the task state does not churn on every resume.
 */
export function mergeSetupRecord(
  previous: SetupRecord | undefined,
  current: SetupRecord,
): SetupRecord {
  if (!previous) return current;
  return {
    ...current,
    preparedAt: previous.preparedAt,
    runtime: {
      ...current.runtime,
      sourcePath: current.runtime.sourcePath ?? previous.runtime.sourcePath,
      copyMode: current.runtime.copyMode ?? previous.runtime.copyMode,
      marker: current.runtime.marker ?? previous.runtime.marker,
    },
  };
}

/** Attach the merged record to a task-state-like object (`state.setup`). */
export function recordSetup(
  state: { setup?: SetupRecord } & Record<string, unknown>,
  record: SetupRecord,
): typeof state {
  state.setup = mergeSetupRecord(state.setup, record);
  return state;
}

/** Compose the four idempotent steps and return the record for the caller. */
export async function prepareSetup(
  options: PrepareSetupOptions = {},
): Promise<SetupRecord> {
  const projectRoot = options.projectRoot ?? PATHS.root;
  const deps = prepareDeps({ projectRoot, ...options.deps });
  // Defaults are scoped to projectRoot so tests and callers can prepare a
  // worktree-shaped temp tree without touching the repository's `_dist`.
  const runtime = await prepareRuntime({
    projectRoot,
    binRootDir: path.join(projectRoot, "_dist", "bin"),
    ...options.runtime,
  });
  const profile = prepareProfile({
    profileDir: path.join(projectRoot, "_dist", "profile", "test"),
    ...options.profile,
  });
  const vite = prepareVite({ projectRoot, ...options.vite });
  return buildSetupRecord(
    { deps, runtime, profile, vite },
    options.now ?? (() => new Date().toISOString()),
  );
}
