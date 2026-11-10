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
import { assertSourceBuildAllowed } from "./fdh_dev_guard.ts";
import {
  cancelJob,
  confirmIntent,
  createJob,
  executionDigest,
  holdRespawn,
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
  readOutbox,
  type ReconcileContext,
  type ReconcileDecision,
  reconcileJob,
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
    "-z",
  ]);
  // Untracked files count by content, so editing an untracked source while a
  // job was lost also stops a silent rerun (E-7).
  const untrackedState = await hashUntracked(
    state.worktree,
    untracked.split("\0").filter((name) => name !== ""),
  );
  const bytes = new TextEncoder().encode(`${diff}\0${untrackedState}`);
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

/** Untracked files larger than this count by size and mtime, not content. */
export const MAX_HASHED_UNTRACKED_BYTES = 16 * 1024 * 1024;

/**
 * One line per untracked file: its name and the SHA-256 of its content (or
 * its size and mtime when it is larger than MAX_HASHED_UNTRACKED_BYTES, or
 * "missing" when it vanished meanwhile), sorted by name.
 */
export async function hashUntracked(
  worktree: string,
  names: string[],
): Promise<string> {
  const lines: string[] = [];
  for (const name of [...names].sort()) {
    const file = path.join(worktree, ...name.split("/"));
    let entry: string;
    let info: Deno.FileInfo | null = null;
    try {
      info = Deno.statSync(file);
      const stamp = `${info.size}:${info.mtime?.toISOString() ?? ""}`;
      if (!info.isFile) {
        // An untracked directory (git reports "sub/") counts by size only.
        entry = `other:${info.size}`;
      } else if (info.size > MAX_HASHED_UNTRACKED_BYTES) {
        entry = `large:${stamp}`;
      } else {
        // `job wait` reconciles every second: hash a file again only when
        // its size or mtime changed.
        const key = `${file}\0${stamp}`;
        entry = untrackedHashCache.get(key) ?? encodeHex(
          await crypto.subtle.digest("SHA-256", Deno.readFileSync(file)),
        );
        untrackedHashCache.set(key, entry);
      }
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) {
        entry = "missing";
      } else {
        // Locked by another process (Windows sharing violation) or not
        // permitted: do not stop reconciling; count it by size and mtime.
        entry = info === null
          ? "unreadable"
          : `unreadable:${info.size}:${info.mtime?.toISOString() ?? ""}`;
      }
    }
    lines.push(`${name}\0${entry}`);
  }
  return lines.join("\n");
}

/** SHA-256 of untracked files by path, size and mtime (this process only). */
const untrackedHashCache = new Map<string, string>();

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
  /** "held": a source-build respawn waits for the W-04 dev guard. */
  action: ReconcileDecision["action"] | "error" | "held";
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
  /** W-04 guard checked before a respawn that compiles source. */
  sourceBuildGuard?: (worktree: string) => Promise<void>;
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
    const context = {
      currentDigest: before.kind === "intent" ? undefined : currentDigest ??
        before.executionDigest,
      holderGone: before.kind === "intent" &&
        options.intentHoldersGone === true,
    };
    // Same W-04 guard as 'verify build' / 'job start': a respawn must not
    // start a source build under this worktree's running dev browser. The
    // decision is taken once, under the journal lock. The guard (it lists
    // processes, so it is slow) runs outside the lock, only when that
    // decision is a source-compiling respawn; the locked decision is then
    // taken again with its answer.
    let allowed: true | string | undefined;
    let locked: { job: JobJournal; value: LockedDecision };
    for (;;) {
      try {
        locked = await lockedReconcile(state, jobId, context, allowed);
        break;
      } catch (error) {
        if (!(error instanceof GuardNeeded) || allowed !== undefined) {
          throw error;
        }
        try {
          await (options.sourceBuildGuard ??
            ((worktree) => assertSourceBuildAllowed(worktree, "before-mach")))(
              state.worktree,
            );
          allowed = true;
        } catch (refusal) {
          allowed = refusal instanceof Error
            ? refusal.message
            : String(refusal);
        }
      }
    }
    const { job, value } = locked;
    const report: ReconcileReport = {
      jobId,
      kind: job.kind,
      state: job.state,
      epoch: job.epoch,
      action: value.action,
    };
    if (value.action === "held") {
      // The holder is now fenced (declared lost, no epoch bump): its step
      // tree is stopped below like any lost attempt's, so the old build does
      // not keep running under the dev server while the job is held.
      report.error = `held, not respawned: ${value.reason}`;
    }
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
      const stopError = `could not stop a lost step tree: ${
        error instanceof Error ? error.message : String(error)
      }`;
      report.error = report.error === undefined
        ? stopError
        : `${report.error}; ${stopError}`;
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
    if (job.state === "awaiting-approval" && job.intent !== null) {
      // Put (or put back) the approval entry. A controller may have died
      // between the journal transition and the ledger write. An approval
      // recorded in the ledger since the job began waiting is not undone.
      const since = [...job.transitions].reverse().find((t) =>
        t.to === "awaiting-approval"
      )?.at ?? job.updatedAt;
      await withLedger(state.worktree, state.taskId, (ledger) => {
        // Re-read the journal under the ledger lock: a 'job resolve' that ran
        // since must not get a stale entry back.
        if (
          readJob(state.worktree, state.taskId, jobId).state !==
            "awaiting-approval"
        ) {
          return;
        }
        const note = quotaNoteFor(job);
        const after = (e: { at: string }, t: string) =>
          Date.parse(e.at) >= Date.parse(t);
        // Approved: this entry's own approval, or an approve-all ('quota
        // approve') after this entry was put on the list.
        const ownApproval = ledger.events.some((e) =>
          e.kind === "approve" && after(e, since) &&
          (e.note ?? "").includes(note)
        );
        const listedAt = [...ledger.events].reverse().find((e) =>
          e.kind === "pending" && e.note === note && after(e, since)
        )?.at;
        const approvedAll = listedAt !== undefined &&
          ledger.events.some((e) =>
            e.kind === "approve" && e.op === undefined && after(e, listedAt)
          );
        if (!ownApproval && !approvedAll) {
          pendingQuota(ledger, job.intent!.op as IntentOp, note);
        }
      });
    }
    if (isTerminal(job.state)) {
      await settleJob(state.worktree, state.taskId, jobId);
    }
    return report;
  }
}

