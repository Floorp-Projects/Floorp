// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  emptyLedger,
  type ExtLedger,
  extLedgerPath,
  finishExtOp,
  ingestObservation,
  planExt,
  planSync,
  readExtLedger,
  refreshOps,
  resolveField,
  startExtOp,
  upsertLink,
  withExtLedger,
} from "./fdh_ext.ts";
import { confirmJob, reconcileTaskJobs, resolveAndApprove } from "./fdh_job.ts";
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
const tick = () => new Promise((r) => setTimeout(r, 5));
const PR = "github:pr/3200";
const ISSUE = "jira:KAN-131";

function linked(): ExtLedger {
  const ledger = emptyLedger("t1");
  upsertLink(ledger, "KAN-131", 3200, "fdh/external-ops");
  return ledger;
}

/** Observe under the ledger lock, as `fdh ext observe` does. */
function observe(
  dir: string,
  ref: string,
  field: string,
  value: string | null,
  version: string,
  extra: { eventId?: string; deleted?: boolean } = {},
) {
  return withExtLedger(
    dir,
    "t1",
    (ledger) =>
      ingestObservation(ledger, { ref, field, value, version, ...extra }),
  );
}

/** The agent performs one planned action through `ext start` / `ext done`. */
async function perform(
  state: ReturnType<typeof taskIn>,
  field: string,
): Promise<{ key: string; to: string }> {
  const action = (await planExt(state.worktree, "t1")).actions.find((a) =>
    a.field === field
  );
  if (action === undefined) throw new Error(`no action for ${field}`);
  const op = await startExtOp(state, action.key, "agent-a");
  if (op.status !== "started") throw new Error(`not started: ${op.status}`);
  await finishExtOp(state, {
    key: action.key,
    epoch: op.epoch,
    agent: "agent-a",
    outcome: "succeeded",
  });
  return { key: action.key, to: action.to };
}

Deno.test("fdh ext: a repeated event or value is a duplicate, an older version is stale", () => {
  const ledger = linked();
  const pr = { ref: PR, field: "pr.state" };
  const take = (value: string, s: number, eventId?: string) =>
    ingestObservation(ledger, { ...pr, value, version: v(s), eventId });
  assertEquals(take("open", 10, "e1"), "applied");
  // The same event delivered twice, and the same value polled twice.
  assertEquals(take("open", 10, "e1"), "duplicate");
  assertEquals(take("open", 10), "duplicate");
  assertEquals(take("merged", 30, "e3"), "applied");
  // Ordering inversion: an older update arrives after the newer one.
  assertEquals(take("draft", 20, "e2"), "stale");
  assertEquals(ledger.observations[PR]["pr.state"].value, "merged");
  // The same version with another value is not taken.
  assertEquals(take("closed", 30), "conflict");
  assertEquals(ledger.observations[PR]["pr.state"].value, "merged");
  assertEquals(
    ledger.events.map((e) => e.kind),
    ["duplicate", "duplicate", "stale", "conflict"],
  );
  // One event id may carry several fields of the same object.
  assertEquals(
    ingestObservation(ledger, {
      ref: PR,
      field: "pr.url",
      value: "https://github.com/o/r/pull/3200",
      version: v(30),
      eventId: "e3",
    }),
    "applied",
  );
});

