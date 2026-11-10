// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertNotEquals,
  assertRejects,
} from "@std/assert";
import {
  cancelAndSettle,
  currentInputs,
  hashUntracked,
  lostTreeBound,
  quotaNoteFor,
  reconcileTaskJobs,
  resolveAndApprove,
  respawnCompilesSource,
  settleJob,
  startIntent,
  startStepJob,
  stepSpecFor,
  waitForJob,
} from "./fdh_job.ts";
import { selectProcessTree, startedNoEarlier } from "./fdh_job_process.ts";
import {
  applyReport,
  beginAttempt,
  readJob,
  reconcileJob,
  recordStepProcess,
  updateJob,
} from "./fdh_executor.ts";
import { readQuotaLedger } from "./fdh_quota.ts";
import {
  createTaskState,
  type GitRunner,
  readTaskState,
  writeTaskState,
} from "./fdh_task.ts";
import type { DevProcessInfo } from "./dev_process_types.ts";

/** A git stand-in: HEAD, diff and untracked names come from `repo`. */
function fakeGit(repo: { head: string; diff: string; untracked: string }) {
  const git: GitRunner = (_cwd, args) => {
    if (args[0] === "rev-parse") return Promise.resolve(repo.head);
    if (args[0] === "diff") return Promise.resolve(repo.diff);
    if (args[0] === "ls-files") return Promise.resolve(repo.untracked);
    return Promise.reject(new Error(`unexpected git ${args.join(" ")}`));
  };
  return git;
}

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

Deno.test("fdh job: step specs run the same commands as verify build|test", () => {
  const test = stepSpecFor("test", "/wt");
  assertEquals(test.commands.map((c) => c.argv), [[
    "deno",
    "task",
    "test:host",
  ]]);
  assertEquals(test.commands[0].assertionName, "host-test-exit-code");
  const build = stepSpecFor("build", "/wt", "all");
  assertEquals(build.commands.map((c) => c.assertionName), [
    "build-exit-code",
    "build-inject-exit-code",
  ]);
  assertEquals(build.commands[0].argv.slice(0, 3), [
    "deno",
    "task",
    "feles-build",
  ]);
  assertEquals(stepSpecFor("build", "/wt", "after-mach").commands.length, 1);
});

