// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertNotEquals,
  assertRejects,
  assertThrows,
} from "@std/assert";
import {
  applyReport,
  beginAttempt,
  confirmIntent,
  createJob,
  type CreateJobInput,
  DEFAULT_LEASE_TTL_MS,
  executionDigest,
  heartbeat,
  type JobInputs,
  type JobJournal,
  listJobIds,
  newJobId,
  parseJobJournal,
  readJob,
  readOutbox,
  reconcileJob,
  reconcileJobFile,
  resolveJob,
  runExecutor,
  type RunningStep,
  type StepSpec,
  updateJob,
  writeNewJob,
  writeOutbox,
} from "./fdh_executor.ts";

const T0 = Date.parse("2026-11-09T12:00:00.000Z");
const at = (ms: number) => new Date(T0 + ms).toISOString();
const TTL = DEFAULT_LEASE_TTL_MS;

const STEP: StepSpec = {
  argv: ["deno", "task", "test:host"],
  cwd: "/wt",
  displayCommand: "deno task test:host",
};
const INPUTS: JobInputs = { baseCommit: "abc", head: "def", guardEnv: {} };

function stepJob(
  worktree = "/wt",
  extra: Partial<CreateJobInput> = {},
): JobJournal {
  return createJob({
    ...extra,
    jobId: "test-1",
    taskId: "t1",
    worktree,
    kind: "test",
    step: STEP,
    inputs: INPUTS,
    executionDigest: "d1",
    at: at(0),
    ...extra,
  });
}

function report(
  epoch: number,
  exitCode: number,
  ms: number,
  holder = `x${epoch}`,
) {
  return {
    jobId: "test-1",
    epoch,
    holder,
    exitCode,
    endedAt: at(ms),
    logRef: null,
  };
}

Deno.test("fdh executor: a step job records attempt 1 at epoch 1 before it starts", () => {
  const job = stepJob();
  assertEquals(job.state, "scheduled");
  assertEquals(job.epoch, 1);
  assertEquals(job.lease, null);
  assertEquals(job.attempts.map((a) => [a.attempt, a.epoch, a.result]), [[
    1,
    1,
    "scheduled",
  ]]);
  assertEquals(job.transitions.map((t) => t.to), ["pending", "scheduled"]);
});

Deno.test("fdh executor: the execution digest is canonical and binds the inputs", async () => {
  const a = await executionDigest("test", STEP, INPUTS);
  const b = await executionDigest("test", {
    displayCommand: STEP.displayCommand,
    cwd: STEP.cwd,
    argv: STEP.argv,
  }, { guardEnv: {}, head: "def", baseCommit: "abc" });
  assertEquals(a, b);
  assertNotEquals(
    a,
    await executionDigest("test", STEP, { ...INPUTS, head: "other" }),
  );
  assertNotEquals(a, await executionDigest("build", STEP, INPUTS));
});

Deno.test("fdh executor E-1: a live executor is reconnected to, not restarted", () => {
  const job = stepJob();
  assertEquals(beginAttempt(job, 1, "x1", at(100)), { ok: true });
  assertEquals(job.state, "running");
  assertEquals(heartbeat(job, 1, "x1", at(TTL)), "renewed");
  // A new controller after the old one stopped: the lease is still fresh.
  assertEquals(
    reconcileJob(job, { currentDigest: "d1", at: at(TTL + TTL / 2) }),
    {
      action: "alive",
      epoch: 1,
    },
  );
  assertEquals(applyReport(job, report(1, 0, TTL + 1000), at(TTL + 1000)), {
    applied: true,
    state: "succeeded",
  });
  assertEquals(job.attempts.length, 1);
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(10 * TTL) }), {
    action: "none",
    state: "succeeded",
  });
});

