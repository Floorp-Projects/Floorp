// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  approvePendingEntry,
  approveQuota,
  assertPendingExternalOp,
  assertQuotaStopReason,
  claimQuota,
  createQuotaLedger,
  parseQuotaLedger,
  pendingQuota,
  quotaLedgerExists,
  quotaLedgerPath,
  readQuotaLedger,
  releaseQuota,
  requireAgent,
  resumeQuota,
  stopQuota,
  summarizeQuota,
  writeQuotaLedger,
} from "./fdh_quota.ts";

const NOW = "2026-11-08T00:00:00.000Z";

function ledger() {
  return createQuotaLedger({
    taskId: "gate4-d",
    worktree: "/tmp/wt",
    now: () => NOW,
  });
}

Deno.test("fdh quota: a second agent cannot claim an actively assigned task", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  assertEquals(state.activeAgent, "agent-a");
  assertThrows(
    () => claimQuota(state, "agent-b", NOW),
    Error,
    "already assigned to agent-a",
  );
  // the same holder may re-claim (idempotent), and one event is recorded
  claimQuota(state, "agent-a", NOW);
  assertEquals(
    state.events.filter((event) => event.kind === "claim").length,
    2,
  );
});

Deno.test("fdh quota: stop records a reason and resume reopens the assignment", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  assertThrows(
    () => stopQuota(state, "agent-b", "quota", "not holder", NOW),
    Error,
    "does not hold",
  );
  stopQuota(state, "agent-a", "quota", "quota exhausted", NOW);
  assertEquals(state.stopped, true);
  assertEquals(state.stopReason, "quota");
  assertEquals(state.activeAgent, null);
  assertThrows(() => claimQuota(state, "agent-b", NOW), Error, "is stopped");
  resumeQuota(state, "agent-b", "resume after quota", NOW);
  assertEquals(state.stopped, false);
  assertEquals(state.stopReason, null);
  assertEquals(state.activeAgent, "agent-b");
  assertThrows(
    () => resumeQuota(state, "agent-b", "", NOW),
    Error,
    "is not stopped",
  );
});

Deno.test("fdh quota: a retried pending for the same op and note is not queued twice", () => {
  const state = ledger();
  assertEquals(pendingQuota(state, "push", "unknown remote state", NOW), true);
  const events = state.events.length;
  const later = "2026-11-08T00:05:00.000Z";
  assertEquals(
    pendingQuota(state, "push", "unknown remote state", later),
    false,
  );
  assertEquals(state.approvalPending, [
    { op: "push", note: "unknown remote state", at: NOW },
  ]);
  assertEquals(state.events.length, events);
  assertEquals(state.updatedAt, NOW);
  // Empty notes compare equal too.
  assertEquals(pendingQuota(state, "pr", "", NOW), true);
  assertEquals(pendingQuota(state, "pr", "", later), false);
  assertEquals(state.approvalPending.length, 2);
});

Deno.test("fdh quota: pending entries differing in op or note are both kept", () => {
  const state = ledger();
  assertEquals(pendingQuota(state, "push", "a", NOW), true);
  assertEquals(pendingQuota(state, "push", "b", NOW), true);
  assertEquals(pendingQuota(state, "pr", "a", NOW), true);
  assertEquals(
    state.approvalPending.map((entry) => `${entry.op}:${entry.note}`),
    ["push:a", "push:b", "pr:a"],
  );
});

Deno.test("fdh quota: the same op can be pending again after approval", () => {
  const state = ledger();
  pendingQuota(state, "push", "retry", NOW);
  approveQuota(state, "", NOW);
  assertEquals(pendingQuota(state, "push", "retry", NOW), true);
  assertEquals(state.approvalPending.length, 1);
});

Deno.test("fdh quota: option validators reject bad values before any ledger change", () => {
  assertPendingExternalOp("push");
  assertThrows(
    () => assertPendingExternalOp("force-push"),
    Error,
    "--op must be one of push|pr|release|external",
  );
  assertQuotaStopReason("quota");
  assertThrows(
    () => assertQuotaStopReason("bogus"),
    Error,
    "--reason must be one of quota|infra|manual",
  );
  assertEquals(requireAgent("  agent-a "), "agent-a");
  assertThrows(() => requireAgent("  "), Error, "--agent must be");
  // stopQuota still validates on its own.
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  assertThrows(
    () => stopQuota(state, "agent-a", "bogus" as never, "", NOW),
    Error,
    "--reason must be one of",
  );
  assertEquals(state.stopped, false);
});

Deno.test("fdh quota: pending external ops await approval and are cleared by approve", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  pendingQuota(state, "push", "unknown remote state", NOW);
  pendingQuota(state, "pr", "PR already open?", NOW);
  assertEquals(state.approvalPending.length, 2);
  assertEquals(state.approvalPending.map((entry) => entry.op), ["push", "pr"]);
  assertThrows(
    () => pendingQuota(state, "unknown" as never, "", NOW),
    Error,
    "--op must be one of",
  );
  approveQuota(state, "human approved", NOW);
  assertEquals(state.approvalPending, []);
  assertThrows(
    () => approveQuota(state, "", NOW),
    Error,
    "no pending approval",
  );
  assertEquals(
    state.events.map((event) => event.kind),
    ["claim", "pending", "pending", "approve"],
  );
});

