// SPDX-License-Identifier: MPL-2.0
/**
 * Runner-side evidence self-check (ADR 8061068 1.2).
 *
 * The Runner validates a report it is about to write using the shared rules
 * and the shared fixture registry, but it never applies the central epoch: an
 * old Runner's external writes are not stopped by central rejection, and
 * external side effects are re-checked for ownership and approval by the
 * Connector instead.
 */

import {
  type FixtureDecision,
  loadFixtureRegistry,
  readFixture,
  sharedFixturePaths,
} from "./fixtures.ts";
import { validateManifest } from "./validate.ts";
import type { ValidationResult } from "./types.ts";

/** Runner-side self-check of one evidence report (no central epoch). */
export function validateReport(value: unknown): ValidationResult {
  const errors = validateManifest(value, { role: "runner" });
  return { ok: errors.length === 0, errors };
}

/** The shared fixture list, as seen by the Runner. */
export function fixturePaths(): string[] {
  return sharedFixturePaths(loadFixtureRegistry());
}

/** Validate every shared fixture with Runner rules. */
export function validateSharedFixtures(): FixtureDecision[] {
  const registry = loadFixtureRegistry();
  const decisions: FixtureDecision[] = [];
  for (const relative of registry.valid) {
    const manifest = readFixture(relative);
    const result = validateReport(manifest);
    decisions.push({
      path: relative,
      ok: result.ok,
      codes: result.errors.map((entry) => entry.code),
    });
  }
  for (const entry of registry.invalid) {
    const manifest = readFixture(entry.path);
    // The Runner never consults the central epoch: a stale report is still a
    // well-formed report as far as its own writes are concerned.
    const result = validateReport(manifest);
    decisions.push({
      path: entry.path,
      ok: result.ok,
      codes: result.errors.map((value) => value.code),
    });
  }
  return decisions;
}