Deno.test("fdh executor E-2: a lost executor is replaced at epoch 2 and the job completes", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(TTL) }), {
    action: "alive",
    epoch: 1,
  });
  assertEquals(reconcileJob(job, { at: at(TTL + 1), currentDigest: "d1" }), {
    action: "respawn",
    epoch: 2,
  });
  assertEquals(job.attempts.map((a) => [a.epoch, a.result]), [[1, "lost"], [
    2,
    "scheduled",
  ]]);
  assertEquals(job.state, "running");
  // The old epoch may not take the new attempt.
  assertEquals(beginAttempt(job, 1, "x1", at(TTL + 2)), {
    ok: false,
    reason: "stale-epoch",
  });
  assertEquals(beginAttempt(job, 2, "x2", at(TTL + 3)), { ok: true });
  assertEquals(
    applyReport(job, report(2, 0, TTL + 50), at(TTL + 50)).applied,
    true,
  );
  assertEquals(job.state, "succeeded");
  assertEquals(job.attempts[1].result, "succeeded");
});

Deno.test("fdh executor E-3: a result in the outbox is applied without rerunning", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  const decision = reconcileJob(job, {
    at: at(3 * TTL),
    outbox: report(1, 0, 500),
    currentDigest: "changed",
  });
  assertEquals(decision, { action: "applied-outbox", state: "succeeded" });
  assertEquals(job.attempts.length, 1);
  assertEquals(job.attempts[0].result, "succeeded");
  assert(job.events.some((e) => e.kind === "outbox-applied"));
});

Deno.test("fdh executor E-3: a failing outbox result is applied as failed, not rerun", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  assertEquals(
    reconcileJob(job, {
      currentDigest: "d1",
      at: at(3 * TTL),
      outbox: report(1, 2, 500),
    }),
    {
      action: "applied-outbox",
      state: "failed",
    },
  );
  assertEquals(job.attempts[0].exitCode, 2);
});

Deno.test("fdh executor E-4/E-5: a stalled executor is fenced and its late report is rejected", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  assertEquals(
    reconcileJob(job, { currentDigest: "d1", at: at(TTL + 1) }).action,
    "respawn",
  );
  beginAttempt(job, 2, "x2", at(TTL + 2));
  // The old executor comes back: its heartbeat is fenced ...
  assertEquals(heartbeat(job, 1, "x1", at(TTL + 3)), "fenced");
  assertEquals(heartbeat(job, 1, "x1", at(TTL + 4)), "fenced");
  assertEquals(job.events.filter((e) => e.kind === "fenced").length, 1);
  // ... and its result is rejected and recorded.
  assertEquals(applyReport(job, report(1, 0, TTL + 5), at(TTL + 5)), {
    applied: false,
    reason: "stale-epoch",
  });
  assertEquals(job.attempts[0].result, "lost");
  assert(job.attempts[0].notes.includes("late report by x1 (exit 0)"));
  assertEquals(job.state, "running");
  assert(job.events.some((e) => e.kind === "stale-epoch" && e.epoch === 1));
  // The job's result is the newer attempt's.
  applyReport(job, report(2, 1, TTL + 9), at(TTL + 9));
  assertEquals(job.state, "failed");
  assertEquals(job.attempts[1].exitCode, 1);
});

Deno.test("fdh executor E-6: an unknown non-idempotent intent awaits approval and is never rerun", () => {
  const job = createJob({
    jobId: "intent-1",
    taskId: "t1",
    worktree: "/wt",
    kind: "intent",
    intent: { op: "push", target: "origin fdh/x" },
    inputs: INPUTS,
    executionDigest: "d",
    holder: "agent-a",
    at: at(0),
  });
  assertEquals(job.state, "running");
  assertEquals(job.lease?.holder, "agent-a");
  assertEquals(
    reconcileJob(job, { currentDigest: "d1", at: at(60_000) }).action,
    "alive",
  );
  assertEquals(
    reconcileJob(job, {
      currentDigest: "d1",
      at: at(60_000),
      holderGone: true,
    }),
    {
      action: "awaiting-approval",
      epoch: 1,
    },
  );
  assertEquals(job.state, "awaiting-approval");
  assertEquals(job.attempts.length, 1);
  assertEquals(
    reconcileJob(job, {
      currentDigest: "d1",
      at: at(10 * 60_000),
      holderGone: true,
    }),
    {
      action: "none",
      state: "awaiting-approval",
    },
  );
  // A late confirm from the lost holder does not decide the outcome.
  assertEquals(
    confirmIntent(job, 1, "agent-a", "succeeded", at(70_000)).applied,
    false,
  );
  assertEquals(job.state, "awaiting-approval");
  // A human allows a retry: a new attempt at epoch 2, held by the agent.
  resolveJob(job, "retry", "remote branch absent", "agent-b", at(80_000));
  assertEquals(job.state, "running");
  assertEquals(job.epoch, 2);
  assertEquals(job.attempts[1].holder, "agent-b");
  assertEquals(
    confirmIntent(job, 2, "agent-b", "succeeded", at(90_000)).applied,
    true,
  );
  assertEquals(job.state, "succeeded");
});

