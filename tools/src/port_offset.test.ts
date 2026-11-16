// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";

import {
  autoPortOffsetCandidates,
  describeLaunchPorts,
  type DevPortProbe,
  launchMarionettePort,
  launchPortsForOffset,
  MARIONETTE_DEFAULT_PORT,
  parsePortOffsetSpec,
  resolveAutoDevPortOffset,
} from "./port_offset.ts";
import {
  DEV_PORT_NON_OVERLAP_SPAN,
  DEV_PORT_OFFSET_MAX,
} from "#libs/shared/dev-ports/defines.ts";

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

Deno.test("start log reports the Marionette port the launch listens on", () => {
  // Offset 0 writes no marionette.port pref, so the default 2828 applies.
  assertEquals(launchMarionettePort(0), MARIONETTE_DEFAULT_PORT);
  assertEquals(MARIONETTE_DEFAULT_PORT, 2828);
  assertEquals(
    describeLaunchPorts(0),
    "port-offset=0 marionette=2828 pref=nora.dev.port_offset",
  );
  // Non-zero offsets shift the 5180 base (Gate 2: offset 100 -> 5280).
  assertEquals(launchMarionettePort(100), 5280);
  assertEquals(
    describeLaunchPorts(100),
    "port-offset=100 marionette=5280 pref=nora.dev.port_offset",
  );
});

// ── Floorp OS-Server dev port (KAN-135) ────────────────────────

import {
  DEV_OS_SERVER_PORT,
  devOsServerBaseUrl,
  OS_SERVER_PRODUCTION_DEFAULT_PORT,
  osServerPortForOffset,
} from "./port_offset.ts";
import {
  DEV_PORT_MAX,
  DEV_PORT_MIN,
  DEV_PORTS,
  devPortOffsetsAreDisjoint,
  HUB_DEV_PAGES_PLUS_PROFILE,
  isTrustedDevPort,
} from "#libs/shared/dev-ports/defines.ts";

Deno.test("osServerPortForOffset validates via the shared offset error", () => {
  assertEquals(osServerPortForOffset(0), 5175);
  assertEquals(osServerPortForOffset(20), 5195);
  assertEquals(osServerPortForOffset(100), 5275);
  assertEquals(osServerPortForOffset(23088), 28263);
  // the largest admissible offset keeps the OS port inside the valid range
  assertEquals(osServerPortForOffset(DEV_PORT_OFFSET_MAX), 65518);
  // invalid offsets fail with the shared user-facing error, not a bogus port
  for (const bad of [Number.NaN, -1, 1.5, DEV_PORT_OFFSET_MAX + 1]) {
    assertThrows(() => osServerPortForOffset(bad));
  }
  assertThrows(
    () => osServerPortForOffset(1),
    Error,
    "non-overlap span",
  );
});

Deno.test(
  "dev OS port is an unused in-span gap, never a trusted page origin",
  () => {
    // inside the frozen dev table span, so existing offset invariants hold
    assertEquals(DEV_OS_SERVER_PORT >= DEV_PORT_MIN, true);
    assertEquals(DEV_OS_SERVER_PORT <= DEV_PORT_MAX, true);
    // it is a gap, not a table entry
    assertEquals(
      new Set<number>(Object.values(DEV_PORTS)).has(DEV_OS_SERVER_PORT),
      false,
    );
    // and it must never enter the dev page-origin trust lists
    assertEquals(
      isTrustedDevPort(DEV_OS_SERVER_PORT, HUB_DEV_PAGES_PLUS_PROFILE, 0),
      false,
    );
    assertEquals(
      isTrustedDevPort(
        DEV_OS_SERVER_PORT + 100,
        HUB_DEV_PAGES_PLUS_PROFILE,
        100,
      ),
      false,
    );
    // the packaged production default stays 58261 (dev band is 5175+offset;
    // only the hand-picked offset 53086 lands back on 58261, caught by the
    // launch probe set / OS-Server bind error, not silently shared)
    assertEquals(OS_SERVER_PRODUCTION_DEFAULT_PORT, 58261);
  },
);

Deno.test(
  "launchPortsForOffset appends the OS port without moving table indices",
  () => {
    const tableSize = Object.values(DEV_PORTS).length;
    const ports = launchPortsForOffset(0);
    assertEquals(ports.length, tableSize + 1);
    assertEquals(ports[0], 5173); // updateDevServer unchanged
    assertEquals(ports[2], 5179); // profileManager unchanged ([2] consumers)
    assertEquals(ports[tableSize], 5175); // OS port appended last
    assertEquals(launchPortsForOffset(100).at(-1), 5275);
  },
);

Deno.test(
  "admissible offsets give disjoint launch port sets, OS port included",
  () => {
    const offsets = [0, 20, 100, 23088, DEV_PORT_OFFSET_MAX];
    const portSets = offsets.map((offset) =>
      new Set(launchPortsForOffset(offset))
    );
    for (let i = 0; i < offsets.length; i++) {
      for (let j = i + 1; j < offsets.length; j++) {
        assertEquals(devPortOffsetsAreDisjoint(offsets[i], offsets[j]), true);
        for (const port of portSets[i]) {
          assertEquals(portSets[j].has(port), false);
        }
        // every OS port of both launches is a valid listening port
        for (const set of [portSets[i], portSets[j]]) {
          for (const port of set) {
            assertEquals(port >= 1024 && port <= 65535, true);
          }
        }
      }
    }
  },
);

Deno.test("auto skips an offset whose dev OS port is busy", async () => {
  // only the OS port of the base topology is occupied
  const offset = await resolveAutoDevPortOffset(probeFor([5175]));
  assertEquals(offset, DEV_PORT_NON_OVERLAP_SPAN);
});

Deno.test("devOsServerBaseUrl builds the loopback OS-Server URL", () => {
  assertEquals(devOsServerBaseUrl(0), "http://127.0.0.1:5175");
  assertEquals(devOsServerBaseUrl(100), "http://127.0.0.1:5275");
});
