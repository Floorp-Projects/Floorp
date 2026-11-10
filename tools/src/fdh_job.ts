// SPDX-License-Identifier: MPL-2.0

/**
 * `fdh job`: durable fdh steps and recorded external intents on one machine
 * (frozen B-02 ADR 8716289, D-23, KAN-129).
 *
 * The controller (`job start|status|wait|reconcile|...`) records a job before
 * anything runs and starts a detached executor process (`job exec`) that owns
 * the step. Any later controller reconciles the journal: it reconnects to a
 * live executor, recovers a result left in the outbox, stops the step tree of
 * a lost executor before it starts the next epoch, puts an intent whose
 * holder is gone into the quota ledger's approval list, and copies every
 * terminal result into the task state once.
 */

import * as path from "@std/path";
import { encodeHex } from "@std/encoding/hex";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import {
  approvePendingEntry,
  pendingQuota,
  quotaLedgerPath,
  readQuotaLedger,
  writeQuotaLedger,
} from "./fdh_quota.ts";
import {
  appendEvent,
  defaultGitRunner,
  type FdhTaskState,
  type GitRunner,
  nowIso,
  readTaskState,
  recordAssertion,
  recordOperation,
  writeTaskState,
} from "./fdh_task.ts";
import { type BuildPhase, buildVerifyCommands } from "./fdh_verify.ts";
import {
  cancelJob,
  confirmIntent,
  createJob,
  executionDigest,
  type IntentOp,
  isTerminal,
  type JobInputs,
  type JobJournal,
  jobsDir,
  listJobIds,
  markRecordedToTask,
  newJobId,
  noteLostStepStopped,
  parseExecutorFault,
  readJob,
  type ReconcileDecision,
  reconcileJobFile,
  type Resolution,
  resolveJob,
  restartStartClock,
  runExecutor,
  type StepSpec,
  updateJob,
  writeNewJob,
} from "./fdh_executor.ts";
import {
  identifyProcess,
  spawnStepCommand,
  stopProcessTree,
} from "./fdh_job_process.ts";

/** `fdh.ts`, which the detached executor runs as `job exec`. */
const FDH_CLI = path.fromFileUrl(new URL("../fdh.ts", import.meta.url));

export type StepKind = "build" | "test";

/** The commands of a step job: the same ones `verify build|test` runs. */
export function stepSpecFor(
  kind: StepKind,
  cwd: string,
  phase: BuildPhase = "all",
): StepSpec {
  if (kind === "test") {
    return {
      cwd,
      displayCommand: "deno task test:host",
      commands: [{
        argv: ["deno", "task", "test:host"],
        displayCommand: "deno task test:host",
        assertionName: "host-test-exit-code",
        expected: "exit code 0",
      }],
    };
  }
  const commands = buildVerifyCommands(phase).map((spec) => ({
    argv: ["deno", ...spec.args],
    displayCommand: spec.displayCommand,
    assertionName: spec.assertionName,
    expected: spec.expected,
  }));
  return {
    cwd,
    displayCommand: commands.map((c) => c.displayCommand).join(" && "),
    commands,
  };
}

/**
 * Inputs bound into the execution digest: base commit, HEAD, guard env, and
 * a hash of the uncommitted state (tracked changes plus untracked names), so
 * an edit made while a job was lost stops a silent rerun (E-7).
 */
export async function currentInputs(
  state: FdhTaskState,
  git: GitRunner = defaultGitRunner,
): Promise<JobInputs> {
  const head = await git(state.worktree, ["rev-parse", "HEAD"]);
  const diff = await git(state.worktree, ["diff", "HEAD", "--binary"]);
  const untracked = await git(state.worktree, [
    "ls-files",
    "--others",
    "--exclude-standard",
  ]);
  const bytes = new TextEncoder().encode(`${diff}\0${untracked}`);
  const worktreeState = encodeHex(
    await crypto.subtle.digest("SHA-256", bytes),
  );
  return {
    baseCommit: state.baseCommit,
    head,
    guardEnv: { ...(state.setup?.guardEnv ?? {}) },
    worktreeState,
  };
}

