// SPDX-License-Identifier: MPL-2.0
/**
 * Shared EvidenceManifest fixture registry (ADR 8061068 section 1.5).
 *
 * The Scheduler (`scheduler.ts`) and the Runner (`runner.ts`) both load the
 * exact same files through this module, so a fixture that one side accepts or
 * rejects is decided by the registry and not by either component.
 */

export interface FixtureInvalidEntry {
  path: string;
  code: string;
  /** Current central epoch to validate this fixture with (stale-epoch case). */
  currentEpoch?: number;
  /** ADR 1.2: an old Runner keeps its external writes; only the centre rejects. */
  runnerAccepts?: boolean;
}

export interface FixtureRegistry {
  schemaVersion: number;
  description: string;
  valid: string[];
  invalid: FixtureInvalidEntry[];
}

export interface FixtureDecision {
  path: string;
  ok: boolean;
  codes: string[];
}

const REGISTRY_URL = new URL("./fixtures/registry.json", import.meta.url);
const FIXTURES_DIR = new URL("./fixtures/", import.meta.url);

export function loadFixtureRegistry(): FixtureRegistry {
  const registry = JSON.parse(
    Deno.readTextFileSync(REGISTRY_URL),
  ) as FixtureRegistry;
  if (registry.schemaVersion !== 1) {
    throw new Error(
      `unsupported fixture registry schemaVersion ${registry.schemaVersion}`,
    );
  }
  return registry;
}

/** Read one fixture (path relative to `fixtures/`) as parsed JSON. */
export function readFixture(relative: string): unknown {
  return JSON.parse(Deno.readTextFileSync(new URL(relative, FIXTURES_DIR)));
}

/** The one shared fixture list: every valid fixture followed by every invalid one. */
export function sharedFixturePaths(
  registry: FixtureRegistry = loadFixtureRegistry(),
): string[] {
  return [...registry.valid, ...registry.invalid.map((entry) => entry.path)];
}

/** Epoch of a manifest, used as the central epoch for non-epoch fixtures. */
export function manifestEpoch(value: unknown): number {
  const epoch = (value as { envelope?: { epoch?: unknown } } | null)?.envelope
    ?.epoch;
  return typeof epoch === "number" ? epoch : 0;
}
