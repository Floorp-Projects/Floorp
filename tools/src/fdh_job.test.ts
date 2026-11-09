// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertNotEquals } from "@std/assert";
import {
  currentInputs,
  quotaNoteFor,
  reconcileTaskJobs,
  resolveAndApprove,
  settleJob,
  startIntent,
  startStepJob,
  stepSpecFor,
} from "./fdh_job.ts";
import { selectProcessTree } from "./fdh_job_process.ts";
import {
  applyReport,
  beginAttempt,
  readJob,
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
  // Root gone: its children are still found by parent pid and start time.
  assertEquals(
    selectProcessTree(snapshot.filter((x) => x.pid !== 10), root).map((x) =>
      x.pid
    ),
    [11, 12],
  );
  // A different process now holds pid 10: it is not selected as the root.
  const reused = [p(10, 1, "2026-11-09T11:00:00.000Z")];
  assertEquals(selectProcessTree(reused, root), []);
});