/** Detached executor process for `epoch` of a job; returns its pid. */
export function spawnExecutorProcess(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
  env: Record<string, string> = {},
): number {
  const child = new Deno.Command(Deno.execPath(), {
    args: [
      "run",
      "-A",
      FDH_CLI,
      "job",
      "exec",
      "--task-id",
      taskId,
      "--job-id",
      jobId,
      "--epoch",
      String(epoch),
    ],
    cwd: worktree,
    env,
    stdin: "null",
    // The executor outlives its controller, so it must not write to the
    // controller's pipes; it keeps its own log in the jobs directory.
    stdout: "null",
    stderr: "null",
    detached: true,
  }).spawn();
  child.unref();
  return child.pid;
}

export interface StartStepJobOptions {
  state: FdhTaskState;
  kind: StepKind;
  phase?: BuildPhase;
  leaseTtlMs?: number;
  maxAttempts?: number;
  git?: GitRunner;
  spawn?: typeof spawnExecutorProcess;
}

/** Record a step job (attempt 1, epoch 1) and start its executor. */
export async function startStepJob(
  options: StartStepJobOptions,
): Promise<JobJournal> {
  const { state } = options;
  const step = stepSpecFor(options.kind, state.worktree, options.phase);
  const inputs = await currentInputs(state, options.git);
  const job = createJob({
    jobId: newJobId(options.kind),
    taskId: state.taskId,
    worktree: state.worktree,
    kind: options.kind,
    step,
    inputs,
    executionDigest: await executionDigest(options.kind, step, inputs),
    leaseTtlMs: options.leaseTtlMs,
    maxAttempts: options.maxAttempts,
  });
  await writeNewJob(job);
  appendEvent(state, {
    timestamp: nowIso(),
    command: `job start ${job.jobId}`,
    note: `${options.kind}: ${step.displayCommand} (digest ${
      job.executionDigest.slice(0, 12)
    })`,
    exitCode: null,
  });
  writeTaskState(state);
  (options.spawn ?? spawnExecutorProcess)(
    state.worktree,
    state.taskId,
    job.jobId,
    1,
  );
  return job;
}

/** Executor log for one epoch (the executor's own errors, not the step's). */
export function executorLogPath(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
): string {
  return path.join(
    jobsDir(worktree, taskId),
    `${jobId}-${epoch}.executor.log`,
  );
}

/** `job exec`: run one epoch of a job as its executor, then settle it. */
export async function runExecutorProcess(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
): Promise<string> {
  const holder = `executor-${Deno.pid}-${
    encodeHex(crypto.getRandomValues(new Uint8Array(3)))
  }`;
  const log = executorLogPath(worktree, taskId, jobId, epoch);
  const note = (line: string) => {
    try {
      Deno.writeTextFileSync(log, `${nowIso()} ${line}\n`, { append: true });
    } catch {
      // the executor log is best-effort
    }
  };
  note(`${holder} starting epoch ${epoch}`);
  try {
    const job = readJob(worktree, taskId, jobId);
    // Fault injection needs an explicit second switch, so a stray
    // FDH_JOB_FAULT in an agent's shell never reaches a real executor.
    const fault = Deno.env.get("FDH_JOB_FAULT_ENABLE") === "1"
      ? parseExecutorFault(Deno.env.get("FDH_JOB_FAULT"))
      : null;
    if (fault !== null) note(`fault injection: ${JSON.stringify(fault)}`);
    const outcome = await runExecutor({
      worktree,
      taskId,
      jobId,
      epoch,
      holder,
      fault,
      runStep: (command, cwd, logPath) =>
        spawnStepCommand(command, cwd, logPath, job.inputs.guardEnv),
      identify: identifyProcess,
    });
    note(`${holder} finished: ${JSON.stringify(outcome)}`);
    await settleJob(worktree, taskId, jobId);
    return outcome.outcome;
  } catch (error) {
    note(`${holder} error: ${error instanceof Error ? error.stack : error}`);
    throw error;
  }
}