Deno.test("fdh executor E-6: an approver can confirm or fail an unknown intent", () => {
  const make = () =>
    createJob({
      jobId: "intent-2",
      taskId: "t1",
      worktree: "/wt",
      kind: "intent",
      intent: { op: "pr", target: "#1" },
      inputs: INPUTS,
      executionDigest: "d",
      holder: "agent-a",
      leaseTtlMs: 1_000,
      at: at(0),
    });
  const done = make();
  reconcileJob(done, { currentDigest: "d1", at: at(1_001) });
  resolveJob(done, "succeeded", "PR exists", undefined, at(2_000));
  assertEquals(done.state, "succeeded");
  assertEquals(done.transitions.slice(-2).map((t) => t.to), [
    "running",
    "succeeded",
  ]);
  const failed = make();
  reconcileJob(failed, { currentDigest: "d1", at: at(1_001) });
  resolveJob(failed, "failed", "", undefined, at(2_000));
  assertEquals(failed.state, "failed");
  assertThrows(
    () => resolveJob(failed, "retry", "", "a"),
    Error,
    "only a job awaiting approval",
  );
});

Deno.test("fdh executor E-7: changed inputs fail the job instead of rerunning it", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  assertEquals(reconcileJob(job, { at: at(TTL + 1), currentDigest: "d2" }), {
    action: "input-changed",
    state: "failed",
  });
  assertEquals(job.attempts.length, 1);
  assert(job.events.some((e) => e.kind === "input-changed"));
});

Deno.test("fdh executor: an executor that never starts is replaced after one TTL", () => {
  const job = stepJob();
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(TTL) }), {
    action: "alive",
    epoch: 1,
  });
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(TTL + 1) }), {
    action: "respawn",
    epoch: 2,
  });
  assertEquals(job.attempts[0].result, "lost");
  // A slow first executor that starts now is refused.
  assertEquals(beginAttempt(job, 1, "slow", at(TTL + 2)).ok, false);
});

Deno.test("fdh executor: job ids, specs and TTLs are validated", () => {
  assert(/^test-20261109t120000z-[0-9a-f]{6}$/.test(newJobId("test", at(0))));
  const base = {
    jobId: "j",
    taskId: "t1",
    worktree: "/wt",
    inputs: INPUTS,
    executionDigest: "d",
  };
  assertThrows(
    () => createJob({ ...base, kind: "test" }),
    Error,
    "needs a step spec",
  );
  assertThrows(
    () =>
      createJob({
        ...base,
        kind: "intent",
        intent: { op: "push", target: "" },
      }),
    Error,
    "needs the agent",
  );
  assertThrows(
    () =>
      createJob({
        ...base,
        kind: "intent",
        intent: { op: "force" as never, target: "" },
        holder: "a",
      }),
    Error,
    "intent op must be one of",
  );
  assertThrows(
    () => createJob({ ...base, kind: "test", step: STEP, leaseTtlMs: 10 }),
    Error,
    "lease TTL",
  );
  assertThrows(
    () => createJob({ ...base, jobId: "Bad/Id", kind: "test", step: STEP }),
    Error,
    "invalid job id",
  );
});

