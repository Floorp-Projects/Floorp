// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  emptyLedger,
  type ExtLedger,
  finishExtOp,
  ingestObservation,
  planSync,
  readExtLedger,
  startExtOp,
  upsertLink,
  withExtLedger,
} from "./fdh_ext.ts";
import { reconcileTaskJobs, resolveAndApprove } from "./fdh_job.ts";
import { readJob } from "./fdh_executor.ts";
import { createTaskState, readTaskState, writeTaskState } from "./fdh_task.ts";

function taskIn(dir: string) {
  const state = createTaskState({
    taskId: "t1",
    worktree: dir,
    baseCommit: "0123456789abcdef0123456789abcdef01234567",
    portOffset: 0,
  });
  writeTaskState(state);
  return readTaskState(dir, "t1");
}

const v = (s: number) =>
  new Date(Date.UTC(2026, 10, 11, 9, 0, s)).toISOString();

function linked(): ExtLedger {
  const ledger = emptyLedger("t1");
  upsertLink(ledger, "KAN-131", 3200, "fdh/external-ops");
  return ledger;
}

Deno.test("fdh ext: a repeated event or value is a duplicate, an older version is stale", () => {
  const ledger = linked();
  const pr = { ref: "github:pr/3200", field: "pr.state" };
  assertEquals(
    ingestObservation(ledger, {
      ...pr,
      value: "open",
      version: v(10),
      eventId: "e1",
    }),
    "applied",
  );
  // The same event delivered twice, and the same value polled twice.
  assertEquals(
    ingestObservation(ledger, {
      ...pr,
      value: "open",
      version: v(10),
      eventId: "e1",
    }),
    "duplicate",
  );
  assertEquals(
    ingestObservation(ledger, { ...pr, value: "open", version: v(10) }),
    "duplicate",
  );
  assertEquals(
    ingestObservation(ledger, {
      ...pr,
      value: "merged",
      version: v(30),
      eventId: "e3",
    }),
    "applied",
  );
  // Ordering inversion: an older update arrives after the newer one.
  assertEquals(
    ingestObservation(ledger, {
      ...pr,
      value: "draft",
      version: v(20),
      eventId: "e2",
    }),
    "stale",
  );
  assertEquals(
    ledger.observations["github:pr/3200"]["pr.state"].value,
    "merged",
  );
  // The same version with another value is not taken.
  assertEquals(
    ingestObservation(ledger, { ...pr, value: "closed", version: v(30) }),
    "conflict",
  );
  assertEquals(
    ledger.observations["github:pr/3200"]["pr.state"].value,
    "merged",
  );
  assertEquals(
    ledger.events.map((e) => e.kind),
    ["duplicate", "duplicate", "stale", "conflict"],
  );
});

Deno.test("fdh ext: refs, versions and links are validated", () => {
  const ledger = linked();
  assertThrows(
    () =>
      ingestObservation(ledger, {
        ref: "gh:3200",
        field: "f",
        value: "x",
        version: v(1),
      }),
    Error,
    "invalid ref",
  );
  assertThrows(
    () =>
      ingestObservation(ledger, {
        ref: "jira:KAN-1",
        field: "f",
        value: "x",
        version: "yesterday",
      }),
    Error,
    "ISO time",
  );
  assertThrows(
    () => upsertLink(ledger, "KAN-132", 3200),
    Error,
    "already linked to KAN-131",
  );
  assertThrows(
    () => upsertLink(ledger, "KAN-131", 3201),
    Error,
    "already linked to PR #3200",
  );
});

