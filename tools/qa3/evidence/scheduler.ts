// SPDX-License-Identifier: MPL-2.0
/**
 * Scheduler-side evidence intake (central side of ADR 8061068 1.2).
 *
 * The Scheduler always knows the current epoch and therefore rejects reports
 * from an older epoch (`stale-epoch`), and refuses reports entirely when the
 * current epoch is not known (`missing-current-epoch`). Everything else uses
 * the shared `validateManifest` rules and the shared fixture registry.
 */

import {
  type FixtureDecision,
  loadFixtureRegistry,
  manifestEpoch,
  readFixture,
  sharedFixturePaths,
} from "./fixtures.ts";
import { validateReportAsScheduler } from "./validate.ts";
import type { ValidationResult } from "./types.ts";

export interface SchedulerIntakeOptions {
  /** Central epoch. Required: the Scheduler always knows it. */
  currentEpoch?: number;
}

/** Central intake of one evidence report. */
export function validateReport(
  value: unknown,
  options: SchedulerIntakeOptions = {},
): ValidationResult {
  const errors = validateReportAsScheduler(value, options.currentEpoch);
  return { ok: errors.length === 0, errors };
}

/** The shared fixture list, as seen by the Scheduler. */
export function fixturePaths(): string[] {
  return sharedFixturePaths(loadFixtureRegistry());
}

/** Validate every shared fixture with central rules. */
export function validateSharedFixtures(): FixtureDecision[] {
  const registry = loadFixtureRegistry();
  const decisions: FixtureDecision[] = [];
  for (const relative of registry.valid) {
    const manifest = readFixture(relative);
    const result = validateReport(manifest, {
      currentEpoch: manifestEpoch(manifest),
    });
    decisions.push({
      path: relative,
      ok: result.ok,
      codes: result.errors.map((entry) => entry.code),
    });
  }
  for (const entry of registry.invalid) {
    const manifest = readFixture(entry.path);
    const currentEpoch = entry.currentEpoch ?? manifestEpoch(manifest);
    const result = validateReport(manifest, { currentEpoch });
    decisions.push({
      path: entry.path,
      ok: result.ok,
      codes: result.errors.map((value) => value.code),
    });
  }
  return decisions;
}
