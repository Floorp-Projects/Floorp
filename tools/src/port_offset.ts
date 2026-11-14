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

// ── Floorp OS-Server port (KAN-135) ────────────────────────────

/**
 * Production/packaged default of the Floorp OS-Server listening port
 * (browser-features/modules/modules/os-server/server.sys.mts). Dev launches
 * avoid it at the default and ladder offsets because the dev profile pref
 * below points at the in-span dev port. One hand-picked exception exists:
 * offset 53086 lands back on 58261 (5175 + 53086). It is not silently
 * shared — the launch probe set includes the OS-Server port, so auto
 * offset resolution and the pre-launch guard reject the rung while a
 * listening installed Floorp holds it, and a direct launch fails at bind
 * time with the server's own port error instead of silently colliding.
 */
export const OS_SERVER_PRODUCTION_DEFAULT_PORT = 58261;

/** Launch-profile pref the OS-Server reads its listening port from. */
export const OS_SERVER_PORT_PREF = "floorp.os.server.port";

/**
 * Dev-only Floorp OS-Server port (KAN-135): an unused gap inside the frozen
 * dev table span [DEV_PORT_MIN, DEV_PORT_MAX].
 *
 * Deliberately NOT a `DEV_PORTS` entry: the OS server is a loopback JSON API,
 * not a trusted dev page origin, so it must not enter the
 * `isTrustedDevPort` allowlists (`HUB_DEV_PAGES*`). Being inside the span
 * keeps every existing offset invariant intact — two admissible launches
 * whose table ports (Vite + Marionette) are disjoint automatically have
 * disjoint OS-Server ports too, and `DEV_PORT_OFFSET_MAX` still bounds the
 * shifted port inside the valid range (5175 + 60343 = 65518 <= 65535).
 */
export const DEV_OS_SERVER_PORT = 5175;

/**
 * Validated Floorp OS-Server port for a dev launch with `offset`.
 * Validation reuses the shared dev-port-offset error so NaN, negative and
 * out-of-range offsets fail with the same user-facing message as elsewhere
 * instead of silently producing a bogus port.
 */
export function osServerPortForOffset(offset: number): number {
  const error = devPortOffsetError(offset);
  if (error !== null) {
    throw new Error(error);
  }
  return DEV_OS_SERVER_PORT + offset;
}

/** Loopback base URL of the dev Floorp OS-Server for `offset`. */
export function devOsServerBaseUrl(offset: number): string {
  return `http://127.0.0.1:${osServerPortForOffset(offset)}`;
}

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

/**
 * All ports a launch with the given offset would bind: the Vite/Marionette
 * table plus the dev-only Floorp OS-Server port (KAN-135). The OS port is
 * appended last so existing index-based consumers keep their table ports;
 * it is also what makes the auto-offset probe and the pre-launch guard
 * (tools/src/fdh_task.ts) check OS-Server port availability for free.
 */
export function launchPortsForOffset(offset: number): number[] {
  return [
    ...Object.values(DEV_PORTS),
    DEV_OS_SERVER_PORT,
  ].map((base) => base + offset);
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
  // launchPortsForOffset includes the dev-only Floorp OS-Server port, so the
  // auto offset resolution skips rungs whose OS-Server port is taken too.
  for (const port of launchPortsForOffset(offset)) {
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

/**
 * Marionette's own default port. Offset 0 writes no `marionette.port` pref
 * (dev_env_manager.ts), so the browser keeps this default.
 */
export const MARIONETTE_DEFAULT_PORT = 2828;

/** The Marionette port a launch with `offset` actually listens on. */
export function launchMarionettePort(offset: number): number {
  return offset > 0 ? marionettePortForOffset(offset) : MARIONETTE_DEFAULT_PORT;
}

/** Human-readable start-log description of the resolved launch topology. */
export function describeLaunchPorts(offset: number): string {
  return [
    `port-offset=${offset}`,
    `marionette=${launchMarionettePort(offset)}`,
    `pref=${DEV_PORT_OFFSET_PREF}`,
  ].join(" ");
}
