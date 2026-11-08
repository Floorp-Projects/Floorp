// SPDX-License-Identifier: MPL-2.0

/**
 * Build-phase selection for `fdh verify build` (FDH hardening IMP-1).
 *
 * The current CLI hardcodes `--phase after-mach`, which only rewrites the
 * XHTML in `_dist/bin` and never compiles TypeScript source. This module makes
 * the phase explicit and derives the ordered command specs so a broken source
 * fails closed before the injector runs.
 *
 * `buildVerifyCommands` returns the subset of `RecordedCommandSpec` that is
 * known without a task state (`command`/`cwd`/`stream` are supplied by the
 * caller). The type is imported from `fdh_task.ts`, which does not import this
 * module, so there is no cycle with the `fdh.ts` CLI.
 */

import type { RecordedCommandSpec } from "./fdh_task.ts";

export type BuildPhase = "all" | "before-mach" | "after-mach";

export const BUILD_PHASES: readonly BuildPhase[] = [
  "all",
  "before-mach",
  "after-mach",
];

/** Spec fields shared with `runRecordedCommand`; the caller adds command/cwd. */
export type BuildVerifyCommandSpec = Pick<
  RecordedCommandSpec,
  "displayCommand" | "args" | "assertionName" | "expected"
>;

const BEFORE_MACH: BuildVerifyCommandSpec = {
  displayCommand: "deno task feles-build build --phase before-mach",
  args: ["task", "feles-build", "build", "--phase", "before-mach"],
  // Keep the established name for the source-compiling command so evidence
  // manifests produced after IMP-1 mean "source compiled", not "injector ran".
  assertionName: "build-exit-code",
  expected: "exit code 0",
};

const AFTER_MACH: BuildVerifyCommandSpec = {
  displayCommand: "deno task feles-build build --phase after-mach",
  args: ["task", "feles-build", "build", "--phase", "after-mach"],
  assertionName: "build-inject-exit-code",
  expected: "exit code 0",
};

/**
 * Parse the optional `--phase` value for `verify build`.
 *
 * Omitted (`undefined`) selects the safe default `all` (before-mach then
 * after-mach). Any supplied value must be exactly one of BUILD_PHASES; an
 * empty or unknown value throws before a child command is spawned.
 */
export function parseBuildPhase(raw?: string): BuildPhase {
  if (raw === undefined) return "all";
  if (!isBuildPhase(raw)) {
    throw new Error(
      `Unknown build phase: ${JSON.stringify(raw)} (expected one of ${
        BUILD_PHASES.join("|")
      })`,
    );
  }
  return raw;
}

/** True when `raw` is one of the documented phases (exact match). */
export function isBuildPhase(raw: string): raw is BuildPhase {
  return BUILD_PHASES.includes(raw as BuildPhase);
}

/**
 * Ordered specs for one `verify build` run.
 *
 * - `all` (default): before-mach (source compile) then after-mach (inject).
 * - `before-mach`: source compile only; useful before a Runtime is installed.
 * - `after-mach`: legacy inject-only fast path; evidence records it as
 *   `build-inject-exit-code`, so the manifest can flag source as unverified.
 *
 * A fresh array is returned each call so callers cannot mutate shared state.
 */
export function buildVerifyCommands(
  phase: BuildPhase,
): BuildVerifyCommandSpec[] {
  switch (phase) {
    case "all":
      return [copySpec(BEFORE_MACH), copySpec(AFTER_MACH)];
    case "before-mach":
      return [copySpec(BEFORE_MACH)];
    case "after-mach":
      return [copySpec(AFTER_MACH)];
    default: {
      const unreachable: never = phase;
      throw new Error(`Unknown build phase: ${JSON.stringify(unreachable)}`);
    }
  }
}

/** Shallow-copy spec with a fresh args array so callers cannot mutate shared state. */
function copySpec(spec: BuildVerifyCommandSpec): BuildVerifyCommandSpec {
  return { ...spec, args: [...spec.args] };
}
