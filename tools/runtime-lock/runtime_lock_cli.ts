// SPDX-License-Identifier: MPL-2.0

import * as path from "@std/path";
import { PROJECT_ROOT } from "../src/defines.ts";
import {
  installLockedRuntime,
  isEnvironmentPermissionError,
  LOCKED_RUNTIME_GITHUB_TOKEN_ENV,
  resolveNativeRuntimeTarget,
  validateLockedRuntimeArtifact,
  validateLockedRuntimeReleaseMetadata,
} from "../src/initializer.ts";
import {
  loadRuntimeLock,
  RUNTIME_LOCK_PATH,
  type RuntimeLock,
} from "../src/runtime_lock.ts";

export type RuntimeLockCommand =
  | "install-native"
  | "validate-lock"
  | "validate-native"
  | "validate-release-metadata";

export interface RuntimeLockCliOptions {
  command: RuntimeLockCommand;
  lockPath: string | URL;
  out?: string;
}

const HELP = `
Usage:
  deno run -A tools/runtime-lock/runtime_lock_cli.ts validate-lock [--lock <path>]
  deno run -A tools/runtime-lock/runtime_lock_cli.ts validate-release-metadata [--lock <path>]
  deno run -A tools/runtime-lock/runtime_lock_cli.ts validate-native --out <directory> [--lock <path>]
  deno run -A tools/runtime-lock/runtime_lock_cli.ts install-native [--lock <path>]

Commands:
  validate-lock    Parse and strictly validate the canonical Runtime lock.
  validate-release-metadata
                   Validate live GitHub release identity and asset metadata.
  validate-native  Download, authenticate, extract, and inspect this host's artifact without installing it.
  install-native   Transactionally install this host's exact locked Runtime into _dist/bin.
`.trim();

function isRuntimeLockCommand(value: string): value is RuntimeLockCommand {
  return value === "install-native" || value === "validate-lock" ||
    value === "validate-native" || value === "validate-release-metadata";
}

function resolveValidationOutput(value: string): string {
  const output = path.resolve(value);
  const distRoot = path.join(PROJECT_ROOT, "_dist");
  const relative = path.relative(distRoot, output);
  if (
    !relative || relative === ".." || relative.startsWith("../") ||
    relative.startsWith("..\\") ||
    path.isAbsolute(relative)
  ) {
    throw new Error("validate-native --out must be a child of _dist.");
  }
  return output;
}

function requireOptionValue(
  option: "--lock" | "--out",
  value: string | undefined,
): string {
  if (!value || value.startsWith("-")) {
    throw new Error(`${option} requires a value.\n\n${HELP}`);
  }
  return value;
}

export function parseRuntimeLockCliArgs(args: string[]): RuntimeLockCliOptions {
  const command = args[0];
  if (!command || command === "--help" || command === "-h") {
    throw new Error(HELP);
  }
  if (!isRuntimeLockCommand(command)) {
    throw new Error(`Unknown Runtime lock command: ${command}\n\n${HELP}`);
  }

  let lockPath: string | URL = RUNTIME_LOCK_PATH;
  let out: string | undefined;
  for (let index = 1; index < args.length; index += 1) {
    const option = args[index];
    const value = args[index + 1];
    if (option === "--lock") {
      lockPath = path.resolve(requireOptionValue(option, value));
      index += 1;
      continue;
    }
    if (option === "--out") {
      out = resolveValidationOutput(requireOptionValue(option, value));
      index += 1;
      continue;
    }
    throw new Error(`Unknown or incomplete option: ${option}\n\n${HELP}`);
  }
  if (command === "validate-native" && !out) {
    throw new Error(`validate-native requires --out <directory>.\n\n${HELP}`);
  }
  if (command !== "validate-native" && out) {
    throw new Error(`${command} does not accept --out.\n\n${HELP}`);
  }
  return { command, lockPath, out };
}

function lockSummary(lock: RuntimeLock): Record<string, unknown> {
  return {
    repository: lock.source.repository,
    trackingRef: lock.source.trackingRef,
    ref: lock.source.ref,
    commit: lock.source.commit,
    tree: lock.source.tree,
    releaseId: lock.source.release.id,
    materials: lock.source.materials.count,
    materialBytes: lock.source.materials.totalBytes,
    tests: lock.source.tests.count,
    tasks: lock.source.tests.totalTasks,
    artifacts: lock.artifacts.map((artifact) => ({
      platform: artifact.platform,
      architecture: artifact.architecture,
      assetId: artifact.asset.id,
      assetName: artifact.asset.name,
      version: artifact.version,
      buildId: artifact.buildId,
    })),
  };
}

function readRuntimeGitHubToken(): string | undefined {
  try {
    const token = Deno.env.get(LOCKED_RUNTIME_GITHUB_TOKEN_ENV)?.trim();
    return token || undefined;
  } catch (error) {
    if (isEnvironmentPermissionError(error)) return undefined;
    throw error;
  }
}

export async function runRuntimeLockCli(args: string[]): Promise<void> {
  const options = parseRuntimeLockCliArgs(args);
  const lock = await loadRuntimeLock(options.lockPath);
  if (options.command === "validate-lock") {
    console.log(JSON.stringify(lockSummary(lock), null, 2));
    return;
  }
  if (options.command === "validate-release-metadata") {
    const release = await validateLockedRuntimeReleaseMetadata({
      lock,
      githubToken: readRuntimeGitHubToken(),
    });
    console.log(
      JSON.stringify(
        {
          status: "validated",
          releaseId: release.id,
          tagName: release.tagName,
          immutable: release.immutable,
          assets: release.assets.length,
        },
        null,
        2,
      ),
    );
    return;
  }

  const target = resolveNativeRuntimeTarget(Deno.build.os, Deno.build.arch);
  if (options.command === "validate-native") {
    const destinationRoot = options.out;
    if (!destinationRoot) {
      throw new Error("validate-native requires an output directory.");
    }
    const artifact = await validateLockedRuntimeArtifact({
      lock,
      target,
      destinationRoot,
    });
    console.log(
      JSON.stringify(
        {
          status: "validated",
          target,
          assetId: artifact.asset.id,
          assetName: artifact.asset.name,
          version: artifact.version,
          buildId: artifact.buildId,
          output: destinationRoot,
        },
        null,
        2,
      ),
    );
    return;
  }

  const result = await installLockedRuntime({ lock, target });
  console.log(
    JSON.stringify(
      {
        status: result.reused ? "already-installed" : "installed",
        target,
        assetId: result.artifact.asset.id,
        assetName: result.artifact.asset.name,
        version: result.artifact.version,
        buildId: result.artifact.buildId,
        backupPath: result.backupPath ?? null,
        controlBackupPath: result.controlBackupPath ?? null,
      },
      null,
      2,
    ),
  );
}

if (import.meta.main) {
  try {
    await runRuntimeLockCli(Deno.args);
  } catch (error) {
    console.error(error instanceof Error ? error.message : String(error));
    Deno.exit(1);
  }
}
