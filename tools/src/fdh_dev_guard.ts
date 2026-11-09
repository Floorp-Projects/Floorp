// SPDX-License-Identifier: MPL-2.0

/**
 * Guard for `fdh verify build` against a running dev browser (W-04).
 *
 * The before-mach phase rebuilds `bridge/*\/_dist`, which the running dev
 * browser loads through its `noraneko-devdir` mounts. On Windows native
 * (2026-11-09) this left Marionette accepting TCP connections while every
 * `NewSession` timed out until `dev down`/`dev up`; Gate 3 on macOS recorded
 * the same symptom as transient. The after-mach phase alone did not trigger
 * it. A source build therefore requires this worktree's dev processes to be
 * stopped; the inject-only phase stays allowed.
 *
 * Liveness reuses dev-tool's process registry and identity rule
 * (PID + start time + command line), so a stale registry entry or a reused PID
 * never blocks the build. A registry that exists but cannot be read or parsed
 * is a guard failure (fail closed), not "no dev processes".
 */

import * as path from "@std/path";
import { listDevProcesses } from "./dev_processes.ts";
import {
  matchesDevIdentity,
  parseDevProcessRegistry,
} from "./dev_process_selection.ts";
import type {
  DevProcessIdentity,
  DevProcessInfo,
} from "./dev_process_types.ts";
import type { BuildPhase } from "./fdh_verify.ts";

/** Dependency hooks so host tests run without real processes. */
export interface DevGuardDeps {
  readRegistry?: (file: string) => DevProcessIdentity[];
  listProcesses?: () => Promise<DevProcessInfo[]>;
}

/**
 * The guard decided to refuse. Any other error from the guard (for example a
 * failed process listing) means liveness could not be determined.
 */
export class DevGuardRefusalError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "DevGuardRefusalError";
  }
}

/** True when the phase runs the before-mach source build. */
export function phaseCompilesSource(phase: BuildPhase): boolean {
  return phase !== "after-mach";
}

/** dev-tool's per-checkout registry inside `worktree`. */
export function devProcessesFileFor(worktree: string): string {
  return path.join(worktree, "_dist", "dev-processes.json");
}

/**
 * Strict registry read for the guard. dev-tool's `readDevProcesses` logs and
 * returns `[]` for a corrupt or unreadable file, which would let a source
 * build run under a live dev browser. Here only a missing file means "no dev
 * processes"; anything else throws, so the guard fails closed.
 */
export function readDevRegistryStrict(file: string): DevProcessIdentity[] {
  try {
    return parseDevProcessRegistry(JSON.parse(Deno.readTextFileSync(file)));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return [];
    const reason = error instanceof Error ? error.message : String(error);
    throw new Error(
      `cannot read the dev process registry ${file}: ${reason}. ` +
        `Run 'fdh dev down' (dev-tool stop removes the registry once this ` +
        `worktree's dev processes are gone), then retry the build.`,
    );
  }
}

/** Registry entries of `worktree` whose process is still the same process. */
export async function liveDevProcesses(
  worktree: string,
  deps: DevGuardDeps = {},
): Promise<DevProcessIdentity[]> {
  const recorded = (deps.readRegistry ?? readDevRegistryStrict)(
    devProcessesFileFor(worktree),
  );
  // An empty registry needs no process snapshot (and no PowerShell/ps call).
  if (recorded.length === 0) return [];
  const live = await (deps.listProcesses ?? (() => listDevProcesses()))();
  return recorded.filter((identity) =>
    live.some((process) => matchesDevIdentity(process, identity))
  );
}

/**
 * Throws before any build command is spawned when `phase` compiles source and
 * this worktree's dev processes are running.
 */
export async function assertSourceBuildAllowed(
  worktree: string,
  phase: BuildPhase,
  deps: DevGuardDeps = {},
): Promise<void> {
  if (!phaseCompilesSource(phase)) return;
  const live = await liveDevProcesses(worktree, deps);
  if (live.length === 0) return;
  throw new DevGuardRefusalError(
    `verify build --phase ${phase} refused: ${live.length} dev process(es) of ` +
      `this worktree are running (PIDs ${
        live.map((entry) => entry.pid).join(", ")
      }). The before-mach source build under a live dev browser leaves ` +
      `Marionette unable to start new sessions until the dev environment ` +
      `restarts. Run 'fdh dev down' first, build, then 'fdh dev up'. ` +
      `(--phase after-mach only re-injects the XHTML and does not compile ` +
      `the source.)`,
  );
}