Deno.test("fdh executor: journals are written once, updated under the lock, and listed", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const job = stepJob(dir);
    await writeNewJob(job);
    await assertRejects(() => writeNewJob(job), Error, "already exists");
    await updateJob(
      dir,
      "t1",
      "test-1",
      (j) => beginAttempt(j, 1, "x1", at(0)),
    );
    assertEquals(readJob(dir, "t1", "test-1").state, "running");
    writeOutbox(dir, "t1", report(1, 0, 10));
    assertEquals(readOutbox(dir, "t1", "test-1", 1)?.exitCode, 0);
    assertEquals(readOutbox(dir, "t1", "test-1", 2), null);
    assertEquals(listJobIds(dir, "t1"), ["test-1"]);
    assertEquals(listJobIds(dir, "nothing"), []);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// runExecutor with a fake clock and a fake step
// ---------------------------------------------------------------------------

interface Harness {
  dir: string;
  clock: { t: number };
  now: () => string;
  sleep: (ms: number) => Promise<void>;
  step: (
    code: number,
    afterMs: number,
  ) => (s: StepSpec, log: string) => RunningStep;
  killed: { value: boolean };
  /** Every sleep the executor asked for (milliseconds). */
  sleeps: number[];
  /** Hold the fake clock at `ms` until `release()` is called. */
  gate: { at: number; release: () => void; reached: Promise<void> };
}

async function harness(
  extra: Partial<CreateJobInput> = {},
): Promise<Harness> {
  const dir = await Deno.makeTempDir();
  await writeNewJob(stepJob(dir, extra));
  const sleeps: number[] = [];
  const clock = { t: 0 };
  const killed = { value: false };
  const now = () => at(clock.t);
  let opened!: () => void;
  const open = new Promise<void>((r) => (opened = r));
  let reachedResolve!: () => void;
  const reached = new Promise<void>((r) => (reachedResolve = r));
  const gate = {
    at: Number.POSITIVE_INFINITY,
    release: () => opened(),
    reached,
  };
  const sleep = async (ms: number) => {
    sleeps.push(ms);
    if (clock.t + ms >= gate.at) {
      reachedResolve();
      await open;
    }
    clock.t += ms;
    await new Promise((r) => setTimeout(r, 0));
  };
  const step = (code: number, afterMs: number) => (): RunningStep => {
    let resolve!: (code: number) => void;
    const status = new Promise<number>((r) => (resolve = r));
    const start = clock.t;
    const poll = async () => {
      while (!killed.value && clock.t - start < afterMs) {
        await new Promise((r) => setTimeout(r, 0));
      }
      resolve(killed.value ? 137 : code);
    };
    poll();
    return {
      status,
      kill: () => {
        killed.value = true;
      },
    };
  };
  return { dir, clock, now, sleep, step, killed, gate, sleeps };
}

Deno.test("fdh executor run: a step runs with heartbeats, writes the outbox, then applies it", async () => {
  const h = await harness();
  try {
    const outcome = await runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: h.step(0, 7_000),
      heartbeatMs: 2_000,
      now: h.now,
      sleep: h.sleep,
    });
    assertEquals(outcome, { outcome: "applied", state: "succeeded" });
    const job = readJob(h.dir, "t1", "test-1");
    assertEquals(job.attempts[0].result, "succeeded");
    assertEquals(job.lease, null);
    assertEquals(readOutbox(h.dir, "t1", "test-1", 1)?.exitCode, 0);
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

Deno.test("fdh executor run: a fenced executor stops its step and writes no result", async () => {
  const h = await harness();
  try {
    const run = runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: h.step(0, 60_000),
      heartbeatMs: 2_000,
      now: h.now,
      sleep: h.sleep,
    });
    // Another controller takes over while the step runs.
    while (readJob(h.dir, "t1", "test-1").state !== "running") {
      await new Promise((r) => setTimeout(r, 0));
    }
    await updateJob(
      h.dir,
      "t1",
      "test-1",
      (job) =>
        reconcileJob(job, {
          currentDigest: "d1",
          holderGone: true,
          at: h.now(),
        }),
    );
    assertEquals(await run, { outcome: "fenced" });
    assert(h.killed.value);
    assertEquals(readOutbox(h.dir, "t1", "test-1", 1), null);
    const job = readJob(h.dir, "t1", "test-1");
    assertEquals(job.epoch, 2);
    assert(job.events.some((e) => e.kind === "fenced" && e.epoch === 1));
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

Deno.test("fdh executor run: exit after the outbox (E-3) is recovered by reconcile", async () => {
  const h = await harness();
  try {
    await assertRejects(
      () =>
        runExecutor({
          worktree: h.dir,
          taskId: "t1",
          jobId: "test-1",
          epoch: 1,
          holder: "x1",
          runStep: h.step(0, 1_000),
          heartbeatMs: 2_000,
          now: h.now,
          sleep: h.sleep,
          fault: { kind: "exit-after-outbox" },
          exit: (code) => {
            throw new Error(`exit ${code}`);
          },
        }),
      Error,
      "exit 70",
    );
    assertEquals(readJob(h.dir, "t1", "test-1").state, "running");
    const { value } = await updateJob(
      h.dir,
      "t1",
      "test-1",
      (job) =>
        reconcileJob(job, {
          currentDigest: "d1",
          at: at(5 * TTL),
          outbox: readOutbox(h.dir, "t1", "test-1", job.epoch),
        }),
    );
    assertEquals(value, { action: "applied-outbox", state: "succeeded" });
    assertEquals(readJob(h.dir, "t1", "test-1").attempts.length, 1);
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

Deno.test("fdh executor run: a stalled executor's report arrives at an old epoch and is rejected (E-5)", async () => {
  const h = await harness();
  // The executor stalls; the clock stops just after its lease expired.
  h.gate.at = TTL + 2_000;
  try {
    const run = runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: h.step(0, 3_000),
      heartbeatMs: 2_000,
      now: h.now,
      sleep: h.sleep,
      fault: { kind: "stall", ms: 3 * TTL },
    });
    // While it stalls, its lease expires and epoch 2 takes over.
    await h.gate.reached;
    const { value } = await updateJob(
      h.dir,
      "t1",
      "test-1",
      (job) => reconcileJob(job, { currentDigest: "d1", at: at(TTL + 1_000) }),
    );
    assertEquals(value, { action: "respawn", epoch: 2 });
    h.gate.release();
    assertEquals(await run, { outcome: "stale-rejected" });
    const job = readJob(h.dir, "t1", "test-1");
    assertEquals(job.attempts[0].result, "lost");
    assert(job.attempts[0].notes.includes("late report by x1 (exit 0)"));
    assertEquals(job.attempts[1].result, "scheduled");
    assertEquals(job.state, "running");
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

Deno.test("fdh executor: a safe step that keeps losing its executor fails after maxAttempts", () => {
  const job = stepJob();
  let t = 0;
  for (let epoch = 1; epoch <= 2; epoch++) {
    beginAttempt(job, epoch, `x${epoch}`, at(t));
    t += TTL + 1;
    assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(t) }), {
      action: "respawn",
      epoch: epoch + 1,
    });
  }
  beginAttempt(job, 3, "x3", at(t));
  t += TTL + 1;
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(t) }), {
    action: "gave-up",
    state: "failed",
  });
  assertEquals(job.attempts.map((a) => a.result), ["lost", "lost", "lost"]);
  assert(job.events.some((e) => e.kind === "gave-up"));
});

Deno.test("fdh executor: a report after its holder was declared lost is recorded, not applied", () => {
  const job = createJob({
    jobId: "intent-3",
    taskId: "t1",
    worktree: "/wt",
    kind: "intent",
    intent: { op: "release", target: "v1" },
    inputs: INPUTS,
    executionDigest: "d",
    holder: "agent-a",
    at: at(0),
  });
  reconcileJob(job, { currentDigest: "d1", at: at(1), holderGone: true });
  assertEquals(confirmIntent(job, 1, "agent-a", "succeeded", at(2)), {
    applied: false,
    reason: "holder-lost",
  });
  assertEquals(job.state, "awaiting-approval");
  assert(job.events.some((e) => e.kind === "late-report" && e.epoch === 1));
});

Deno.test("fdh executor run: a step that cannot start is reported as a failed attempt", async () => {
  const h = await harness();
  try {
    const outcome = await runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: () => {
        throw new Error("spawn failed");
      },
      now: h.now,
      sleep: h.sleep,
    });
    assertEquals(outcome, { outcome: "applied", state: "failed" });
    const job = readJob(h.dir, "t1", "test-1");
    assertEquals(job.attempts[0].exitCode, 127);
    const log = Deno.readTextFileSync(`${h.dir}/${job.attempts[0].logRef}`);
    assert(log.includes("spawn failed"));
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

// ---------------------------------------------------------------------------
// review findings (PR #3091)
// ---------------------------------------------------------------------------

Deno.test("fdh executor: a job whose executor never starts is closed through running, not stuck", () => {
  // Default maxAttempts (3): two respawns, then gave-up.
  const job = stepJob();
  let t = 0;
  for (const epoch of [2, 3]) {
    t += TTL + 1;
    assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(t) }), {
      action: "respawn",
      epoch,
    });
    assertEquals(job.state, "scheduled");
  }
  t += TTL + 1;
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(t) }), {
    action: "gave-up",
    state: "failed",
  });
  assertEquals(
    job.transitions.slice(-2).map((x) => `${x.from}>${x.to}`),
    ["scheduled>running", "running>failed"],
  );
  // Later reconciles are no-ops, not errors.
  assertEquals(reconcileJob(job, { currentDigest: "d1", at: at(t + 1) }), {
    action: "none",
    state: "failed",
  });

  const changed = stepJob();
  assertEquals(
    reconcileJob(changed, { currentDigest: "d2", at: at(TTL + 1) }),
    { action: "input-changed", state: "failed" },
  );
  const single = stepJob("/wt", { maxAttempts: 1 });
  assertEquals(
    reconcileJob(single, { currentDigest: "d1", at: at(TTL + 1) }),
    { action: "gave-up", state: "failed" },
  );
});