function tailOf(text: string, max = 4000): string {
  return text.length <= max ? text : `…${text.slice(text.length - max)}`;
}

/**
 * Copy a terminal job's result into the task state, once: one operation and
 * one assertion per step command result, and one `job result` event. The
 * journal is marked afterwards; a crash in between is caught by the event
 * marker, so a retry does not record the result twice.
 */
export async function settleJob(
  worktree: string,
  taskId: string,
  jobId: string,
): Promise<boolean> {
  // The whole copy runs under the journal lock with `recordedToTask` as the
  // guard, so two settlers (the executor after its ACK and a controller) do
  // not both record. Lock order: journal, then task state.
  const { value } = await updateJob(worktree, taskId, jobId, (job) => {
    if (!isTerminal(job.state) || job.recordedToTask) return false;
    copyResultToTask(worktree, taskId, job);
    markRecordedToTask(job);
    return true;
  });
  return value;
}

/**
 * Copy one terminal job into the task state: an operation and an assertion
 * per command result, a failed assertion for each command that never
 * produced a result (so evidence cannot fall back to an older pass), and one
 * `job result <id>` event that marks the copy as done.
 */
function copyResultToTask(
  worktree: string,
  taskId: string,
  job: JobJournal,
): void {
  const marker = `job result ${job.jobId}`;
  const state = readTaskState(worktree, taskId);
  if (!state.events.some((e) => e.command === marker)) {
    for (const result of job.commandResults) {
      const command = job.step?.commands[result.index];
      if (command === undefined) continue;
      let output = "";
      if (result.logRef !== null) {
        try {
          output = tailOf(Deno.readTextFileSync(
            path.join(worktree, ...result.logRef.split("/")),
          ));
        } catch {
          output = `(log ${result.logRef} unavailable)`;
        }
      }
      recordOperation(state, {
        command: command.argv[0],
        args: command.argv.slice(1),
        displayCommand: command.displayCommand,
        cwd: job.step?.cwd ?? worktree,
        startedAt: result.startedAt,
        endedAt: result.endedAt,
        exitCode: result.exitCode,
        output,
      });
      if (command.assertionName !== undefined) {
        recordAssertion(state, {
          name: command.assertionName,
          expected: command.expected ?? "exit code 0",
          actual: `exit code ${result.exitCode}`,
          passed: result.exitCode === 0,
          at: result.endedAt,
        });
      }
    }
    // Commands without a result (the job failed, was given up or cancelled
    // before they ran or finished) are recorded as not verified.
    for (const [index, command] of (job.step?.commands ?? []).entries()) {
      if (command.assertionName === undefined) continue;
      if (job.commandResults.some((r) => r.index === index)) continue;
      recordAssertion(state, {
        name: command.assertionName,
        expected: command.expected ?? "exit code 0",
        actual:
          `no result: job ${job.jobId} ended ${job.state} before this command completed`,
        passed: false,
        at: job.updatedAt,
      });
    }
    const lost = job.attempts.filter((a) => a.result === "lost").length;
    appendEvent(state, {
      timestamp: nowIso(),
      command: marker,
      note: `${job.kind} ${job.state} after ${job.attempts.length} attempt(s)` +
        `${lost > 0 ? `, ${lost} lost` : ""}, epoch ${job.epoch}`,
      exitCode: job.state === "succeeded" ? 0 : 1,
    });
    writeTaskState(state);
  }
}

/** Note text that links a quota pending entry to its job. */
export function quotaNoteFor(job: JobJournal): string {
  return `job ${job.jobId}: ${job.intent?.target ?? ""}`.trim();
}

/** Same file path after resolving; case-insensitive on Windows only. */
function samePath(a: string, b: string): boolean {
  const norm = (p: string) => {
    const resolved = path.resolve(p);
    return Deno.build.os === "windows" ? resolved.toLowerCase() : resolved;
  };
  return norm(a) === norm(b);
}