Deno.test("fdh ext: refs, fields, versions, links and ledgers are validated", async () => {
  const ledger = linked();
  const bad = (input: Parameters<typeof ingestObservation>[1], text: string) =>
    assertThrows(() => ingestObservation(ledger, input), Error, text);
  bad(
    { ref: "gh:3200", field: "pr.state", value: "x", version: v(1) },
    "invalid ref",
  );
  bad(
    { ref: ISSUE, field: "status", value: "x", version: "yesterday" },
    "ISO time",
  );
  bad(
    { ref: ISSUE, field: "titel", value: "x", version: v(1) },
    "unknown field",
  );
  bad(
    { ref: ISSUE, field: "status", value: "x", version: v(1), deleted: true },
    "no value",
  );
  assertThrows(
    () => upsertLink(ledger, "KAN-132", 3200),
    Error,
    "already linked to KAN-131",
  );
  assertThrows(
    () => upsertLink(ledger, "KAN-131", 3201),
    Error,
    "does not change",
  );
  assertThrows(() => extLedgerPath("/wt", "../../escape"), Error);
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    const file = extLedgerPath(dir, "t1");
    Deno.mkdirSync(`${dir}/_dist/fdh/ext`, { recursive: true });
    Deno.writeTextFileSync(file, "{not json");
    assertThrows(() => readExtLedger(dir, "t1"), Error, file);
    Deno.writeTextFileSync(
      file,
      JSON.stringify({ ...emptyLedger("t1"), observations: null }),
    );
    assertThrows(
      () => readExtLedger(dir, "t1"),
      Error,
      "invalid fdh ext ledger",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: start, write, done and a later poll keep the Jira status in step with the PR", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "open", v(10));
    await observe(dir, ISSUE, "status", "進行中", v(5));
    const first = await perform(state, "pr.state");
    assertEquals(first.to, "レビュー中");
    // The write is in Jira, but no poll has shown it yet.
    assertEquals(
      (await planExt(dir, "t1")).holds.map((h) => h.reason),
      ["awaiting-poll"],
    );
    // The poll after 'ext done' shows the write. Its Jira version (the
    // write's own time, v(20)) is earlier than 'ext done': that is normal.
    await tick();
    await observe(dir, ISSUE, "status", "レビュー中", v(20));
    assertEquals((await planExt(dir, "t1")).inSync.map((s) => s.field), [
      "pr.state",
    ]);
    // The PR merges: a new operation (its key carries the PR's version).
    await observe(dir, PR, "pr.state", "merged", v(40));
    const second = await perform(state, "pr.state");
    assertEquals(second.to, "完了");
    assert(second.key !== first.key);
    await tick();
    await observe(dir, ISSUE, "status", "完了", v(41));
    const done = await planExt(dir, "t1");
    assertEquals([done.actions, done.holds], [[], []]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a person's edit is kept, a concurrent change is held until resolved", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "draft", v(10));
    await observe(dir, ISSUE, "status", "未着手", v(5));
    await perform(state, "pr.state"); // 進行中
    await tick();
    await observe(dir, ISSUE, "status", "進行中", v(20));
    // A person moves the issue back to 未着手; the PR did not change.
    await observe(dir, ISSUE, "status", "未着手", v(30));
    const kept = await planExt(dir, "t1");
    assertEquals(kept.actions, []);
    assertEquals(kept.keptEdits.map((k) => k.value), ["未着手"]);
    // Now the PR moves on too: both sides changed since fdh's write.
    await observe(dir, PR, "pr.state", "open", v(40));
    const held = await planExt(dir, "t1");
    assertEquals(held.actions, []);
    assertEquals(held.holds.map((h) => h.reason), ["concurrent-edit"]);
    // The approver keeps the person's status: nothing is written.
    await withExtLedger(
      dir,
      "t1",
      (l) => resolveField(l, "KAN-131", "pr.state", "mirror"),
    );
    const settled = await planExt(dir, "t1");
    assertEquals([settled.actions, settled.holds], [[], []]);
    // Had they chosen the PR's side, the write would be planned again.
    await withExtLedger(
      dir,
      "t1",
      (l) => resolveField(l, "KAN-131", "pr.state", "owner"),
    );
    assertEquals((await planExt(dir, "t1")).actions.map((a) => a.to), [
      "レビュー中",
    ]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: no status write before Jira is polled, and a status never moves back", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: PR,
    field: "pr.state",
    value: "open",
    version: v(10),
  });
  assertEquals(planSync(ledger).actions, []);
  assertEquals(planSync(ledger).holds.map((h) => h.reason), [
    "mirror-unobserved",
  ]);
  ingestObservation(ledger, {
    ref: ISSUE,
    field: "status",
    value: "完了",
    version: v(11),
  });
  assertEquals(planSync(ledger).actions, []);
  assertEquals(planSync(ledger).keptEdits.map((k) => k.value), ["完了"]);
});

Deno.test("fdh ext: an operation resolved by an approver still records its baseline", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "open", v(10));
    await observe(dir, ISSUE, "status", "進行中", v(5));
    const [action] = (await planExt(dir, "t1")).actions;
    const op = await startExtOp(state, action.key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    // The agent dies after writing; the approver checks Jira and resolves.
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    assertEquals(readJob(dir, "t1", op.jobId).state, "awaiting-approval");
    await assertRejects(
      () => startExtOp(state, action.key, "agent-b"),
      Error,
      "in flight",
    );
    await resolveAndApprove(
      state,
      op.jobId,
      "succeeded",
      "checked in Jira",
      "approver",
    );
    assertEquals((await planExt(dir, "t1")).holds.map((h) => h.reason), [
      "awaiting-poll",
    ]);
    assertEquals(
      readExtLedger(dir, "t1").links["KAN-131"].synced["pr.state"].mirrorValue,
      "レビュー中",
    );
    await tick();
    await observe(dir, ISSUE, "status", "レビュー中", v(20));
    // A person then edits Jira while the PR merges: held, not overwritten.
    await observe(dir, ISSUE, "status", "未着手", v(30));
    await observe(dir, PR, "pr.state", "merged", v(40));
    assertEquals((await planExt(dir, "t1")).holds.map((h) => h.reason), [
      "concurrent-edit",
    ]);
    assertEquals(
      (await startExtOp(state, action.key, "agent-b")).status,
      "already-done",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a confirm whose ledger write was lost is recorded by the repeat or the next plan", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.url", "https://github.com/o/r/pull/3200", v(10));
    const [action] = (await planExt(dir, "t1")).actions;
    assertEquals(action.field, "pr.url");
    const op = await startExtOp(state, action.key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    assertEquals(
      readExtLedger(dir, "t1").ops[action.key].sync?.mirrorValue,
      action.to,
    );
    // The confirm lands, then the process dies before the ledger write.
    await confirmJob(state, op.jobId, op.epoch, "agent-a", "succeeded");
    const repeated = await finishExtOp(state, {
      key: action.key,
      epoch: op.epoch,
      agent: "agent-a",
      outcome: "succeeded",
    });
    assertEquals(repeated.outcome, "succeeded");
    assertEquals(
      readExtLedger(dir, "t1").links["KAN-131"].synced["pr.url"].mirrorValue,
      action.to,
    );
    await assertRejects(
      () =>
        finishExtOp(state, {
          key: action.key,
          epoch: op.epoch,
          agent: "agent-a",
          outcome: "failed",
        }),
      Error,
      "already ended succeeded",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a deleted object stops writes for it, does not come back, and an old deletion is stale", () => {
  const ledger = linked();
  ingestObservation(ledger, {
    ref: PR,
    field: "pr.url",
    value: "https://github.com/o/r/pull/3200",
    version: v(10),
  });
  ingestObservation(ledger, {
    ref: ISSUE,
    field: "status",
    value: "進行中",
    version: v(30),
  });
  assertEquals(
    ingestObservation(ledger, {
      ref: ISSUE,
      field: "object",
      value: null,
      version: v(20),
      deleted: true,
    }),
    "stale",
  );
  assertEquals(
    ingestObservation(ledger, {
      ref: ISSUE,
      field: "object",
      value: null,
      version: v(40),
      deleted: true,
    }),
    "deleted",
  );
  const plan = planSync(ledger);
  assertEquals(plan.actions, []);
  assertEquals(plan.holds.map((h) => [h.field, h.reason]), [[
    "pr.url",
    "deleted",
  ]]);
  assertEquals(
    ingestObservation(ledger, {
      ref: ISSUE,
      field: "object",
      value: "present",
      version: v(50),
    }),
    "conflict",
  );
  assert(ledger.observations[ISSUE].object.deleted);
});

Deno.test("fdh ext: a CI result is posted once per head and result, without waiting for a poll", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "ci.result", "abc1234:pending", v(10));
    assertEquals((await planExt(dir, "t1")).actions, []);
    await observe(dir, PR, "ci.result", "abc1234:failure", v(20));
    const failure = await perform(state, "ci.result");
    assertEquals(failure.key, "KAN-131:ci.result:abc1234:failure");
    // The comment is never polled back: the next head is still posted.
    await observe(dir, PR, "ci.result", "def5678:success", v(30));
    assertEquals((await planExt(dir, "t1")).actions.map((a) => a.to), [
      "def5678:success",
    ]);
    await perform(state, "ci.result");
    // The same head and result again is already posted.
    await observe(dir, PR, "ci.result", "abc1234:failure", v(40));
    assertEquals((await planExt(dir, "t1")).actions, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a keyed operation runs once, is refused while in flight, and may be retried after a failure", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const key = "github:pr-create:fdh/external-ops";
    const first = await startExtOp(state, key, "agent-a", "pr");
    if (first.status !== "started") throw new Error("unreachable");
    await assertRejects(
      () => startExtOp(state, key, "agent-b", "pr"),
      Error,
      "in flight",
    );
    await finishExtOp(state, {
      key,
      epoch: first.epoch,
      agent: "agent-a",
      outcome: "succeeded",
    });
    assertEquals(
      await startExtOp(state, key, "agent-b", "pr"),
      { status: "already-done", key, jobId: first.jobId },
    );
    const comment = "github:comment:3200:summary";
    const c1 = await startExtOp(state, comment, "agent-a");
    if (c1.status !== "started") throw new Error("unreachable");
    await finishExtOp(state, {
      key: comment,
      epoch: c1.epoch,
      agent: "agent-a",
      outcome: "failed",
    });
    const c2 = await startExtOp(state, comment, "agent-a");
    assert(c2.status === "started" && c2.jobId !== c1.jobId);
    // A wrong epoch is fenced like 'job confirm'.
    await assertRejects(
      () =>
        finishExtOp(state, {
          key: comment,
          epoch: 9,
          agent: "agent-a",
          outcome: "succeeded",
        }),
      Error,
      "confirm not applied",
    );
    // Two starts of one new key at once: one runs.
    const results = await Promise.allSettled(
      [1, 2, 3].map((i) =>
        startExtOp(state, "github:pr-create:other", `agent-${i}`, "pr")
      ),
    );
    assertEquals(results.filter((r) => r.status === "fulfilled").length, 1);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: an operation recorded without its intent (crash in between) may be started again", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (ledger) => {
      ledger.ops["k"] = {
        key: "k",
        jobId: "intent-20261111t090000z-abcdef",
        epoch: 1,
        startedAt: v(0),
        outcome: null,
        sync: null,
      };
    });
    await withExtLedger(dir, "t1", (ledger) => refreshOps(ledger, dir));
    assertEquals(readExtLedger(dir, "t1").ops["k"].outcome, "failed");
    assertEquals((await startExtOp(state, "k", "agent-a")).status, "started");
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
        (_, i) => observe(dir, `jira:KAN-${i + 1}`, "status", "進行中", v(i)),
      ),
    );
    assertEquals(Object.keys(readExtLedger(dir, "t1").observations).length, 8);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a PR re-poll between plan and start keeps the key, and a key outside the plan is refused", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "open", v(10));
    await observe(dir, ISSUE, "status", "進行中", v(5));
    const [planned] = (await planExt(dir, "t1")).actions;
    // A PR comment bumps updated_at; the value is the same.
    await observe(dir, PR, "pr.state", "open", v(15));
    const op = await startExtOp(state, planned.key, "agent-a");
    assertEquals(op.status, "started");
    assertEquals(
      readExtLedger(dir, "t1").ops[planned.key].sync?.mirrorValue,
      "レビュー中",
    );
    // While it is in flight the field is held, whatever the PR does.
    await observe(dir, PR, "pr.state", "merged", v(20));
    const held = await planExt(dir, "t1");
    assertEquals([held.actions, held.holds.map((h) => h.reason)], [[], [
      "op-in-flight",
    ]]);
    await assertRejects(
      () => startExtOp(state, "KAN-131:pr.state:完了#0", "agent-b"),
      Error,
      "not an action of the current plan (op-in-flight",
    );
    await assertRejects(
      () =>
        startExtOp(
          state,
          "KAN-131:pr.state:完了@2026-01-01T00:00:00Z",
          "agent-b",
        ),
      Error,
      "not an action of the current plan",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: the same value again after a change is a new operation, and resolve starts a new one", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "draft", v(10));
    await observe(dir, ISSUE, "status", "未着手", v(5));
    const a = await perform(state, "pr.state"); // 進行中
    await tick();
    await observe(dir, ISSUE, "status", "進行中", v(11));
    // A person moves it back while the PR stays a draft: kept.
    await observe(dir, ISSUE, "status", "未着手", v(20));
    assertEquals((await planExt(dir, "t1")).keptEdits.map((k) => k.value), [
      "未着手",
    ]);
    // The approver wants the PR's side after all: a new key, startable.
    await withExtLedger(
      dir,
      "t1",
      (l) => resolveField(l, "KAN-131", "pr.state", "owner"),
    );
    const b = await perform(state, "pr.state");
    assertEquals(b.to, "進行中");
    assert(b.key !== a.key);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: a late done is fenced by the job's epoch, and a deleted fdh comment does not stop CI comments", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "ci.result", "abc1234:failure", v(10));
    const [action] = (await planExt(dir, "t1")).actions;
    const op = await startExtOp(state, action.key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    await finishExtOp(state, {
      key: action.key,
      epoch: op.epoch,
      agent: "agent-a",
      outcome: "succeeded",
    });
    await assertRejects(
      () =>
        finishExtOp(state, {
          key: action.key,
          epoch: 99,
          agent: "agent-x",
          outcome: "succeeded",
        }),
      Error,
      "already ended succeeded",
    );
    await observe(dir, ISSUE, "ci-comment", null, v(20), { deleted: true });
    await observe(dir, PR, "ci.result", "def5678:success", v(30));
    assertEquals((await planExt(dir, "t1")).actions.map((a) => a.to), [
      "def5678:success",
    ]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext: an older operation resolved late does not replace a newer baseline", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-131", 3200));
    await observe(dir, PR, "pr.state", "open", v(10));
    await observe(dir, ISSUE, "status", "進行中", v(5));
    const [action] = (await planExt(dir, "t1")).actions;
    const op = await startExtOp(state, action.key, "agent-a");
    if (op.status !== "started") throw new Error("unreachable");
    // A newer write recorded meanwhile (by an operation started later).
    const later = new Date(Date.now() + 60_000).toISOString();
    await withExtLedger(dir, "t1", (l) => {
      l.links["KAN-131"].synced["pr.state"] = {
        ownerValue: "merged",
        mirrorValue: "完了",
        at: later,
        opStartedAt: later,
      };
    });
    // The old operation is confirmed only now.
    await confirmJob(state, op.jobId, op.epoch, "agent-a", "succeeded");
    await withExtLedger(dir, "t1", (l) => refreshOps(l, dir));
    const ledger = readExtLedger(dir, "t1");
    assertEquals(ledger.ops[action.key].outcome, "succeeded");
    assertEquals(
      ledger.links["KAN-131"].synced["pr.state"].mirrorValue,
      "完了",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