type LockedDecision = ReconcileDecision | { action: "held"; reason: string };

/** The locked decision is a source-compiling respawn the guard must clear. */
class GuardNeeded extends Error {
  constructor() {
    super("the respawn compiles source: the W-04 dev guard must clear it");
  }
}

/**
 * Reconcile one journal under its lock (reading the current epoch's outbox
 * inside it, as `reconcileJobFile` does). A respawn that would compile
 * source is committed only if `allowed` is true; with no answer yet it
 * throws `GuardNeeded` and nothing is written; refused (a message), the
 * respawn is held instead (`holdRespawn`).
 */
async function lockedReconcile(
  state: FdhTaskState,
  jobId: string,
  context: Omit<ReconcileContext, "outbox" | "at">,
  allowed: true | string | undefined,
): Promise<{ job: JobJournal; value: LockedDecision }> {
  return await updateJob(state.worktree, state.taskId, jobId, (job) => {
    const at = nowIso();
    // A settled job never uses its outbox; do not let a damaged one stop a
    // sweep over the task's jobs.
    const outbox = isTerminal(job.state) || job.state === "awaiting-approval"
      ? null
      : readOutbox(state.worktree, state.taskId, jobId, job.epoch);
    const compiles = respawnCompilesSource(job);
    const trial = compiles ? structuredClone(job) : job;
    const value: LockedDecision = reconcileJob(trial, {
      ...context,
      at,
      outbox,
    });
    if (!compiles) return value;
    if (value.action !== "respawn" || allowed === true) {
      Object.assign(job, trial);
      return value;
    }
    if (allowed === undefined) throw new GuardNeeded();
    holdRespawn(job, allowed, at);
    return { action: "held", reason: allowed };
  });
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
        if (report.action === "held") {
          throw new Error(
            `job ${options.jobId} is held: its source build waits for this worktree's dev server to stop. Run 'fdh dev down', then 'fdh job wait' again (the job keeps its attempts). ${report.error}`,
          );
        }
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
    (j) => {
      // A retry after the ledger update below failed: the journal is already
      // resolved the same way, so only the ledger step is repeated.
      if (alreadyResolvedAs(j, resolution)) return;
      resolveJob(j, resolution, note, agent);
    },
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
): Promise<{ job: JobJournal; reports: ReconcileReport[] }> {
  await updateJob(
    state.worktree,
    state.taskId,
    jobId,
    (j) => cancelJob(j, reason),
  );
  // Reconcile at once: it stops any lost tree left from an earlier epoch
  // (e.g. an interrupted respawn) and records a closed job's result.
  const reports = await reconcileTaskJobs({
    ...options,
    state,
    jobIds: [jobId],
  });
  return { job: readJob(state.worktree, state.taskId, jobId), reports };
}

/**
 * Was `job` already resolved with `resolution` by an approver? (The last
 * "resolved" event says so and the job is past awaiting-approval.)
 */
export function alreadyResolvedAs(
  job: JobJournal,
  resolution: Resolution,
): boolean {
  // Only a closing resolution is repeatable: the job's state is its result.
  // A repeated 'retry' would silently ignore a different --agent.
  if (resolution === "retry" || job.state !== resolution) return false;
  const last = [...job.events].reverse().find((e) => e.kind === "resolved");
  return last !== undefined &&
    last.note.startsWith(`resolved ${resolution} by approver`);
}

/** Would a respawn of `job` start at a command that compiles source? */
export function respawnCompilesSource(job: JobJournal): boolean {
  if (job.kind !== "build" || job.step === null) return false;
  return job.step.commands.slice(job.checkpoint).some((command) =>
    command.argv.includes("before-mach")
  );
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
