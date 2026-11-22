// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  claimGuiLease,
  createGuiLease,
  endGuiLease,
  parseGuiLease,
  readGuiLease,
  summarizeGuiLease,
  writeGuiLease,
} from "./fdh_gui.ts";

const NOW = "2026-11-08T00:00:00.000Z";

Deno.test("fdh gui lease: one holder at a time and a second claim is rejected", () => {
  const lease = createGuiLease({ now: () => NOW });
  claimGuiLease(lease, "agent-a", "gate4-a0", "/tmp/wt0", NOW);
  assertEquals(lease.status, "active");
  assertEquals(lease.holder, "agent-a");
  assertEquals(summarizeGuiLease(lease).taskId, "gate4-a0");
  // the same holder may re-claim idempotently without another event
  claimGuiLease(lease, "agent-a", "gate4-a0", "/tmp/wt0", NOW);
  assertEquals(lease.events.length, 1);
  assertThrows(
    () => claimGuiLease(lease, "agent-b", "gate4-a1", "/tmp/wt1", NOW),
    Error,
    "GUI desktop lease is held by agent-a",
  );
});

Deno.test("fdh gui lease: only the holder may end or cancel", () => {
  const lease = createGuiLease({ now: () => NOW });
  claimGuiLease(lease, "agent-a", "gate4-a0", "/tmp/wt0", NOW);
  assertThrows(
    () => endGuiLease(lease, "agent-b", "ended", "", NOW),
    Error,
    "does not hold the GUI desktop lease",
  );
  endGuiLease(lease, "agent-a", "cancelled", "desktop reassigned", NOW);
  assertEquals(lease.status, "cancelled");
  assertEquals(lease.holder, null);
  assertEquals(lease.taskId, null);
  assertEquals(lease.endedAt, NOW);
  assertEquals(lease.events.map((event) => event.kind), ["claim", "cancel"]);
  // after release another agent can claim
  claimGuiLease(lease, "agent-b", "gate4-a1", "/tmp/wt1", NOW);
  assertEquals(lease.holder, "agent-b");
  assertThrows(
    () => endGuiLease(lease, "agent-b", "bogus" as never, "", NOW),
    Error,
    "--reason must be one of ended|cancelled",
  );
  endGuiLease(lease, "agent-b", "ended", "", NOW);
  assertEquals(lease.status, "ended");
  assertThrows(
    () => endGuiLease(lease, "agent-b", "ended", "", NOW),
    Error,
    "GUI desktop lease is not active",
  );
  assertEquals(lease.events.length, 4);
});

Deno.test("fdh gui lease: round-trips atomically and rejects invalid shape", async () => {
  const root = await Deno.makeTempDir({ prefix: "fdh-gui-" });
  try {
    const file = `${root}/fdh/gui-desktop-lease.json`;
    assertEquals(readGuiLease(file).status, "ended");
    const lease = createGuiLease({ now: () => NOW });
    claimGuiLease(lease, "agent-a", "gate4-a0", root, NOW);
    writeGuiLease(file, lease);
    const readBack = readGuiLease(file);
    assertEquals(readBack.holder, "agent-a");
    assertEquals(readBack.events.length, 1);
    assertEquals(summarizeGuiLease(readBack).status, "active");
    assertThrows(
      () => parseGuiLease({ ...lease, schemaVersion: 2 }),
      Error,
      "schemaVersion must be 1",
    );
    assertThrows(
      () => parseGuiLease({ ...lease, status: "bogus" }),
      Error,
      "status is not admissible",
    );
    assertThrows(
      () => parseGuiLease({ ...lease, events: [{ at: NOW, kind: "bogus" }] }),
      Error,
      "events.kind is not admissible",
    );
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});
