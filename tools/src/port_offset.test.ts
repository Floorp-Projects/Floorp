// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";

import {
  autoPortOffsetCandidates,
  type DevPortProbe,
  launchPortsForOffset,
  parsePortOffsetSpec,
  resolveAutoDevPortOffset,
} from "./port_offset.ts";
import { DEV_PORT_NON_OVERLAP_SPAN } from "#libs/shared/dev-ports/defines.ts";

// ── argument parsing ───────────────────────────────────────────

Deno.test("--port-offset omitted or empty defaults to 0", () => {
  assertEquals(parsePortOffsetSpec(undefined), 0);
  assertEquals(parsePortOffsetSpec(""), 0);
});

Deno.test("--port-offset auto stays the auto spec", () => {
  assertEquals(parsePortOffsetSpec("auto"), "auto");
});

Deno.test("--port-offset accepts admissible non-negative offsets", () => {
  assertEquals(parsePortOffsetSpec("0"), 0);
  assertEquals(parsePortOffsetSpec("100"), 100);
  assertEquals(
    parsePortOffsetSpec(String(DEV_PORT_NON_OVERLAP_SPAN)),
    DEV_PORT_NON_OVERLAP_SPAN,
  );
});

Deno.test("--port-offset rejects invalid values with a user-facing error", () => {
  for (const bad of ["-1", "1.5", "n100", "0x10", "auto2"]) {
    assertThrows(() => parsePortOffsetSpec(bad));
  }
  // the error is aimed at a CLI user
  assertThrows(
    () => parsePortOffsetSpec("-1"),
    Error,
    "--port-offset",
  );
  // manual offsets inside the non-overlap span are rejected: a launch shifted
  // by less than the span can share ports (Marionette included) with offset 0
  assertThrows(() => parsePortOffsetSpec("1"), Error, "non-overlap span");
});

// ── getPortFor (dev_server) threading ──────────────────────────

Deno.test("getPortFor shifts managed dev servers by the offset", async () => {
  const { getPortFor } = await import("./dev_server.ts");
  assertEquals(getPortFor("settings"), 5183);
  assertEquals(getPortFor("settings", 100), 5283);
  assertEquals(getPortFor("main", 100), 5281);
  assertEquals(getPortFor("profile-manager", 100), 5279);
  assertEquals(getPortFor("workflow-progress", 100), 5292);
  assertEquals(getPortFor("unknown-server"), 8080); // DEV_SERVER.default_port
  assertThrows(() => getPortFor("settings", 1), Error, "non-overlap span");
});

// ── auto resolution with an injected probe ─────────────────────

function probeFor(busyPorts: Iterable<number>): DevPortProbe {
  const busy = new Set(busyPorts);
  return (port: number) => Promise.resolve(busy.has(port));
}

Deno.test("auto prefers offset 0 when the base topology is free", async () => {
  assertEquals(await resolveAutoDevPortOffset(probeFor([])), 0);
});

Deno.test("auto skips a partially busy offset-0 topology", async () => {
  // a single busy table port blocks the zero offset…
  const busyBasePort = launchPortsForOffset(0)[2];
  const offset = await resolveAutoDevPortOffset(probeFor([busyBasePort]));
  // …and the next rung is a full span above it, keeping the port sets disjoint
  assertEquals(offset, DEV_PORT_NON_OVERLAP_SPAN);
});

Deno.test("auto skips every busy ladder rung", async () => {
  const busyPorts = [
    ...launchPortsForOffset(0),
    ...launchPortsForOffset(DEV_PORT_NON_OVERLAP_SPAN),
  ];
  const offset = await resolveAutoDevPortOffset(probeFor(busyPorts));
  assertEquals(offset, 2 * DEV_PORT_NON_OVERLAP_SPAN);
});

Deno.test("auto candidates are 0 plus the non-overlap ladder", () => {
  const candidates = autoPortOffsetCandidates();
  assertEquals(candidates[0], 0);
  assertEquals(candidates[1], DEV_PORT_NON_OVERLAP_SPAN);
  assertEquals(candidates[2], 2 * DEV_PORT_NON_OVERLAP_SPAN);
  // differences between any two candidates are >= the span, so candidate port
  // sets (Marionette included) are pairwise disjoint by design
  for (let i = 1; i < candidates.length; i++) {
    for (let j = i + 1; j < candidates.length; j++) {
      assertEquals(
        candidates[j] - candidates[i] >= DEV_PORT_NON_OVERLAP_SPAN,
        true,
      );
    }
  }
});