Deno.test("fdh ext: the plan moves the Jira status with the PR, and is in sync after the write is polled", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "pr.state",
    value: "open",
    version: v(10),
  });
  ingestObservation(ledger, {
    ref: "jira:KAN-131",
    field: "status",
    value: "進行中",
    version: v(5),
  });
  const first = planSync(ledger);
  const action = first.actions.find((a) => a.field === "pr.state")!;
  assertEquals(
    { ref: action.ref, from: action.from, to: action.to, key: action.key },
    {
      ref: "jira:KAN-131",
      from: "進行中",
      to: "レビュー中",
      key: "KAN-131:pr.state:レビュー中",
    },
  );
  // The agent performed it: fdh records what it wrote.
  ledger.links["KAN-131"].synced["pr.state"] = {
    ownerValue: "open",
    mirrorValue: "レビュー中",
    at: v(20),
  };
  // Until a poll shows the write, nothing is decided.
  assertEquals(planSync(ledger).holds.map((h) => h.reason), ["awaiting-poll"]);
  ingestObservation(ledger, {
    ref: "jira:KAN-131",
    field: "status",
    value: "レビュー中",
    version: v(21),
  });
  assertEquals(planSync(ledger).inSync, [{
    link: "KAN-131",
    field: "pr.state",
  }]);
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "pr.state",
    value: "merged",
    version: v(40),
  });
  assertEquals(planSync(ledger).actions.map((a) => a.to), ["完了"]);
});

Deno.test("fdh ext: a concurrent edit is held, a lone mirror edit is kept, and a status never moves back", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "pr.state",
    value: "open",
    version: v(10),
  });
  ledger.links["KAN-131"].synced["pr.state"] = {
    ownerValue: "open",
    mirrorValue: "レビュー中",
    at: v(20),
  };
  // A person moved the issue back to 進行中 after fdh's write.
  ingestObservation(ledger, {
    ref: "jira:KAN-131",
    field: "status",
    value: "進行中",
    version: v(30),
  });
  const kept = planSync(ledger);
  assertEquals(kept.actions, []);
  assertEquals(kept.keptEdits, [{
    link: "KAN-131",
    field: "pr.state",
    value: "進行中",
  }]);
  // Now the PR also changes: both sides moved since fdh's write.
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "pr.state",
    value: "merged",
    version: v(40),
  });
  const held = planSync(ledger);
  assertEquals(held.actions, []);
  assertEquals(held.holds.map((h) => h.reason), ["concurrent-edit"]);
  // An issue someone already closed is not reopened by a PR state.
  const closed = linked();
  ingestObservation(closed, {
    ref: "github:pr/3200",
    field: "pr.state",
    value: "open",
    version: v(10),
  });
  ingestObservation(closed, {
    ref: "jira:KAN-131",
    field: "status",
    value: "完了",
    version: v(11),
  });
  assertEquals(planSync(closed).actions, []);
  assertEquals(planSync(closed).keptEdits.map((k) => k.value), ["完了"]);
});

Deno.test("fdh ext: a deleted object stops writes for it, and does not come back", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "pr.url",
    value: "https://github.com/o/r/pull/3200",
    version: v(10),
  });
  ingestObservation(ledger, {
    ref: "jira:KAN-131",
    field: "object",
    value: null,
    version: v(12),
    deleted: true,
  });
  const plan = planSync(ledger);
  assertEquals(plan.actions, []);
  assertEquals(plan.holds.map((h) => [h.field, h.reason]), [[
    "pr.url",
    "deleted",
  ]]);
  assertEquals(
    ingestObservation(ledger, {
      ref: "jira:KAN-131",
      field: "object",
      value: "present",
      version: v(20),
    }),
    "conflict",
  );
  assert(ledger.observations["jira:KAN-131"].object.deleted);
});

Deno.test("fdh ext: a CI result is posted once per head and result, pending is not posted", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "ci.result",
    value: "abc1234:pending",
    version: v(10),
  });
  assertEquals(planSync(ledger).actions, []);
  ingestObservation(ledger, {
    ref: "github:pr/3200",
    field: "ci.result",
    value: "abc1234:failure",
    version: v(20),
  });
  const [post] = planSync(ledger).actions;
  assertEquals([post.mirrorField, post.to, post.retry], [
    "ci-comment",
    "abc1234:failure",
    "keyed-once",
  ]);
  assertEquals(post.key, "KAN-131:ci.result:abc1234:failure");
});