Deno.test("fdh executor: an unparsable reconcile time throws instead of bumping the epoch", () => {
  const job = stepJob();
  for (const bad of ["", "garbage"]) {
    assertThrows(
      () => reconcileJob(job, { currentDigest: "d1", at: bad }),
      Error,
      "not a valid ISO time",
    );
  }
  assertEquals(job.epoch, 1);
  assertEquals(job.attempts.length, 1);
});

Deno.test("fdh executor: reconciling a step job requires the current digest", () => {
  const job = stepJob();
  assertThrows(
    () => reconcileJob(job, { at: at(TTL + 1) }),
    Error,
    "needs the current execution digest",
  );
  assertEquals(job.epoch, 1);
});

Deno.test("fdh executor: a report from another holder at the current epoch is not applied", () => {
  const job = stepJob();
  beginAttempt(job, 1, "x1", at(0));
  assertEquals(applyReport(job, report(1, 0, 10, "mallory"), at(10)), {
    applied: false,
    reason: "wrong-holder",
  });
  assertEquals(job.state, "running");
  assert(job.events.some((e) => e.kind === "wrong-holder"));
});

Deno.test("fdh executor: a late report after a retry keeps the lost attempt and is recorded once", () => {
  const job = createJob({
    jobId: "test-1",
    taskId: "t1",
    worktree: "/wt",
    kind: "intent",
    intent: { op: "push", target: "x" },
    inputs: INPUTS,
    executionDigest: "d",
    holder: "x1",
    at: at(0),
  });
  reconcileJob(job, { at: at(1), holderGone: true });
  resolveJob(job, "retry", "", "x2", at(2));
  for (let i = 0; i < 5; i++) {
    assertEquals(applyReport(job, report(1, 0, 3), at(3 + i)), {
      applied: false,
      reason: "stale-epoch",
    });
  }
  assertEquals(job.attempts[0].result, "lost");
  assert(job.attempts[0].notes.includes("late report by x1 (exit 0)"));
  assertEquals(job.events.filter((e) => e.kind === "stale-epoch").length, 1);
});

