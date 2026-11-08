// SPDX-License-Identifier: MPL-2.0

/**
 * CLI-side machinery for the `dev-tool start --port-offset <n|auto>` flag
 * (FDH Gate 2 task 2-B). Parsing/validation delegates to the shared table
 * helpers; `auto` probes the smallest admissible offset whose shifted port set
 * is completely free on this machine.
 */

import {
  DEV_PORT_NON_OVERLAP_SPAN,
  DEV_PORT_OFFSET_MAX,
  DEV_PORT_OFFSET_PREF,
  DEV_PORTS,
  devPortOffsetError,
  marionettePortForOffset,
} from "#libs/shared/dev-ports/defines.ts";

/** A parsed --port-offset argument: a validated number, or "auto". */
export type PortOffsetSpec = number | "auto";

/**
 * Parse the raw CLI value of `--port-offset`:
 * - omitted/empty → `0` (frozen base topology; identical to main);
 * - `"auto"` → `"auto"` (probe free offsets);
 * - a decimal string → the validated offset (integer, >= 0, inside the port
 *   range, and outside the non-overlap span so concurrent launch port sets
 *   stay disjoint);
 * - anything else throws a user-facing error.
 */
export function parsePortOffsetSpec(
  raw: string | number | undefined,
): PortOffsetSpec {
  if (raw === undefined || raw === "") {
    return 0;
  }
  if (raw === "auto") {
    return "auto";
  }
  if (typeof raw === "number" || /^\d+$/.test(raw.trim())) {
    const error = devPortOffsetError(raw);
    if (error === null) {
      return Number(String(raw).trim());
    }
    throw new Error(error);
  }
  throw new Error(`--port-offset must be <n|auto>, got ${JSON.stringify(raw)}`);
}

/**
 * Dependency-injected port probe: resolves `true` when something is already
 * listening on `port` on this machine.
 */
export type DevPortProbe = (port: number) => Promise<boolean>;

/** Default probe: a TCP connect succeeding means the port is in use. */
export async function isTcpPortInUse(port: number): Promise<boolean> {
  try {
    const conn = await Deno.connect({ port, transport: "tcp" });
    conn.close();
    return true;
  } catch {
    return false;
  }
}

/** All ports a launch with the given offset would bind (Vite + Marionette). */
export function launchPortsForOffset(offset: number): number[] {
  return Object.values(DEV_PORTS).map((base) => base + offset);
}

/**
 * Admissible candidate offsets for `auto`: `0` first (the frozen base
 * topology), then the non-overlap ladder (`span`, `2*span`, ...). Differences
 * between any two candidates are at least the table span, so candidate port
 * sets — Marionette included — are pairwise disjoint by design.
 */
export function autoPortOffsetCandidates(): number[] {
  const candidates: number[] = [0];
  const span = DEV_PORT_NON_OVERLAP_SPAN;
  for (let offset = span; offset <= DEV_PORT_OFFSET_MAX; offset += span) {
    candidates.push(offset);
  }
  return candidates;
}

async function busyPortAt(
  offset: number,
  probe: DevPortProbe,
): Promise<number | null> {
  for (const base of Object.values(DEV_PORTS)) {
    const port = base + offset;
    if (await probe(port)) {
      return port;
    }
  }
  return null;
}

/**
 * Resolve `--port-offset auto`: the first candidate offset whose entire
 * dev/test port set — Marionette included — is free on this machine. Throws
 * when no candidate fits.
 */
export async function resolveAutoDevPortOffset(
  probe: DevPortProbe = isTcpPortInUse,
): Promise<number> {
  for (const offset of autoPortOffsetCandidates()) {
    const busyPort = await busyPortAt(offset, probe);
    if (busyPort === null) {
      return offset;
    }
  }
  throw new Error("No free --port-offset auto candidate found");
}

/** Human-readable start-log description of the resolved launch topology. */
export function describeLaunchPorts(offset: number): string {
  return [
    `port-offset=${offset}`,
    `marionette=${marionettePortForOffset(offset)}`,
    `pref=${DEV_PORT_OFFSET_PREF}`,
  ].join(" ");
}