Deno.test("fdh job: the inputs bind HEAD, guard env and the uncommitted state", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = await taskIn(dir);
    const repo = { head: "h1", diff: "", untracked: "" };
    const a = await currentInputs(state, fakeGit(repo));
    assertEquals(a.head, "h1");
    assertEquals(a.baseCommit, "0123456789abcdef0123456789abcdef01234567");
    repo.untracked = "new-file.txt";
    const b = await currentInputs(state, fakeGit(repo));
    assertNotEquals(a.worktreeState, b.worktreeState);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a lost executor's step tree is stopped before the next epoch is spawned", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = await taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const spawned: number[] = [];
    const spawn = (_w: string, _t: string, _j: string, epoch: number) => {
      spawned.push(epoch);
      return 1000 + epoch;
    };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn,
    });
    assertEquals(spawned, [1]);
    const old = new Date(Date.now() - 60_000).toISOString();
    const root = {
      pid: 77,
      startTime: "s",
      commandLine: "deno task test:host",
    };
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", root);
    });
    const stoppedRoots: number[] = [];
    const reports = await reconcileTaskJobs({
      state,
      git,
      spawn,
      stopTree: (r) => {
        stoppedRoots.push(r.pid);
        return Promise.resolve([{ ...r, ppid: 1, name: "deno" }]);
      },
    });
    assertEquals(reports[0].action, "respawn");
    assertEquals(reports[0].stopped, 1);
    assertEquals(stoppedRoots, [77]);
    assertEquals(spawned, [1, 2]);
    const after = readJob(dir, "t1", job.jobId);
    assert(
      after.events.some((e) => e.kind === "lost-step-stopped" && e.epoch === 1),
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: changed inputs fail a lost job instead of respawning it", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = await taskIn(dir);
    const repo = { head: "h", diff: "", untracked: "" };
    const spawned: number[] = [];
    const spawn = (_w: string, _t: string, _j: string, epoch: number) => {
      spawned.push(epoch);
      return 1;
    };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git: fakeGit(repo),
      spawn,
    });
    const old = new Date(Date.now() - 60_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => beginAttempt(j, 1, "x1", old));
    repo.diff = "edited";
    const [report] = await reconcileTaskJobs({
      state,
      git: fakeGit(repo),
      spawn,
    });
    assertEquals(report.action, "input-changed");
    assertEquals(spawned, [1]);
    // The failure is copied into the task state once.
    const events = readTaskState(dir, "t1").events.filter((e) =>
      e.command === `job result ${job.jobId}`
    );
    assertEquals(events.length, 1);
    assertEquals(readJob(dir, "t1", job.jobId).recordedToTask, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a terminal result is recorded in the task state exactly once", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = await taskIn(dir);
    const job = await startStepJob({
      state,
      kind: "test",
      git: fakeGit({ head: "h", diff: "", untracked: "" }),
      spawn: () => 1,
    });
    const logRef = `_dist/fdh/jobs/t1/${job.jobId}-1-0.log`;
    Deno.writeTextFileSync(`${dir}/${logRef}`, "ok | 3 passed | 0 failed\n");
    const now = new Date().toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", now);
      applyReport(j, {
        jobId: job.jobId,
        epoch: 1,
        holder: "x1",
        exitCode: 0,
        endedAt: now,
        logRef,
        results: [{
          index: 0,
          epoch: 1,
          exitCode: 0,
          startedAt: now,
          endedAt: now,
          logRef,
        }],
      }, now);
    });
    assertEquals(await settleJob(dir, "t1", job.jobId), true);
    assertEquals(await settleJob(dir, "t1", job.jobId), false);
    const task = readTaskState(dir, "t1");
    assertEquals(task.operations.length, 1);
    assert(task.operations[0].output.includes("3 passed"));
    assertEquals(task.assertions.map((a) => [a.name, a.passed]), [[
      "host-test-exit-code",
      true,
    ]]);
    // A crash after the task-state write but before the journal mark does
    // not duplicate the record.
    await updateJob(dir, "t1", job.jobId, (j) => {
      j.recordedToTask = false;
    });
    await settleJob(dir, "t1", job.jobId);
    assertEquals(readTaskState(dir, "t1").operations.length, 1);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a lost intent goes to the quota ledger and its resolution approves it", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = await taskIn(dir);
    const job = await startIntent(state, "push", "origin b", "agent-a");
    const reports = await reconcileTaskJobs({ state, intentHoldersGone: true });
    assertEquals(reports[0].action, "awaiting-approval");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    const ledger = readQuotaLedger(dir, "t1");
    assertEquals(ledger.approvalPending.map((p) => [p.op, p.note]), [[
      "push",
      quotaNoteFor(readJob(dir, "t1", job.jobId)),
    ]]);
    const resolved = await resolveAndApprove(
      state,
      job.jobId,
      "succeeded",
      "seen on origin",
    );
    assertEquals(resolved.state, "succeeded");
    assertEquals(readQuotaLedger(dir, "t1").approvalPending, []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: the process tree follows parent pids, also after the root exited", () => {
  const p = (pid: number, ppid: number, startTime: string): DevProcessInfo => ({
    pid,
    ppid,
    name: "deno",
    commandLine: `cmd ${pid}`,
    startTime,
  });
  const root = {
    pid: 10,
    startTime: "2026-11-09T10:00:00.000Z",
    commandLine: "cmd 10",
  };
  const snapshot = [
    p(10, 1, root.startTime),
    p(11, 10, "2026-11-09T10:00:01.000Z"),
    p(12, 11, "2026-11-09T10:00:02.000Z"),
    // An older process whose parent pid happens to be 10 (pid reuse): not ours.
    p(13, 10, "2026-11-09T09:00:00.000Z"),
    p(14, 1, "2026-11-09T10:00:03.000Z"),
  ];
  assertEquals(selectProcessTree(snapshot, root).map((x) => x.pid), [
    10,
    11,
    12,
  ]);
  // Root gone: its children are found by parent pid and start time, but
  // only those that started before the attempt was declared lost.
  const gone = snapshot.filter((x) => x.pid !== 10);
  assertEquals(selectProcessTree(gone, root), []);
  assertEquals(
    selectProcessTree(gone, root, "2026-11-09T10:00:05.000Z").map((x) => x.pid),
    [11, 12],
  );
  // A different process now holds pid 10: it is not selected as the root.
  const reused = [p(10, 1, "2026-11-09T11:00:00.000Z")];
  assertEquals(selectProcessTree(reused, root), []);
});