Deno.test("fdh executor: retrying an intent without a holder throws before changing the job", () => {
  const job = createJob({
    jobId: "intent-1",
    taskId: "t1",
    worktree: "/wt",
    kind: "intent",
    intent: { op: "pr", target: "x" },
    inputs: INPUTS,
    executionDigest: "d",
    holder: "a",
    at: at(0),
  });
  reconcileJob(job, { at: at(1), holderGone: true });
  const before = JSON.stringify(job);
  assertThrows(
    () => resolveJob(job, "retry", "", " "),
    Error,
    "needs the agent",
  );
  assertEquals(JSON.stringify(job), before);
});

Deno.test("fdh executor: journals with broken invariants are rejected when read", () => {
  const good = JSON.parse(JSON.stringify(stepJob()));
  assertEquals(
    parseJobJournal(JSON.parse(JSON.stringify(good))).jobId,
    "test-1",
  );
  const broken: Array<[string, (j: Record<string, unknown>) => void]> = [
    ["the last attempt must belong", (j) => (j.attempts = [])],
    ["the last attempt must belong", (j) => (j.epoch = 2)],
    ["step", (j) => (j.step = null)],
    ["lease", (j) => (j.lease = { holder: 1 })],
    ["attempts entry", (j) => (j.attempts = [{ attempt: 1 }])],
  ];
  for (const [message, mutate] of broken) {
    const copy = JSON.parse(JSON.stringify(good));
    mutate(copy);
    assertThrows(() => parseJobJournal(copy), Error, message);
  }
});