Deno.test("fdh ext: a keyed operation runs once, is refused while in flight, and may be retried after a failure", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const key = "github:pr-create:fdh/external-ops";
    const first = await startExtOp(state, key, "agent-a", "pr");
    assertEquals(first.status, "started");
    // A second agent (or a retry) while the first has no outcome: refused.
    await assertRejects(
      () => startExtOp(state, key, "agent-b", "pr"),
      Error,
      "in flight",
    );
    if (first.status !== "started") throw new Error("unreachable");
    await finishExtOp(state, {
      key,
      epoch: first.epoch,
      agent: "agent-a",
      outcome: "succeeded",
    });
    // Done: no second PR.
    const again = await startExtOp(state, key, "agent-b", "pr");
    assertEquals(again, { status: "already-done", key, jobId: first.jobId });
    // A failed operation may be started again (a new intent).
    const comment = "KAN-131:ci.result:abc1234:failure";
    const c1 = await startExtOp(state, comment, "agent-a");
    if (c1.status !== "started") throw new Error("unreachable");
    await finishExtOp(state, {
      key: comment,
      epoch: c1.epoch,
      agent: "agent-a",
      outcome: "failed",
    });
    const c2 = await startExtOp(state, comment, "agent-a");
    assertEquals(c2.status, "started");
    assert(c2.status === "started" && c2.jobId !== c1.jobId);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: an operation whose agent was lost waits for approval, and a resolved success counts as done", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const key = "jira:transition:KAN-131:完了";
    const op = await startExtOp(state, key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    // The agent died: its outcome is unknown, so it is not redone silently.
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    assertEquals(readJob(dir, "t1", op.jobId).state, "awaiting-approval");
    await assertRejects(
      () => startExtOp(state, key, "agent-b"),
      Error,
      "in flight",
    );
    // An approver checked Jira and found it done.
    await resolveAndApprove(
      state,
      op.jobId,
      "succeeded",
      "checked in Jira",
      "approver",
    );
    assertEquals(
      (await startExtOp(state, key, "agent-b")).status,
      "already-done",
    );
    assertEquals(readExtLedger(dir, "t1").ops[key].outcome, "succeeded");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: done records what fdh wrote, and an op in flight holds its field", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (ledger) => {
      upsertLink(ledger, "KAN-131", 3200);
      ingestObservation(ledger, {
        ref: "github:pr/3200",
        field: "pr.state",
        value: "open",
        version: v(10),
      });
    });
    const [action] = planSync(readExtLedger(dir, "t1")).actions;
    const op = await startExtOp(state, action.key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    assertEquals(
      planSync(readExtLedger(dir, "t1")).holds.map((h) => h.reason),
      ["op-in-flight"],
    );
    await finishExtOp(state, {
      key: action.key,
      epoch: op.epoch,
      agent: "agent-a",
      outcome: "succeeded",
      sync: {
        link: "KAN-131",
        field: "pr.state",
        ownerValue: "open",
        mirrorValue: action.to,
      },
    });
    const ledger = readExtLedger(dir, "t1");
    assertEquals(
      ledger.links["KAN-131"].synced["pr.state"].mirrorValue,
      "レビュー中",
    );
    // A wrong epoch is fenced like 'job confirm'.
    const other = await startExtOp(state, "k2", "agent-a");
    if (other.status !== "started") throw new Error("unreachable");
    await assertRejects(
      () =>
        finishExtOp(state, {
          key: "k2",
          epoch: 9,
          agent: "agent-a",
          outcome: "succeeded",
        }),
      Error,
      "confirm not applied",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: two writers do not lose each other's observations", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await Promise.all(
      Array.from(
        { length: 8 },
        (_, i) =>
          withExtLedger(dir, "t1", (ledger) =>
            ingestObservation(ledger, {
              ref: `jira:KAN-${i + 1}`,
              field: "status",
              value: "進行中",
              version: v(i),
            })),
      ),
    );
    assertEquals(Object.keys(readExtLedger(dir, "t1").observations).length, 8);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