async function withLedger<T>(
  worktree: string,
  taskId: string,
  fn: (ledger: ReturnType<typeof readQuotaLedger>) => T,
): Promise<T> {
  const file = quotaLedgerPath(worktree, taskId);
  return await withFileLock(lockPathFor(file), () => {
    const ledger = readQuotaLedger(worktree, taskId, true);
    // writeQuotaLedger writes to the path the ledger itself records: it must
    // be the locked file (same guard as 'fdh quota').
    if (
      ledger.taskId !== taskId ||
      !samePath(quotaLedgerPath(ledger.worktree, ledger.taskId), file)
    ) {
      throw new Error(
        `quota ledger ${file} records another task or worktree (${ledger.taskId} in ${ledger.worktree}); refusing to update it`,
      );
    }
    const value = fn(ledger);
    writeQuotaLedger(ledger);
    return value;
  });
}

export interface ReconcileReport {
  jobId: string;
  kind: string;
  state: string;
  epoch: number;
  action: ReconcileDecision["action"] | "error";
  /** Why this job could not be reconciled (the sweep continues). */
  error?: string;
  /** Processes of a lost attempt that were stopped before the respawn. */
  stopped?: number;
  /** Executor pid started for a new epoch. */
  executorPid?: number;
}

export interface ReconcileTaskOptions {
  state: FdhTaskState;
  jobIds?: string[];
  /** Holders of intents are gone (`task resume`). Steps are never assumed. */
  intentHoldersGone?: boolean;
  git?: GitRunner;
  spawn?: typeof spawnExecutorProcess;
  stopTree?: typeof stopProcessTree;
}

/**
 * Reconcile jobs of a task and act on each decision (respawn, approval
 * list, task-state record). The digest is computed outside the journal lock.
 */