// ---------------------------------------------------------------------------
// review round 1 on PR #3097 (reviewer A)
// ---------------------------------------------------------------------------

Deno.test("fdh job: a lost step tree is also stopped when the job closes without a respawn", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const repo = { head: "h", diff: "", untracked: "" };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git: fakeGit(repo),
      spawn: () => 1,
    });
    const old = new Date(Date.now() - 60_000).toISOString();
    const root = {
      pid: 88,
      startTime: "s",
      commandLine: "deno task test:host",
    };
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", root);
    });
    repo.diff = "edited";
    const stopped: number[] = [];
    const [report] = await reconcileTaskJobs({
      state,
      git: fakeGit(repo),
      spawn: () => 1,
      stopTree: (r) => {
        stopped.push(r.pid);
        return Promise.resolve([]);
      },
    });
    assertEquals(report.action, "input-changed");
    assertEquals(stopped, [88]);
    assert(
      readJob(dir, "t1", job.jobId).events.some((e) =>
        e.kind === "lost-step-stopped"
      ),
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: concurrent settles record a result once", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const job = await startStepJob({
      state,
      kind: "test",
      git: fakeGit({ head: "h", diff: "", untracked: "" }),
      spawn: () => 1,
    });
    const now = new Date().toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", now);
      applyReport(j, {
        jobId: job.jobId,
        epoch: 1,
        holder: "x1",
        exitCode: 0,
        endedAt: now,
        logRef: null,
        results: [{
          index: 0,
          epoch: 1,
          exitCode: 0,
          startedAt: now,
          endedAt: now,
          logRef: null,
        }],
      }, now);
    });
    const results = await Promise.all(
      Array.from({ length: 6 }, () => settleJob(dir, "t1", job.jobId)),
    );
    assertEquals(results.filter(Boolean).length, 1);
    const task = readTaskState(dir, "t1");
    assertEquals(task.operations.length, 1);
    assertEquals(
      task.events.filter((e) => e.command === `job result ${job.jobId}`).length,
      1,
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: children of a process that reused the root pid are never selected", () => {
  const p = (pid: number, ppid: number, startTime: string): DevProcessInfo => ({
    pid,
    ppid,
    name: "x",
    commandLine: `cmd ${pid}`,
    startTime,
  });
  const root = {
    pid: 10,
    startTime: "2026-11-09T10:00:00.000Z",
    commandLine: "cmd 10",
  };
  // pid 10 now belongs to another process, whose child started later.
  const snapshot = [
    { ...p(10, 1, "2026-11-09T11:00:00.000Z"), commandLine: "other" },
    p(20, 10, "2026-11-09T11:00:01.000Z"),
  ];
  assertEquals(selectProcessTree(snapshot, root), []);
});

Deno.test("fdh job: Linux start ticks compare numerically within one boot", () => {
  assert(startedNoEarlier("boot:100000", "boot:99999"));
  assert(!startedNoEarlier("boot:99999", "boot:100000"));
  assert(!startedNoEarlier("other:100000", "boot:99999"));
  assert(
    startedNoEarlier("2026-11-09T10:00:01.000Z", "2026-11-09T10:00:00.000Z"),
  );
  assert(!startedNoEarlier("garbage", "2026-11-09T10:00:00.000Z"));
});

// ---------------------------------------------------------------------------
// review round 2 on PR #3097 (reviewer B)
// ---------------------------------------------------------------------------