Deno.test("fdh executor: invalid outboxes give a clear error; reconcileJobFile reads the outbox under the lock", async () => {
  const dir = await Deno.makeTempDir();
  try {
    await writeNewJob(stepJob(dir));
    await updateJob(
      dir,
      "t1",
      "test-1",
      (j) => beginAttempt(j, 1, "x1", at(0)),
    );
    const file = `${dir}/_dist/fdh/jobs/t1/test-1.outbox-1.json`;
    for (const text of ["null", "{ broken", '{"jobId":"test-1","epoch":1}']) {
      Deno.writeTextFileSync(file, text);
      assertThrows(
        () => readOutbox(dir, "t1", "test-1", 1),
        Error,
        "invalid fdh job outbox",
      );
    }
    writeOutbox(dir, "t1", report(1, 0, 5));
    const { value } = await reconcileJobFile(dir, "t1", "test-1", {
      at: at(5 * TTL),
      currentDigest: "d1",
    });
    assertEquals(value, { action: "applied-outbox", state: "succeeded" });
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh executor run: a heartbeat that cannot be written stops the step", async () => {
  const h = await harness();
  try {
    const run = runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: h.step(0, 60_000),
      heartbeatMs: 2_000,
      now: h.now,
      sleep: h.sleep,
    });
    while (readJob(h.dir, "t1", "test-1").state !== "running") {
      await new Promise((r) => setTimeout(r, 0));
    }
    Deno.writeTextFileSync(
      `${h.dir}/_dist/fdh/jobs/t1/test-1.json`,
      "{ corrupted",
    );
    await assertRejects(() => run);
    assert(h.killed.value);
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});

Deno.test("fdh executor run: heartbeats stay inside a short TTL and log refs use '/'", async () => {
  const h = await harness({ leaseTtlMs: 1_000 });
  try {
    const outcome = await runExecutor({
      worktree: h.dir,
      taskId: "t1",
      jobId: "test-1",
      epoch: 1,
      holder: "x1",
      runStep: h.step(0, 3_000),
      heartbeatMs: 2_000,
      now: h.now,
      sleep: h.sleep,
    });
    assertEquals(outcome, { outcome: "applied", state: "succeeded" });
    assert(h.sleeps.length > 0 && Math.max(...h.sleeps) <= 333, `${h.sleeps}`);
    const job = readJob(h.dir, "t1", "test-1");
    assertEquals(job.attempts[0].logRef, "_dist/fdh/jobs/t1/test-1-1.log");
  } finally {
    await Deno.remove(h.dir, { recursive: true });
  }
});