export async function reconcileTaskJobs(
  options: ReconcileTaskOptions,
): Promise<ReconcileReport[]> {
  const { state } = options;
  const ids = options.jobIds ?? listJobIds(state.worktree, state.taskId);
  const reports: ReconcileReport[] = [];
  // The inputs are computed once per sweep (three git calls), outside any lock.
  const inputsCache: { value: JobInputs | null } = { value: null };
  for (const jobId of ids) {
    try {
      reports.push(
        await reconcileOne(options, jobId, inputsCache),
      );
    } catch (error) {
      // One damaged journal must not stop the sweep over the task's jobs
      // (task resume still moves the other intents to awaiting-approval).
      reports.push({
        jobId,
        kind: "unknown",
        state: "unknown",
        epoch: 0,
        action: "error",
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }
  return reports;
}

async function reconcileOne(
  options: ReconcileTaskOptions,
  jobId: string,
  inputsCache: { value: JobInputs | null },
): Promise<ReconcileReport> {
  const { state } = options;
  {
    const before = readJob(state.worktree, state.taskId, jobId);
    let currentDigest: string | undefined;
    if (before.kind !== "intent" && !isTerminal(before.state)) {
      inputsCache.value ??= await currentInputs(state, options.git);
      const inputs = inputsCache.value;
      currentDigest = await executionDigest(before.kind, before.step!, inputs);
    }
    const { job, value } = await reconcileJobFile(
      state.worktree,
      state.taskId,
      jobId,
      {
        currentDigest: before.kind === "intent" ? undefined : currentDigest ??
          before.executionDigest,
        holderGone: before.kind === "intent" &&
          options.intentHoldersGone === true,
      },
    );
    const report: ReconcileReport = {
      jobId,
      kind: job.kind,
      state: job.state,
      epoch: job.epoch,
      action: value.action,
    };
    // Stop what any lost (or, after a cancel, abandoned) attempt left
    // running and has not been stopped yet: before the next epoch runs the
    // same step, when the job closes, and on any later reconcile, so a
    // respawn that was interrupted (controller killed after the epoch bump)
    // does not leave an older step tree running for good.
    const pending = job.attempts.filter((a) =>
      (a.result === "lost" || a.result === "cancelled") && a.process != null &&
      !job.events.some((e) =>
        e.kind === "lost-step-stopped" && e.epoch === a.epoch
      )
    );
    try {
      for (const lost of pending) {
        const root = lost.process!;
        const stopped = await (options.stopTree ?? stopProcessTree)(
          root,
          lostTreeBound(lost, job.leaseTtlMs),
        );
        report.stopped = (report.stopped ?? 0) + stopped.length;
        await updateJob(
          state.worktree,
          state.taskId,
          jobId,
          (j) => noteLostStepStopped(j, lost.epoch, root, stopped.length),
        );
      }
    } catch (error) {
      // Without the stop the next epoch could run beside the old tree: do not
      // spawn now. A later reconcile retries the stop (the new attempt is
      // then overdue and counted as lost).
      report.error = `could not stop a lost step tree: ${
        error instanceof Error ? error.message : String(error)
      }`;
      // A closed job still gets its result recorded; the stop is retried by
      // the next reconcile.
      if (isTerminal(job.state)) {
        await settleJob(state.worktree, state.taskId, jobId);
      }
      return report;
    }
    if (value.action === "respawn") {
      // The stop can take seconds (PowerShell on Windows): start the new
      // attempt's start deadline now, not at the epoch bump.
      await updateJob(
        state.worktree,
        state.taskId,
        jobId,
        (j) => restartStartClock(j, value.epoch),
      );
      report.executorPid = (options.spawn ?? spawnExecutorProcess)(
        state.worktree,
        state.taskId,
        jobId,
        value.epoch,
      );
    }
    if (value.action === "awaiting-approval" && job.intent !== null) {
      await withLedger(
        state.worktree,
        state.taskId,
        (ledger) =>
          pendingQuota(ledger, job.intent!.op as IntentOp, quotaNoteFor(job)),
      );
    }
    if (isTerminal(job.state)) {
      await settleJob(state.worktree, state.taskId, jobId);
    }
    return report;
  }
}

export interface WaitOptions extends ReconcileTaskOptions {
  jobId: string;
  timeoutMs: number;
  pollMs?: number;
  sleep?: (ms: number) => Promise<void>;
  /** Consecutive reconcile errors before the wait gives up (default 3). */
  maxErrors?: number;
  /** Called with each reconcile error (the CLI prints it to stderr). */
  onError?: (message: string) => void;
}

/** Reconcile one job until it is terminal or awaits approval. */
export async function waitForJob(options: WaitOptions): Promise<JobJournal> {
  const sleep = options.sleep ??
    ((ms: number) => new Promise<void>((r) => setTimeout(r, ms)));
  const deadline = Date.now() + options.timeoutMs;
  let errors = 0;
  for (;;) {
    const [report] = await reconcileTaskJobs({
      ...options,
      jobIds: [options.jobId],
    });
    if (report?.error !== undefined) {
      // A reconcile that keeps failing (git unusable, a tree that cannot be
      // stopped) would otherwise leave a lost job untouched until timeout.
      errors += 1;
      options.onError?.(report.error);
      if (errors >= (options.maxErrors ?? 3)) {
        throw new Error(
          `job ${options.jobId} could not be reconciled ${errors} times in a row: ${report.error}`,
        );
      }
    } else {
      errors = 0;
    }
    const job = readJob(
      options.state.worktree,
      options.state.taskId,
      options.jobId,
    );
    if (isTerminal(job.state) || job.state === "awaiting-approval") {
      return job;
    }
    if (Date.now() >= deadline) {
      throw new Error(
        `job ${options.jobId} is still ${job.state} (epoch ${job.epoch}) after ${options.timeoutMs} ms; run 'fdh job wait' again`,
      );
    }
    await sleep(options.pollMs ?? 1_000);
  }
}

/** Record an intent for a non-idempotent operation the agent will perform. */
export async function startIntent(
  state: FdhTaskState,
  op: IntentOp,
  target: string,
  agent: string,
  leaseTtlMs?: number,
): Promise<JobJournal> {
  const intent = { op, target };
  const inputs: JobInputs = {
    baseCommit: state.baseCommit,
    head: "",
    guardEnv: {},
  };
  const job = createJob({
    jobId: newJobId("intent"),
    taskId: state.taskId,
    worktree: state.worktree,
    kind: "intent",
    intent,
    inputs,
    executionDigest: await executionDigest("intent", intent, inputs),
    holder: agent,
    leaseTtlMs,
  });
  await writeNewJob(job);
  appendEvent(state, {
    timestamp: nowIso(),
    command: `job intent ${job.jobId}`,
    note: `${op} ${target} by ${agent.trim()}`,
    exitCode: null,
  });
  writeTaskState(state);
  return job;
}

/** The agent reports the outcome of its intent (fenced by epoch). */
export async function confirmJob(
  state: FdhTaskState,
  jobId: string,
  epoch: number,
  agent: string,
  outcome: "succeeded" | "failed",
) {
  const { job, value } = await updateJob(
    state.worktree,
    state.taskId,
    jobId,
    (j) => confirmIntent(j, epoch, agent, outcome),
  );
  if (isTerminal(job.state)) {
    await settleJob(state.worktree, state.taskId, jobId);
  }
  return { job, value };
}

/**
 * An approver resolves a job awaiting approval; its quota pending entry is
 * approved with it.
 */
export async function resolveAndApprove(
  state: FdhTaskState,
  jobId: string,
  resolution: Resolution,
  note: string,
  agent?: string,
): Promise<JobJournal> {
  const { job } = await updateJob(
    state.worktree,
    state.taskId,
    jobId,
    (j) => resolveJob(j, resolution, note, agent),
  );
  if (job.intent !== null) {
    await withLedger(
      state.worktree,
      state.taskId,
      (ledger) =>
        approvePendingEntry(
          ledger,
          job.intent!.op,
          quotaNoteFor(job),
          `resolved ${resolution}`,
        ),
    );
  }
  if (isTerminal(job.state)) {
    await settleJob(state.worktree, state.taskId, jobId);
  }
  return job;
}

/** Request a cancel; a settled result is copied into the task state. */
export async function cancelAndSettle(
  state: FdhTaskState,
  jobId: string,
  reason: string,
  options: Omit<ReconcileTaskOptions, "state" | "jobIds"> = {},
): Promise<JobJournal> {
  await updateJob(
    state.worktree,
    state.taskId,
    jobId,
    (j) => cancelJob(j, reason),
  );
  // Reconcile at once: it stops any lost tree left from an earlier epoch
  // (e.g. an interrupted respawn) and records a closed job's result.
  await reconcileTaskJobs({ ...options, state, jobIds: [jobId] });
  return readJob(state.worktree, state.taskId, jobId);
}

/**
 * Latest start time a child of a lost root may have: the holder's last sign
 * of life plus one TTL (the loss may be noticed long after it happened).
 */
export function lostTreeBound(
  attempt: { lastSeenAt?: string; endedAt: string | null },
  leaseTtlMs: number,
): string | undefined {
  const seen = attempt.lastSeenAt !== undefined
    ? Date.parse(attempt.lastSeenAt)
    : NaN;
  if (Number.isFinite(seen)) {
    const bound = seen + leaseTtlMs;
    const ended = attempt.endedAt === null ? NaN : Date.parse(attempt.endedAt);
    return new Date(Number.isFinite(ended) ? Math.min(bound, ended) : bound)
      .toISOString();
  }
  return attempt.endedAt ?? undefined;
}

/** Compact JSON view of a job for the CLI. */
export function summarizeJob(job: JobJournal) {
  return {
    jobId: job.jobId,
    kind: job.kind,
    state: job.state,
    epoch: job.epoch,
    checkpoint: job.checkpoint,
    commands: job.step?.commands.length ?? 0,
    attempts: job.attempts.map((a) => ({
      attempt: a.attempt,
      epoch: a.epoch,
      result: a.result,
      holder: a.holder,
      exitCode: a.exitCode,
    })),
    intent: job.intent,
    recordedToTask: job.recordedToTask,
    events: job.events.length,
    /** Journal, outbox, step logs and executor logs live here. */
    jobsDir: jobsDir(job.worktree, job.taskId),
  };
}