Deno.test("fdh job: an interrupted respawn still gets the older lost tree stopped", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const spawned: number[] = [];
    const spawn = (_w: string, _t: string, _j: string, epoch: number) => {
      spawned.push(epoch);
      return 1;
    };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn,
    });
    const old = new Date(Date.now() - 120_000).toISOString();
    const older = new Date(Date.now() - 60_000).toISOString();
    const root = {
      pid: 91,
      startTime: "s",
      commandLine: "deno task test:host",
    };
    // Epoch 1 ran and was lost; a controller bumped to epoch 2 and died
    // before it stopped epoch 1's tree or spawned epoch 2.
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", root);
      reconcileJob(j, { currentDigest: j.executionDigest, at: older });
    });
    assertEquals(readJob(dir, "t1", job.jobId).epoch, 2);
    const stopped: number[] = [];
    const [report] = await reconcileTaskJobs({
      state,
      git,
      spawn,
      stopTree: (r) => {
        stopped.push(r.pid);
        return Promise.resolve([]);
      },
    });
    assertEquals(report.action, "respawn");
    assertEquals(report.epoch, 3);
    assertEquals(stopped, [91]);
    assertEquals(spawned, [1, 3]);
    // A later reconcile does not stop it again.
    await reconcileTaskJobs({
      state,
      git,
      spawn,
      stopTree: (r) => {
        stopped.push(r.pid);
        return Promise.resolve([]);
      },
    });
    assertEquals(stopped, [91]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: when the stop fails, the next epoch is not spawned", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const spawned: number[] = [];
    const spawn = (_w: string, _t: string, _j: string, epoch: number) => {
      spawned.push(epoch);
      return 1;
    };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn,
    });
    const old = new Date(Date.now() - 60_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", {
        pid: 5,
        startTime: "s",
        commandLine: "c",
      });
    });
    const [report] = await reconcileTaskJobs({
      state,
      git,
      spawn,
      stopTree: () => Promise.reject(new Error("access denied")),
    });
    assert(report.error?.includes("access denied"));
    assertEquals(spawned, [1]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: one corrupt journal does not stop the sweep, and its error names the file", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const intent = await startIntent(state, "push", "x", "agent-a");
    Deno.writeTextFileSync(`${dir}/_dist/fdh/jobs/t1/aaa-bad.json`, "{ trunc");
    const reports = await reconcileTaskJobs({ state, intentHoldersGone: true });
    const bad = reports.find((r) => r.jobId === "aaa-bad");
    assertEquals(bad?.action, "error");
    assert(bad?.error?.includes("aaa-bad.json"));
    assertEquals(
      reports.find((r) => r.jobId === intent.jobId)?.action,
      "awaiting-approval",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: children of an exited pid reuser are not selected", () => {
  const root = {
    pid: 10,
    startTime: "2026-11-09T10:00:00.000Z",
    commandLine: "deno task test:host",
  };
  // Root gone, pid 10 free again; its "children" started after the loss.
  const snapshot: DevProcessInfo[] = [
    {
      pid: 20,
      ppid: 10,
      name: "code.exe",
      commandLine: "code",
      startTime: "2026-11-09T12:00:00.000Z",
    },
  ];
  assertEquals(
    selectProcessTree(snapshot, root, "2026-11-09T10:05:00.000Z"),
    [],
  );
});

Deno.test("fdh job: job wait gives up after consecutive reconcile errors instead of polling silently", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn: () => 1,
    });
    const old = new Date(Date.now() - 60_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", {
        pid: 5,
        startTime: "s",
        commandLine: "c",
      });
    });
    const seen: string[] = [];
    await assertRejects(
      () =>
        waitForJob({
          state,
          jobId: job.jobId,
          timeoutMs: 60_000,
          git,
          spawn: () => 1,
          stopTree: () => Promise.reject(new Error("denied")),
          sleep: () => Promise.resolve(),
          onError: (m) => seen.push(m),
        }),
      Error,
      "could not be reconciled 3 times in a row",
    );
    assertEquals(seen.length, 3);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: the child bound of a lost root is its last heartbeat plus one TTL", () => {
  assertEquals(
    lostTreeBound({
      lastSeenAt: "2026-11-09T10:00:00.000Z",
      endedAt: "2026-11-10T08:00:00.000Z",
    }, 10_000),
    "2026-11-09T10:00:10.000Z",
  );
  assertEquals(
    lostTreeBound({
      lastSeenAt: "2026-11-09T10:00:00.000Z",
      endedAt: "2026-11-09T10:00:03.000Z",
    }, 10_000),
    "2026-11-09T10:00:03.000Z",
  );
  assertEquals(
    lostTreeBound({ endedAt: "2026-11-09T10:00:03.000Z" }, 10_000),
    "2026-11-09T10:00:03.000Z",
  );
});