Deno.test("fdh quota: release clears the holder without stopping", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  releaseQuota(state, "agent-a", NOW);
  assertEquals(state.activeAgent, null);
  assertEquals(state.stopped, false);
  assertThrows(
    () => releaseQuota(state, "agent-a", NOW),
    Error,
    "does not hold",
  );
});

Deno.test("fdh quota: ledger round-trips atomically and rejects invalid shape", async () => {
  const root = await Deno.makeTempDir({ prefix: "fdh-quota-" });
  try {
    const state = createQuotaLedger({
      taskId: "gate4-d",
      worktree: root,
      now: () => NOW,
    });
    claimQuota(state, "agent-a", NOW);
    const file = writeQuotaLedger(state);
    assertEquals(file, quotaLedgerPath(root, "gate4-d"));
    assertEquals(quotaLedgerExists(root, "gate4-d"), true);
    const readBack = readQuotaLedger(root, "gate4-d");
    assertEquals(readBack.activeAgent, "agent-a");
    assertEquals(readBack.events.length, 1);
    assertEquals(summarizeQuota(readBack).taskId, "gate4-d");

    assertThrows(
      () => parseQuotaLedger({ ...state, schemaVersion: 2 }),
      Error,
      "schemaVersion must be 1",
    );
    assertThrows(
      () => parseQuotaLedger({ ...state, stopReason: "unknown" }),
      Error,
      "stopReason is not admissible",
    );
    assertThrows(
      () => readQuotaLedger(root, "missing-task"),
      Error,
      "no fdh quota ledger",
    );
    const created = readQuotaLedger(root, "new-task", true);
    assertEquals(created.events, []);
    assertEquals(created.stopped, false);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("fdh quota: stop reasons and pending ops are constrained enums", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  for (const reason of ["quota", "infra", "manual"] as const) {
    stopQuota(state, "agent-a", reason, "", NOW);
    resumeQuota(state, "agent-a", "", NOW);
  }
  assertThrows(
    () => stopQuota(state, "agent-a", "other" as never, "", NOW),
    Error,
    "--reason must be one of",
  );
  assertEquals(
    state.events.filter((event) => event.kind === "stop").length,
    3,
  );
});

Deno.test("fdh quota: a pending entry with the optional empty note round-trips", async () => {
  const root = await Deno.makeTempDir({ prefix: "fdh-quota-pending-" });
  try {
    const state = createQuotaLedger({
      taskId: "gate4-d",
      worktree: root,
      now: () => NOW,
    });
    claimQuota(state, "agent-a", NOW);
    pendingQuota(state, "push", "", NOW);
    writeQuotaLedger(state);
    const readBack = readQuotaLedger(root, "gate4-d");
    assertEquals(readBack.approvalPending.length, 1);
    assertEquals(readBack.approvalPending[0].op, "push");
    assertEquals(readBack.approvalPending[0].note, "");
    assertEquals(readBack.events.at(-1)?.kind, "pending");
    // a resume over this ledger must still be readable (4-C approval-pending path)
    assertEquals(readBack.approvalPending[0].at, NOW);
  } finally {
    await Deno.remove(root, { recursive: true });
  }
});

Deno.test("fdh quota: parse constrains approvalPending ops and event shapes", () => {
  const state = ledger();
  claimQuota(state, "agent-a", NOW);
  pendingQuota(state, "push", "", NOW);
  assertThrows(
    () =>
      parseQuotaLedger({
        ...state,
        approvalPending: [{ op: "force-push", note: "", at: NOW }],
      }),
    Error,
    "approvalPending.op is not admissible",
  );
  assertThrows(
    () =>
      parseQuotaLedger({
        ...state,
        events: [{ at: NOW, kind: "bogus" }],
      }),
    Error,
    "events.kind is not admissible",
  );
  assertThrows(
    () =>
      parseQuotaLedger({
        ...state,
        events: [{ at: NOW, kind: "pending", op: "force-push" }],
      }),
    Error,
    "events.op is not admissible",
  );
  // a valid pending ledger parses
  assertEquals(parseQuotaLedger(state).approvalPending[0].note, "");
});

Deno.test("fdh quota: approvePendingEntry approves one matching entry only", () => {
  const state = ledger();
  pendingQuota(state, "push", "job a", NOW);
  pendingQuota(state, "push", "job b", NOW);
  assertEquals(
    approvePendingEntry(state, "push", "job a", "resolved", NOW),
    true,
  );
  assertEquals(state.approvalPending.map((p) => p.note), ["job b"]);
  assertEquals(approvePendingEntry(state, "push", "job a", "", NOW), false);
  assertEquals(state.events.at(-1)?.kind, "approve");
  assertEquals(parseQuotaLedger(state).approvalPending.length, 1);
});