Deno.test("fdh job: job cancel also stops a lost tree left by an interrupted respawn", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn: () => 1,
    });
    const old = new Date(Date.now() - 120_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", {
        pid: 61,
        startTime: "s",
        commandLine: "c",
      });
      reconcileJob(j, { currentDigest: j.executionDigest, at: old });
    });
    const stopped: number[] = [];
    const { job: after } = await cancelAndSettle(
      state,
      job.jobId,
      "not needed",
      {
        git,
        spawn: () => 1,
        stopTree: (r) => {
          stopped.push(r.pid);
          return Promise.resolve([]);
        },
      },
    );
    assertEquals(after.state, "cancelled");
    assertEquals(stopped, [61]);
    assertEquals(after.recordedToTask, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// CodeRabbit on PR #3097 head 03adaa8d
// ---------------------------------------------------------------------------

Deno.test("fdh job: a job that ends without a command result records a failed assertion", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const repo = { head: "h", diff: "", untracked: "" };
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git: fakeGit(repo),
      spawn: () => 1,
    });
    const old = new Date(Date.now() - 60_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => beginAttempt(j, 1, "x1", old));
    repo.diff = "edited";
    await reconcileTaskJobs({ state, git: fakeGit(repo), spawn: () => 1 });
    const assertions = readTaskState(dir, "t1").assertions;
    assertEquals(assertions.map((a) => [a.name, a.passed]), [[
      "host-test-exit-code",
      false,
    ]]);
    assert(assertions[0].actual.includes("ended failed"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a quota ledger that names another task is not written", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const job = await startIntent(state, "push", "x", "agent-a");
    Deno.mkdirSync(`${dir}/_dist/fdh/quota`, { recursive: true });
    Deno.writeTextFileSync(
      `${dir}/_dist/fdh/quota/t1.json`,
      JSON.stringify({
        schemaVersion: 1,
        taskId: "other",
        worktree: dir,
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        activeAgent: null,
        stopped: false,
        stopReason: null,
        approvalPending: [],
        events: [],
      }),
    );
    const [report] = await reconcileTaskJobs({
      state,
      intentHoldersGone: true,
    });
    assertEquals(report.action, "error");
    assert(report.error?.includes("records another task or worktree"));
    // The journal already moved to awaiting-approval; the ledger was untouched.
    assertEquals(readJob(dir, "t1", job.jobId).state, "awaiting-approval");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// KAN-137 hardening
// ---------------------------------------------------------------------------

Deno.test("fdh job: editing an untracked file changes the execution inputs", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    Deno.writeTextFileSync(`${dir}/notes.ts`, "export const a = 1;\n");
    const git = fakeGit({ head: "h", diff: "", untracked: "notes.ts\0" });
    const a = await currentInputs(state, git);
    // A different size: the per-process cache (keyed by size and mtime)
    // cannot hide the change.
    Deno.writeTextFileSync(`${dir}/notes.ts`, "export const a = 22;\n");
    const b = await currentInputs(state, git);
    assertNotEquals(a.worktreeState, b.worktreeState);
    // Same content and mtime as at first: the same digest again.
    Deno.writeTextFileSync(`${dir}/notes.ts`, "export const a = 1;\n");
    const t = new Date("2026-11-10T00:00:00.000Z");
    Deno.utimeSync(`${dir}/notes.ts`, t, t);
    const c = await currentInputs(state, git);
    Deno.writeTextFileSync(`${dir}/notes.ts`, "export const a = 3;\n");
    Deno.utimeSync(
      `${dir}/notes.ts`,
      new Date(t.getTime() + 5_000),
      new Date(t.getTime() + 5_000),
    );
    assertNotEquals(
      (await currentInputs(state, git)).worktreeState,
      c.worktreeState,
    );
    const lines = await hashUntracked(dir, ["gone.ts", "notes.ts"]);
    assert(lines.startsWith("gone.ts\0missing\nnotes.ts\0"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a source-build respawn is held while the dev guard refuses", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const spawned: number[] = [];
    const spawn = (_w: string, _t: string, _j: string, epoch: number) => {
      spawned.push(epoch);
      return 1;
    };
    const job = await startStepJob({
      state,
      kind: "build",
      phase: "all",
      leaseTtlMs: 1_000,
      git,
      spawn,
    });
    assert(respawnCompilesSource(readJob(dir, "t1", job.jobId)));
    const old = new Date(Date.now() - 60_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => beginAttempt(j, 1, "x1", old));
    // Held: the journal is untouched, so no attempt is used up however
    // long the dev server runs.
    for (let i = 0; i < 4; i++) {
      const [report] = await reconcileTaskJobs({
        state,
        git,
        spawn,
        sourceBuildGuard: () => Promise.reject(new Error("dev server running")),
      });
      assertEquals(report.action, "held");
      assert(report.error?.includes("dev server running"));
    }
    const held = readJob(dir, "t1", job.jobId);
    assertEquals(held.epoch, 1);
    assertEquals(held.attempts.length, 1);
    assertEquals(spawned, [1]);
    // Once the guard passes ('fdh dev down'), the job is taken over.
    const [after] = await reconcileTaskJobs({
      state,
      git,
      spawn,
      sourceBuildGuard: () => Promise.resolve(),
    });
    assertEquals(after.action, "respawn");
    assertEquals(spawned, [1, 2]);
    // After the source command is checkpointed, an after-mach-only respawn is
    // not guarded.
    await updateJob(dir, "t1", job.jobId, (j) => {
      j.checkpoint = 1;
    });
    assertEquals(respawnCompilesSource(readJob(dir, "t1", job.jobId)), false);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a missing approval entry is put back, but an approval is not undone", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const job = await startIntent(state, "push", "origin b", "agent-a");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    // The ledger entry is lost (e.g. the controller died before writing it).
    const file = `${dir}/_dist/fdh/quota/t1.json`;
    const ledger = JSON.parse(Deno.readTextFileSync(file));
    ledger.approvalPending = [];
    Deno.writeTextFileSync(file, JSON.stringify(ledger));
    await reconcileTaskJobs({ state });
    assertEquals(readQuotaLedger(dir, "t1").approvalPending.length, 1);
    // A human approves through the quota ledger: reconcile leaves it approved.
    const approved = readQuotaLedger(dir, "t1");
    approved.approvalPending = [];
    approved.events.push({
      at: new Date(Date.now() + 1_000).toISOString(),
      kind: "approve",
      note: "human approved",
    });
    Deno.writeTextFileSync(file, JSON.stringify(approved));
    await reconcileTaskJobs({ state });
    assertEquals(readQuotaLedger(dir, "t1").approvalPending, []);
    assertEquals(readJob(dir, "t1", job.jobId).state, "awaiting-approval");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: resolving again after a failed ledger write only repeats the ledger step", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const job = await startIntent(state, "push", "origin b", "agent-a");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    const file = `${dir}/_dist/fdh/quota/t1.json`;
    const good = Deno.readTextFileSync(file);
    // The first resolve updates the journal, then the ledger write fails.
    Deno.writeTextFileSync(file, "{ broken");
    await assertRejects(() =>
      resolveAndApprove(state, job.jobId, "succeeded", "seen on origin")
    );
    assertEquals(readJob(dir, "t1", job.jobId).state, "succeeded");
    Deno.writeTextFileSync(file, good);
    const again = await resolveAndApprove(state, job.jobId, "succeeded", "");
    assertEquals(again.state, "succeeded");
    assertEquals(readQuotaLedger(dir, "t1").approvalPending, []);
    // A different resolution is still refused.
    await assertRejects(
      () => resolveAndApprove(state, job.jobId, "failed", ""),
      Error,
      "only a job awaiting approval",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: job cancel returns the reconcile errors", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const git = fakeGit({ head: "h", diff: "", untracked: "" });
    const job = await startStepJob({
      state,
      kind: "test",
      leaseTtlMs: 1_000,
      git,
      spawn: () => 1,
    });
    const old = new Date(Date.now() - 120_000).toISOString();
    await updateJob(dir, "t1", job.jobId, (j) => {
      beginAttempt(j, 1, "x1", old);
      recordStepProcess(j, 1, "x1", {
        pid: 9,
        startTime: "s",
        commandLine: "c",
      });
      reconcileJob(j, { currentDigest: j.executionDigest, at: old });
    });
    const { job: after, reports } = await cancelAndSettle(
      state,
      job.jobId,
      "",
      {
        git,
        spawn: () => 1,
        stopTree: () => Promise.reject(new Error("denied")),
      },
    );
    assertEquals(after.state, "cancelled");
    assert(reports[0].error?.includes("denied"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: another entry's approval does not stop the repair", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await startIntent(state, "push", "origin b", "agent-a");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    const file = `${dir}/_dist/fdh/quota/t1.json`;
    const ledger = JSON.parse(Deno.readTextFileSync(file));
    ledger.approvalPending = [];
    ledger.events.push({
      at: new Date(Date.now() + 1_000).toISOString(),
      kind: "approve",
      op: "pr",
      note: "resolved succeeded job intent-other: x",
    });
    Deno.writeTextFileSync(file, JSON.stringify(ledger));
    await reconcileTaskJobs({ state });
    assertEquals(readQuotaLedger(dir, "t1").approvalPending.length, 1);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: an approve-all before an entry was listed does not stop its repair", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    await startIntent(state, "push", "origin a", "agent-a");
    const b = await startIntent(state, "pr", "draft", "agent-a");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    const file = `${dir}/_dist/fdh/quota/t1.json`;
    const ledger = JSON.parse(Deno.readTextFileSync(file));
    // A's entry was never written; it was not in the list that approve-all
    // cleared, so it must come back. B's entry was listed and approved.
    ledger.approvalPending = [];
    ledger.events = ledger.events.filter((e: { kind: string; note?: string }) =>
      !(e.kind === "pending" && e.note?.includes("origin a"))
    );
    ledger.events.push({
      at: new Date(Date.now() + 1_000).toISOString(),
      kind: "approve",
      note: "pr",
    });
    Deno.writeTextFileSync(file, JSON.stringify(ledger));
    await reconcileTaskJobs({ state });
    const pending = readQuotaLedger(dir, "t1").approvalPending.map((p) => p.op);
    assertEquals(pending, ["push"]);
    assertEquals(readJob(dir, "t1", b.jobId).state, "awaiting-approval");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: a repeated retry resolution is refused", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = taskIn(dir);
    const job = await startIntent(state, "push", "origin b", "agent-a");
    await reconcileTaskJobs({ state, intentHoldersGone: true });
    await resolveAndApprove(state, job.jobId, "retry", "", "agent-b");
    await assertRejects(
      () => resolveAndApprove(state, job.jobId, "retry", "", "agent-c"),
      Error,
      "only a job awaiting approval",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh job: an unreadable untracked file is counted, not fatal", async () => {
  const dir = await Deno.makeTempDir();
  try {
    Deno.mkdirSync(`${dir}/sub`);
    Deno.writeTextFileSync(`${dir}/held.txt`, "held\n");
    const holder = Deno.openSync(`${dir}/held.txt`, {
      read: true,
      write: true,
    });
    try {
      // Windows locks are mandatory, so the read fails there (os error 33);
      // POSIX locks are advisory and the file is hashed as usual.
      holder.lockSync(true);
      const lines = await hashUntracked(dir, ["held.txt", "sub/"]);
      if (Deno.build.os === "windows") {
        assert(lines.startsWith("held.txt\0unreadable:5:"), lines);
      } else {
        assert(!lines.includes("unreadable"), lines);
      }
      assert(lines.includes("sub/\0other:"), lines);
    } finally {
      holder.close();
    }
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
