// SPDX-License-Identifier: MPL-2.0

/**
 * fdh one-machine fault-tolerant Executor journal (B-02 draft ADR, D-16,
 * KAN-129).
 *
 * A Job is one long-running fdh step (`verify build`, `verify test`) or one
 * non-idempotent external operation an agent performs outside fdh (an
 * "intent"). Its journal lives at `<worktree>/_dist/fdh/jobs/<task>/<job>.json`
 * and is the durable record of who runs it:
 *
 *   - run/attempt/epoch and the execution digest are written BEFORE the
 *     executor process starts (persist-before-start);
 *   - the executor holds a time-based lease and renews it with heartbeats;
 *   - every executor write carries its epoch (fencing token) and is applied
 *     only when it equals the journal's epoch; an older epoch is rejected and
 *     recorded as `stale-epoch`;
 *   - the executor writes its result to an outbox file first and then applies
 *     it to the journal (the ACK), so a crash in between is recovered by
 *     `reconcileJob` without rerunning the step.
 *
 * Job states use the frozen Envelope state-transition table
 * (`tools/qa3/evidence/state-transitions.json`, ADR 8061068 section 1.2); the
 * table is not changed. Losing an attempt keeps the Job `running` and records
 * the loss on the attempt (`lost`), then a new attempt runs at epoch + 1.
 *
 * This module is pure apart from the small journal/outbox I/O helpers: the
 * clock, the executor process and the step command are injected, so every
 * B-02 test case (E-1..E-9) is exercised without real processes. It does not
 * claim exactly-once execution: a safe step may run again after its executor
 * is lost, and an old executor's local writes cannot be undone.
 */

import * as path from "@std/path";
import { join as posixJoin } from "@std/path/posix";
import { encodeHex } from "@std/encoding/hex";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import { FDH_DIR, nowIso, validateTaskId } from "./fdh_task.ts";
import type { State } from "../qa3/evidence/types.ts";
import { loadTransitions } from "../qa3/evidence/validate.ts";

export const FDH_JOBS_DIR = path.join(FDH_DIR, "jobs");

/** Heartbeat period and lease TTL defaults (milliseconds). */
export const DEFAULT_HEARTBEAT_MS = 2_000;
export const DEFAULT_LEASE_TTL_MS = 10_000;
/** Intents are renewed by the agent, not by heartbeats: a longer lease. */
export const DEFAULT_INTENT_TTL_MS = 30 * 60_000;
export const MIN_LEASE_TTL_MS = 1_000;
/** A safe step is given up (failed) after this many attempts were lost. */
export const DEFAULT_MAX_ATTEMPTS = 3;

export const JOB_KINDS = ["build", "test", "intent"] as const;
export type JobKind = (typeof JOB_KINDS)[number];

/** External operations an intent may stand for (same set as quota pending). */
export const INTENT_OPS = ["push", "pr", "release", "external"] as const;
export type IntentOp = (typeof INTENT_OPS)[number];

export type AttemptResult =
  | "scheduled"
  | "running"
  | "succeeded"
  | "failed"
  | "lost"
  | "cancelled";

export interface JobAttempt {
  attempt: number;
  epoch: number;
  /** Executor (or agent, for an intent) that holds this attempt. */
  holder: string | null;
  scheduledAt: string;
  startedAt: string | null;
  endedAt: string | null;
  result: AttemptResult;
  exitCode: number | null;
  /** Worktree-relative log of the last step command it ran, if any. */
  logRef: string | null;
  /**
   * The step command process this attempt started (root of its tree), so a
   * controller can stop an orphaned step before it runs the job again.
   */
  process: ProcessIdentity | null;
  /** Last time the holder was known alive (set when it is declared lost). */
  lastSeenAt?: string;
  notes: string;
}

/** Identity of a process: pid plus start time and command line (pid reuse). */
export interface ProcessIdentity {
  pid: number;
  startTime: string;
  commandLine: string;
}

export interface JobLease {
  holder: string;
  epoch: number;
  heartbeatAt: string;
  ttlMs: number;
}

/** One command of a safe step. */
export interface StepCommand {
  argv: string[];
  displayCommand: string;
  /** Task-state assertion recorded for this command's exit code. */
  assertionName?: string;
  expected?: string;
}

/**
 * What a safe step runs, in order; recorded in full so a takeover runs the
 * same thing. A takeover resumes at the job's checkpoint (the first command
 * that has not completed successfully).
 */
export interface StepSpec {
  cwd: string;
  displayCommand: string;
  commands: StepCommand[];
}

/** Outcome of one step command, recorded on the job once it is known. */
export interface CommandResult {
  index: number;
  epoch: number;
  exitCode: number;
  startedAt: string;
  endedAt: string;
  logRef: string | null;
}

export interface IntentSpec {
  op: IntentOp;
  target: string;
}

/** Inputs bound into the execution digest. */
export interface JobInputs {
  baseCommit: string;
  head: string;
  guardEnv: Record<string, string>;
  /** Hash of the uncommitted state (tracked diff and untracked names). */
  worktreeState?: string;
}

export interface JobTransition {
  from: State | null;
  to: State;
  at: string;
  reason: string;
}

export type JobEventKind =
  | "stale-epoch"
  | "fenced"
  | "outbox-applied"
  | "lease-expired"
  | "input-changed"
  | "resolved"
  | "late-report"
  | "wrong-holder"
  | "gave-up"
  | "cancel-requested"
  | "cancelled"
  | "heartbeat-failed"
  | "lost-step-stopped";

export interface JobEvent {
  at: string;
  kind: JobEventKind;
  epoch: number;
  note: string;
}

export interface JobJournal {
  schemaVersion: 1;
  jobId: string;
  taskId: string;
  worktree: string;
  kind: JobKind;
  step: StepSpec | null;
  intent: IntentSpec | null;
  inputs: JobInputs;
  /** SHA-256 (hex) of the canonical {kind, step|intent, inputs}. */
  executionDigest: string;
  state: State;
  /** Current fencing token; only the holder of this epoch may write. */
  epoch: number;
  lease: JobLease | null;
  /** Lease TTL used for every attempt of this job. */
  leaseTtlMs: number;
  /** Attempts a safe step may lose before the job fails instead of rerunning. */
  maxAttempts: number;
  attempts: JobAttempt[];
  /** Index of the first step command that has not completed successfully. */
  checkpoint: number;
  /** Results of the step commands, in order (successes, then the failure). */
  commandResults: CommandResult[];
  /** The terminal result has been copied into the task state. */
  recordedToTask: boolean;
  transitions: JobTransition[];
  events: JobEvent[];
  createdAt: string;
  updatedAt: string;
}

/** Result an executor reports for its attempt (also the outbox content). */
export interface JobReport {
  jobId: string;
  epoch: number;
  holder: string;
  exitCode: number;
  endedAt: string;
  logRef: string | null;
  /** Command results of this attempt that were not checkpointed. */
  results: CommandResult[];
}

// ---------------------------------------------------------------------------
// state machine (frozen table)
// ---------------------------------------------------------------------------

const TABLE = loadTransitions();
const TERMINAL = new Set<State>(TABLE.terminalStates);

/**
 * Holders are compared after trimming, at every entry point, so a padded id
 * from a command line names the same holder everywhere. An empty one is an
 * error.
 */
function normalizeHolder(holder: string): string {
  const trimmed = typeof holder === "string" ? holder.trim() : "";
  if (trimmed === "") throw new Error("holder must be a non-empty string");
  return trimmed;
}

/** Parse an ISO time; an unparsable value is a caller error, not "expired". */
function requireTime(value: string, name: string): number {
  const ms = Date.parse(value);
  if (!Number.isFinite(ms)) {
    throw new Error(
      `${name} is not a valid ISO time: ${JSON.stringify(value)}`,
    );
  }
  return ms;
}

/** Is `state` terminal in the frozen table (succeeded/failed/cancelled)? */
export function isTerminal(state: State): boolean {
  return TERMINAL.has(state);
}

/** Move the job to `to`; throws when the frozen table has no such edge. */
function transition(
  job: JobJournal,
  to: State,
  at: string,
  reason: string,
): void {
  const allowed = TABLE.transitions[job.state] ?? [];
  if (!allowed.includes(to)) {
    throw new Error(
      `job ${job.jobId}: transition ${job.state} -> ${to} is not in the state-transition table`,
    );
  }
  job.transitions.push({ from: job.state, to, at, reason });
  job.state = to;
  job.updatedAt = at;
}

/** Append one journal event (rejections, fencing, recoveries). */
function event(
  job: JobJournal,
  kind: JobEventKind,
  at: string,
  epoch: number,
  note: string,
): void {
  job.events.push({ at, kind, epoch, note });
  job.updatedAt = at;
}

// ---------------------------------------------------------------------------
// identity and digest
// ---------------------------------------------------------------------------

/** Job ids share the task-id alphabet and are at most 80 characters. */
export function validateJobId(jobId: string): string {
  if (!/^[a-z0-9][a-z0-9-]*$/.test(jobId) || jobId.length > 80) {
    throw new Error(`invalid job id ${JSON.stringify(jobId)}`);
  }
  return jobId;
}

/** New job id, e.g. `test-20261109T120000z-3fa9c1`. */
export function newJobId(kind: JobKind, at = nowIso()): string {
  const stamp = at.replace(/[-:]/g, "").replace(/\.\d+/, "").toLowerCase();
  const random = encodeHex(crypto.getRandomValues(new Uint8Array(3)));
  return validateJobId(`${kind}-${stamp}-${random}`);
}

/** Key-sorted JSON so equal values always hash to the same digest. */
function canonical(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonical).join(",")}]`;
  if (value !== null && typeof value === "object") {
    const entries = Object.entries(value as Record<string, unknown>)
      .filter(([, v]) => v !== undefined)
      .sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0));
    return `{${
      entries.map(([k, v]) => `${JSON.stringify(k)}:${canonical(v)}`).join(",")
    }}`;
  }
  return JSON.stringify(value);
}

/** SHA-256 (hex) of the canonical {kind, spec, inputs} of a job. */
export async function executionDigest(
  kind: JobKind,
  spec: StepSpec | IntentSpec,
  inputs: JobInputs,
): Promise<string> {
  const bytes = new TextEncoder().encode(canonical({ kind, spec, inputs }));
  return encodeHex(await crypto.subtle.digest("SHA-256", bytes));
}

// ---------------------------------------------------------------------------
// pure transitions
// ---------------------------------------------------------------------------

export interface CreateJobInput {
  jobId: string;
  taskId: string;
  worktree: string;
  kind: JobKind;
  step?: StepSpec;
  intent?: IntentSpec;
  inputs: JobInputs;
  executionDigest: string;
  leaseTtlMs?: number;
  maxAttempts?: number;
  /** Intents start held by this agent; steps start unheld. */
  holder?: string;
  at?: string;
}

/**
 * Create a job with attempt 1 at epoch 1, before anything runs. A step job is
 * `scheduled` (its executor has not started yet); an intent is `running`,
 * held by the agent that is about to perform the external operation.
 */
export function createJob(input: CreateJobInput): JobJournal {
  validateTaskId(input.taskId);
  validateJobId(input.jobId);
  if (!JOB_KINDS.includes(input.kind)) {
    throw new Error(`job kind must be one of ${JOB_KINDS.join("|")}`);
  }
  const isIntent = input.kind === "intent";
  if (
    isIntent !== (input.intent !== undefined) ||
    (isIntent && input.step !== undefined)
  ) {
    throw new Error(
      "an intent job needs an intent spec, a step job needs none",
    );
  }
  if (!isIntent && input.step === undefined) {
    throw new Error(`a ${input.kind} job needs a step spec`);
  }
  if (!isIntent && !isValidStep(input.step)) {
    throw new Error(
      "a step spec needs a string cwd and displayCommand and at least one command with a non-empty string argv",
    );
  }
  if (isIntent && !INTENT_OPS.includes(input.intent!.op)) {
    throw new Error(`intent op must be one of ${INTENT_OPS.join("|")}`);
  }
  if (isIntent && (input.holder ?? "").trim() === "") {
    throw new Error("an intent needs the agent that performs it (--agent)");
  }
  const ttl = input.leaseTtlMs ??
    (isIntent ? DEFAULT_INTENT_TTL_MS : DEFAULT_LEASE_TTL_MS);
  if (!Number.isInteger(ttl) || ttl < MIN_LEASE_TTL_MS) {
    throw new Error(`lease TTL must be an integer >= ${MIN_LEASE_TTL_MS} ms`);
  }
  const maxAttempts = input.maxAttempts ?? DEFAULT_MAX_ATTEMPTS;
  if (!Number.isInteger(maxAttempts) || maxAttempts < 1) {
    throw new Error("max attempts must be a positive integer");
  }
  const at = input.at ?? nowIso();
  requireTime(at, "creation time");
  const job: JobJournal = {
    schemaVersion: 1,
    jobId: input.jobId,
    taskId: input.taskId,
    worktree: input.worktree,
    kind: input.kind,
    // Copies: a caller that reuses its spec objects cannot change a recorded
    // job behind its digest.
    step: input.step === undefined ? null : {
      ...input.step,
      commands: input.step.commands.map((command) => ({
        ...command,
        argv: [...command.argv],
      })),
    },
    intent: input.intent === undefined ? null : { ...input.intent },
    inputs: { ...input.inputs, guardEnv: { ...input.inputs.guardEnv } },
    executionDigest: input.executionDigest,
    state: "pending",
    epoch: 1,
    lease: null,
    leaseTtlMs: ttl,
    maxAttempts,
    attempts: [],
    checkpoint: 0,
    commandResults: [],
    recordedToTask: false,
    transitions: [{ from: null, to: "pending", at, reason: "created" }],
    events: [],
    createdAt: at,
    updatedAt: at,
  };
  transition(job, "scheduled", at, "attempt 1 recorded before start");
  job.attempts.push({
    attempt: 1,
    epoch: 1,
    holder: null,
    scheduledAt: at,
    startedAt: null,
    endedAt: null,
    result: "scheduled",
    exitCode: null,
    logRef: null,
    process: null,
    notes: "",
  });
  if (isIntent) {
    startAttempt(job, 1, input.holder!.trim(), at);
  }
  return job;
}

/** The attempt of the current epoch (the journal invariant: the last one). */
function currentAttempt(job: JobJournal): JobAttempt {
  const attempt = job.attempts[job.attempts.length - 1];
  if (attempt === undefined || attempt.epoch !== job.epoch) {
    throw new Error(`job ${job.jobId} has no attempt for epoch ${job.epoch}`);
  }
  return attempt;
}

/** The attempt that ran at `epoch`, if any. */
function attemptAt(job: JobJournal, epoch: number): JobAttempt | undefined {
  return job.attempts.find((attempt) => attempt.epoch === epoch);
}

export type BeginResult =
  | { ok: true }
  | {
    ok: false;
    reason: "stale-epoch" | "not-scheduled" | "terminal" | "cancel-requested";
  };

/**
 * The executor of `epoch` takes its attempt: only when that epoch is the
 * journal's epoch and the attempt has not started. Otherwise nothing changes
 * except a recorded `stale-epoch` event, and the executor must exit.
 */
export function beginAttempt(
  job: JobJournal,
  epoch: number,
  rawHolder: string,
  at = nowIso(),
): BeginResult {
  requireTime(at, "begin time");
  const holder = normalizeHolder(rawHolder);
  if (isTerminal(job.state)) return { ok: false, reason: "terminal" };
  if (job.state === "cancel_requested") {
    return { ok: false, reason: "cancel-requested" };
  }
  if (epoch !== job.epoch) {
    event(job, "stale-epoch", at, epoch, `begin by ${holder} refused`);
    return { ok: false, reason: "stale-epoch" };
  }
  const attempt = currentAttempt(job);
  if (attempt.result !== "scheduled") {
    return { ok: false, reason: "not-scheduled" };
  }
  startAttempt(job, epoch, holder, at);
  return { ok: true };
}

/** Mark the current attempt running, held by `holder`, with a fresh lease. */
function startAttempt(
  job: JobJournal,
  epoch: number,
  holder: string,
  at: string,
): void {
  const attempt = currentAttempt(job);
  attempt.holder = holder;
  attempt.startedAt = at;
  attempt.result = "running";
  if (job.state === "scheduled") {
    transition(job, "running", at, `attempt ${attempt.attempt} started`);
  }
  job.lease = { holder, epoch, heartbeatAt: at, ttlMs: job.leaseTtlMs };
  job.updatedAt = at;
}

export type HeartbeatResult = "renewed" | "fenced" | "cancel";

/**
 * Renew the lease. `fenced` means a newer epoch took over: stop the step.
 * `cancel` means a cancel was requested: stop the step and `finishCancel`.
 */
export function heartbeat(
  job: JobJournal,
  epoch: number,
  rawHolder: string,
  at = nowIso(),
): HeartbeatResult {
  requireTime(at, "heartbeat time");
  const holder = normalizeHolder(rawHolder);
  if (
    epoch !== job.epoch || job.lease === null || job.lease.epoch !== epoch ||
    job.lease.holder !== holder
  ) {
    if (!job.events.some((e) => e.kind === "fenced" && e.epoch === epoch)) {
      event(job, "fenced", at, epoch, `${holder} found epoch ${job.epoch}`);
    }
    return "fenced";
  }
  job.lease.heartbeatAt = at;
  job.updatedAt = at;
  return job.state === "cancel_requested" ? "cancel" : "renewed";
}

/** Is `holder` of `epoch` the current lease holder (not fenced)? */
function holdsLease(job: JobJournal, epoch: number, holder: string): boolean {
  return epoch === job.epoch && job.lease !== null &&
    job.lease.epoch === epoch && job.lease.holder === holder;
}

/** Record the root process of the step command this attempt started. */
export function recordStepProcess(
  job: JobJournal,
  epoch: number,
  rawHolder: string,
  identity: ProcessIdentity,
): boolean {
  const holder = normalizeHolder(rawHolder);
  if (!holdsLease(job, epoch, holder)) return false;
  currentAttempt(job).process = { ...identity };
  return true;
}

export type CheckpointResult = "advanced" | "fenced" | "cancel";

/**
 * A step command completed successfully: record it and move the checkpoint,
 * so a takeover does not run it again. Fenced like a heartbeat, and renews
 * the lease.
 */
export function advanceCheckpoint(
  job: JobJournal,
  epoch: number,
  rawHolder: string,
  result: CommandResult,
  at = nowIso(),
): CheckpointResult {
  requireTime(at, "checkpoint time");
  const holder = normalizeHolder(rawHolder);
  if (!holdsLease(job, epoch, holder)) {
    heartbeat(job, epoch, holder, at); // records the fencing event once
    return "fenced";
  }
  if (result.index !== job.checkpoint || result.exitCode !== 0) {
    throw new Error(
      `job ${job.jobId}: checkpoint ${job.checkpoint} cannot take command ${result.index} (exit ${result.exitCode})`,
    );
  }
  job.commandResults.push({ ...result, epoch });
  job.checkpoint += 1;
  return heartbeat(job, epoch, holder, at) === "cancel" ? "cancel" : "advanced";
}

/**
 * Request a cancel. A job that has not started, or that awaits approval, is
 * cancelled at once; a running job goes to `cancel_requested` and its holder
 * stops at its next heartbeat (or a reconcile closes it once the holder is
 * gone). An intent is cancelled at once: its agent says it will not perform
 * the operation.
 */
export function cancelJob(
  job: JobJournal,
  reason: string,
  at = nowIso(),
): State {
  requireTime(at, "cancel time");
  if (isTerminal(job.state)) {
    throw new Error(`job ${job.jobId} is already ${job.state}`);
  }
  if (job.state === "cancel_requested") {
    // Already requested: the holder stops at its next heartbeat, or a
    // reconcile closes the job once the holder is gone. Not a force.
    return job.state;
  }
  const note = reason === ""
    ? "cancel requested"
    : `cancel requested: ${reason}`;
  event(job, "cancel-requested", at, job.epoch, note);
  const attempt = currentAttempt(job);
  if (
    job.state === "running" && job.kind !== "intent" &&
    attempt.result === "running"
  ) {
    // A holder runs the step: it stops at its next heartbeat.
    transition(job, "cancel_requested", at, note);
    return job.state;
  }
  if (job.state === "running") transition(job, "cancel_requested", at, note);
  if (attempt.result === "scheduled" || attempt.result === "running") {
    attempt.result = "cancelled";
    attempt.endedAt = at;
  }
  job.lease = null;
  transition(job, "cancelled", at, note);
  event(job, "cancelled", at, job.epoch, note);
  return job.state;
}

/** The holder stopped its step after a cancel request: close the job. */
export function finishCancel(
  job: JobJournal,
  epoch: number,
  rawHolder: string,
  at = nowIso(),
): boolean {
  requireTime(at, "cancel time");
  const holder = normalizeHolder(rawHolder);
  if (job.state !== "cancel_requested" || !holdsLease(job, epoch, holder)) {
    return false;
  }
  const attempt = currentAttempt(job);
  attempt.result = "cancelled";
  attempt.endedAt = at;
  job.lease = null;
  transition(job, "cancelled", at, `step stopped by ${holder}`);
  event(job, "cancelled", at, epoch, `step stopped by ${holder}`);
  return true;
}

/**
 * A controller stopped what a lost attempt left running (its step process
 * tree) before the next epoch runs the step again.
 */
export function noteLostStepStopped(
  job: JobJournal,
  epoch: number,
  root: ProcessIdentity,
  stopped: number,
  at = nowIso(),
): void {
  event(
    job,
    "lost-step-stopped",
    at,
    epoch,
    `stopped ${stopped} process(es) of the step tree rooted at pid ${root.pid}`,
  );
}

/**
 * Restart the start deadline of the current scheduled attempt (a controller
 * spent time stopping a lost tree before it spawns the executor).
 */
export function restartStartClock(
  job: JobJournal,
  epoch: number,
  at = nowIso(),
): boolean {
  requireTime(at, "schedule time");
  if (job.epoch !== epoch) return false;
  const attempt = currentAttempt(job);
  if (attempt.result !== "scheduled") return false;
  attempt.scheduledAt = at;
  job.updatedAt = at;
  return true;
}

/** The terminal result was copied into the task state. */
export function markRecordedToTask(job: JobJournal): void {
  if (!isTerminal(job.state)) {
    throw new Error(`job ${job.jobId} is ${job.state}, not terminal`);
  }
  job.recordedToTask = true;
}

export type ApplyResult =
  | { applied: true; state: State }
  | {
    applied: false;
    reason:
      | "stale-epoch"
      | "holder-lost"
      | "wrong-holder"
      | "already-applied"
      | "terminal";
  };

/**
 * Apply an executor's report (the ACK). Only the current epoch's running
 * attempt is applied; an older epoch is rejected and recorded, and the job's
 * result stays the newer attempt's.
 */
export function applyReport(
  job: JobJournal,
  report: JobReport,
  at = nowIso(),
): ApplyResult {
  requireTime(at, "apply time");
  requireTime(report.endedAt, "report endedAt");
  if (report.jobId !== job.jobId) {
    throw new Error(`report for ${report.jobId} applied to ${job.jobId}`);
  }
  const attempt = attemptAt(job, report.epoch);
  if (report.epoch !== job.epoch) {
    const note =
      `report by ${report.holder} (exit ${report.exitCode}) rejected; current epoch ${job.epoch}`;
    // An older epoch was closed as `lost` when the newer one was scheduled.
    // It stays `lost` (the job never used its outcome); the late report and
    // its exit code are kept in the attempt notes and in one event.
    if (attempt !== undefined && !attempt.notes.includes(note)) {
      attempt.notes = attempt.notes === ""
        ? `late ${note}`
        : `${attempt.notes}; late ${note}`;
    }
    // One event per distinct late report, so a looping old executor cannot
    // grow the journal without bound.
    if (
      !job.events.some((e) =>
        e.kind === "stale-epoch" && e.epoch === report.epoch && e.note === note
      )
    ) {
      event(job, "stale-epoch", at, report.epoch, note);
    }
    return { applied: false, reason: "stale-epoch" };
  }
  if (attempt === undefined) {
    throw new Error(
      `job ${job.jobId} has no attempt for epoch ${report.epoch}`,
    );
  }
  if (attempt.result === "lost") {
    // Reconcile already declared this holder lost (an intent now awaits
    // approval): a late report does not decide the outcome.
    event(
      job,
      "late-report",
      at,
      report.epoch,
      `report by ${report.holder} (exit ${report.exitCode}) after the holder was declared lost; not applied`,
    );
    return { applied: false, reason: "holder-lost" };
  }
  if (attempt.result !== "running") {
    return { applied: false, reason: "already-applied" };
  }
  if (isTerminal(job.state)) return { applied: false, reason: "terminal" };
  if (attempt.holder !== normalizeHolder(report.holder)) {
    const note = `report by ${report.holder} for an attempt held by ${
      attempt.holder ?? "nobody"
    }; not applied`;
    if (
      !job.events.some((e) =>
        e.kind === "wrong-holder" && e.epoch === report.epoch && e.note === note
      )
    ) {
      event(job, "wrong-holder", at, report.epoch, note);
    }
    return { applied: false, reason: "wrong-holder" };
  }
  attempt.result = report.exitCode === 0 ? "succeeded" : "failed";
  attempt.endedAt = report.endedAt;
  attempt.exitCode = report.exitCode;
  attempt.logRef = report.logRef;
  for (const result of report.results ?? []) {
    if (!job.commandResults.some((r) => r.index === result.index)) {
      job.commandResults.push({ ...result, epoch: report.epoch });
    }
  }
  if (report.exitCode === 0 && job.step !== null) {
    job.checkpoint = job.step.commands.length;
  }
  job.lease = null;
  transition(
    job,
    report.exitCode === 0 ? "succeeded" : "failed",
    at,
    `attempt ${attempt.attempt} exited ${report.exitCode}`,
  );
  return { applied: true, state: job.state };
}

/** Has the holder's lease expired at `at` (no lease counts as expired)? */
export function leaseExpired(job: JobJournal, at: string): boolean {
  if (job.lease === null) return true;
  // A corrupted heartbeat time counts as expired (take over, fail-safe).
  const age = requireTime(at, "reconcile time") -
    Date.parse(job.lease.heartbeatAt);
  return !(age <= job.lease.ttlMs);
}

/** Has a scheduled attempt waited longer than one TTL for its executor? */
function startOverdue(job: JobJournal, at: string): boolean {
  const attempt = currentAttempt(job);
  const age = requireTime(at, "reconcile time") -
    Date.parse(attempt.scheduledAt);
  return !(age <= job.leaseTtlMs);
}

export type ReconcileDecision =
  /** Terminal, or awaiting a human: nothing to do. */
  | { action: "none"; state: State }
  /** The holder's lease is fresh (or its executor may still be starting). */
  | { action: "alive"; epoch: number }
  /** A lost executor's outbox result was applied without rerunning. */
  | { action: "applied-outbox"; state: State }
  /** A new attempt at `epoch` was recorded; start its executor now. */
  | { action: "respawn"; epoch: number }
  /** An unknown non-idempotent operation now awaits approval. */
  | { action: "awaiting-approval"; epoch: number }
  /** The inputs changed since the digest; the job failed, not rerun. */
  | { action: "input-changed"; state: State }
  /** Too many attempts were lost; the job failed instead of rerunning. */
  | { action: "gave-up"; state: State }
  /** A cancel was pending and its holder is gone; the job is cancelled. */
  | { action: "cancelled"; state: State };

export interface ReconcileContext {
  at?: string;
  /**
   * Outbox report of the current epoch, if the executor wrote one. It must be
   * read under the same journal lock as the reconcile (`reconcileJobFile`
   * does this); an outbox read before the lock can miss a result written in
   * between, and the step would then be rerun although its result was durable.
   */
  outbox?: JobReport | null;
  /** Digest of the inputs as they are now. Required for step jobs. */
  currentDigest?: string;
  /**
   * The previous holder is known to be gone (e.g. `task resume` of an intent's
   * task): treat its lease as expired.
   */
  holderGone?: boolean;
}

/**
 * Decide what a lost or live job needs. Called under the journal lock by any
 * controller (`job status|wait|reconcile`, `task resume`). A respawn decision
 * has already recorded the new attempt and epoch, so the old executor is
 * fenced before the new one starts.
 */
export function reconcileJob(
  job: JobJournal,
  context: ReconcileContext = {},
): ReconcileDecision {
  const at = context.at ?? nowIso();
  requireTime(at, "reconcile time");
  if (isTerminal(job.state) || job.state === "awaiting-approval") {
    return { action: "none", state: job.state };
  }
  if (job.kind !== "intent" && context.currentDigest === undefined) {
    throw new Error(
      `reconciling step job ${job.jobId} needs the current execution digest`,
    );
  }
  const attempt = currentAttempt(job);

  // ACK loss: the executor wrote its result but did not apply it.
  if (
    context.outbox != null && context.outbox.epoch === job.epoch &&
    attempt.result === "running"
  ) {
    const applied = applyReport(job, context.outbox, at);
    if (applied.applied) {
      event(
        job,
        "outbox-applied",
        at,
        job.epoch,
        "result recovered from outbox",
      );
      return { action: "applied-outbox", state: job.state };
    }
  }

  // Only a lease of the current attempt is evidence that its holder lives.
  const leaseIsCurrent = job.lease !== null &&
    job.lease.epoch === job.epoch && job.lease.holder === attempt.holder;
  const expired = context.holderGone === true ||
    (attempt.result === "scheduled"
      ? startOverdue(job, at)
      : !leaseIsCurrent || leaseExpired(job, at));
  if (!expired) return { action: "alive", epoch: job.epoch };

  if (job.state === "cancel_requested") {
    // The holder is gone while a cancel was pending: nothing will run again.
    attempt.lastSeenAt ??= job.lease?.heartbeatAt ?? attempt.startedAt ??
      attempt.scheduledAt;
    attempt.result = "cancelled";
    attempt.endedAt = at;
    job.lease = null;
    transition(job, "cancelled", at, "cancel requested; holder gone");
    event(job, "cancelled", at, job.epoch, "holder gone after cancel request");
    return { action: "cancelled", state: job.state };
  }

  // A held respawn already declared this holder lost (holdRespawn).
  if (attempt.result !== "lost") declareHolderLost(job, attempt, at, "");

  if (job.kind === "intent") {
    transition(
      job,
      "awaiting-approval",
      at,
      `unknown outcome of non-idempotent ${
        job.intent?.op ?? "external"
      } operation`,
    );
    return { action: "awaiting-approval", epoch: job.epoch };
  }

  if (context.currentDigest !== job.executionDigest) {
    event(job, "input-changed", at, job.epoch, "digest differs; not rerun");
    closeAsFailed(job, at, "inputs changed since the job was recorded");
    return { action: "input-changed", state: job.state };
  }

  if (job.attempts.length >= job.maxAttempts) {
    event(
      job,
      "gave-up",
      at,
      job.epoch,
      `${job.attempts.length} attempt(s) lost; not rerun again`,
    );
    closeAsFailed(job, at, `${job.attempts.length} attempt(s) lost`);
    return { action: "gave-up", state: job.state };
  }
  scheduleNextAttempt(job, at, `attempt ${attempt.attempt} lost`);
  return { action: "respawn", epoch: job.epoch };
}

/**
 * Record that the current attempt's holder is lost: from now on its
 * heartbeats and checkpoints are fenced (no lease) and its late report is not
 * applied (the attempt is `lost`).
 */
function declareHolderLost(
  job: JobJournal,
  attempt: JobAttempt,
  at: string,
  extra: string,
): void {
  event(
    job,
    "lease-expired",
    at,
    job.epoch,
    (attempt.result === "scheduled"
      ? `executor for attempt ${attempt.attempt} never started`
      : `holder ${attempt.holder ?? "?"} lost`) + extra,
  );
  // When the holder was last known alive (its last heartbeat, or the start):
  // the loss can be noticed much later than it happened.
  attempt.lastSeenAt = job.lease?.heartbeatAt ?? attempt.startedAt ??
    attempt.scheduledAt;
  attempt.result = "lost";
  attempt.endedAt = at;
  attempt.notes = `holder lost; outcome unknown${extra}`;
  job.lease = null;
  job.updatedAt = at;
}

/**
 * A reconcile decided to respawn, but the respawn is held (the W-04 dev
 * guard refused a source build): declare the current holder lost now, so it
 * is fenced before a controller stops its step tree, without scheduling the
 * next attempt (no epoch bump, no attempt used up). A later reconcile
 * schedules it once the respawn is allowed. Returns false if the holder was
 * already declared lost.
 */
export function holdRespawn(
  job: JobJournal,
  reason: string,
  at = nowIso(),
): boolean {
  requireTime(at, "hold time");
  const attempt = currentAttempt(job);
  if (attempt.result === "lost") return false;
  if (attempt.result !== "running" && attempt.result !== "scheduled") {
    throw new Error(
      `job ${job.jobId}: cannot hold attempt ${attempt.attempt} (${attempt.result})`,
    );
  }
  declareHolderLost(job, attempt, at, `; respawn held: ${reason}`);
  return true;
}

/**
 * Fail a job from `running` or from `scheduled` (an executor that never
 * started). The frozen table has no `scheduled -> failed` edge, so a job
 * whose executor never began is closed through `running`, the same way an
 * approver's confirmation goes `awaiting-approval -> running -> succeeded`.
 */
function closeAsFailed(job: JobJournal, at: string, reason: string): void {
  if (job.state === "scheduled") {
    transition(job, "running", at, "no attempt started; closing the job");
  }
  transition(job, "failed", at, reason);
}

function scheduleNextAttempt(job: JobJournal, at: string, notes: string): void {
  job.epoch += 1;
  job.attempts.push({
    attempt: job.attempts.length + 1,
    epoch: job.epoch,
    holder: null,
    scheduledAt: at,
    startedAt: null,
    endedAt: null,
    result: "scheduled",
    exitCode: null,
    logRef: null,
    process: null,
    notes,
  });
  job.updatedAt = at;
}

export type IntentOutcome = "succeeded" | "failed";

/** The agent confirms the outcome of its intent (fenced like a report). */
export function confirmIntent(
  job: JobJournal,
  epoch: number,
  holder: string,
  outcome: IntentOutcome,
  at = nowIso(),
): ApplyResult {
  if (job.kind !== "intent") {
    throw new Error(`job ${job.jobId} is not an intent`);
  }
  return applyReport(job, {
    jobId: job.jobId,
    epoch,
    holder,
    exitCode: outcome === "succeeded" ? 0 : 1,
    endedAt: at,
    logRef: null,
    results: [],
  }, at);
}

export type Resolution = "succeeded" | "failed" | "retry";

/**
 * A human resolves a job that awaits approval after checking the external
 * state: it did happen (`succeeded`), it did not and must not be retried
 * (`failed`), or it may be performed again (`retry`, a new attempt at
 * epoch + 1 held by `holder`).
 */
export function resolveJob(
  job: JobJournal,
  resolution: Resolution,
  note: string,
  holder?: string,
  at = nowIso(),
): JobJournal {
  if (job.state !== "awaiting-approval") {
    throw new Error(
      `job ${job.jobId} is ${job.state}; only a job awaiting approval can be resolved`,
    );
  }
  if (
    resolution === "retry" && job.kind === "intent" &&
    (holder ?? "").trim() === ""
  ) {
    throw new Error("retrying an intent needs the agent that performs it");
  }
  requireTime(at, "resolve time");
  const reason = `resolved ${resolution} by approver${note ? `: ${note}` : ""}`;
  event(job, "resolved", at, job.epoch, reason);
  if (resolution === "failed") {
    transition(job, "failed", at, reason);
    return job;
  }
  // The table has no awaiting-approval -> succeeded edge: go through running.
  transition(job, "running", at, reason);
  if (resolution === "succeeded") {
    transition(job, "succeeded", at, "outcome confirmed by approver");
    return job;
  }
  scheduleNextAttempt(job, at, "retry approved");
  if (job.kind === "intent") {
    startAttempt(job, job.epoch, holder!.trim(), at);
  }
  return job;
}

// ---------------------------------------------------------------------------
// journal and outbox I/O
// ---------------------------------------------------------------------------

/** `<worktree>/_dist/fdh/jobs/<task-id>`. */
export function jobsDir(worktree: string, taskId: string): string {
  validateTaskId(taskId);
  return path.join(worktree, FDH_JOBS_DIR, taskId);
}

/** Path of one job journal. */
export function jobJournalPath(
  worktree: string,
  taskId: string,
  jobId: string,
): string {
  return path.join(jobsDir(worktree, taskId), `${validateJobId(jobId)}.json`);
}

/** Path of the outbox an executor of `epoch` writes before its ACK. */
export function outboxPath(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
): string {
  return path.join(
    jobsDir(worktree, taskId),
    `${validateJobId(jobId)}.outbox-${epoch}.json`,
  );
}

/**
 * Rename, retrying briefly on Windows sharing violations: the rename fails
 * with EACCES/EPERM while another process (a `job status` reader, an
 * antivirus scan) has the target open without FILE_SHARE_DELETE.
 */
function renameWithRetry(from: string, to: string): void {
  for (let retry = 0;; retry++) {
    try {
      Deno.renameSync(from, to);
      return;
    } catch (error) {
      const retryable = error instanceof Deno.errors.PermissionDenied ||
        error instanceof Deno.errors.Busy ||
        (error instanceof Error && /os error 32\b/.test(error.message));
      if (!retryable || retry >= 5) {
        try {
          Deno.removeSync(from);
        } catch {
          // best-effort: do not leave the tmp file behind
        }
        throw error;
      }
      Atomics.wait(
        new Int32Array(new SharedArrayBuffer(4)),
        0,
        0,
        10 * 2 ** retry,
      );
    }
  }
}

function writeJsonAtomic(file: string, value: unknown): void {
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.tmp-${Deno.pid}`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(value, null, 2)}\n`);
  renameWithRetry(tmp, file);
}

const ATTEMPT_RESULTS: readonly AttemptResult[] = [
  "scheduled",
  "running",
  "succeeded",
  "failed",
  "lost",
  "cancelled",
];

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

/** A command runs a non-empty argv of strings (no holes, no empty program). */
function isValidCommand(command: unknown): command is StepCommand {
  return isRecord(command) && Array.isArray(command.argv) &&
    command.argv.length > 0 &&
    Array.from(command.argv as unknown[]).every((arg) =>
      typeof arg === "string"
    ) &&
    command.argv[0] !== "" && typeof command.displayCommand === "string" &&
    (command.assertionName === undefined ||
      typeof command.assertionName === "string") &&
    (command.expected === undefined || typeof command.expected === "string");
}

/** A command result with integer fields, ISO times and a string-or-null log. */
function isCommandResult(value: unknown): value is CommandResult {
  return isRecord(value) && Number.isInteger(value.index) &&
    (value.index as number) >= 0 && Number.isInteger(value.epoch) &&
    Number.isInteger(value.exitCode) && typeof value.startedAt === "string" &&
    Number.isFinite(Date.parse(value.startedAt)) &&
    typeof value.endedAt === "string" &&
    Number.isFinite(Date.parse(value.endedAt)) &&
    (value.logRef === null || typeof value.logRef === "string");
}

/** A step runs one or more valid commands in a string cwd. */
function isValidStep(step: unknown): step is StepSpec {
  return isRecord(step) && typeof step.cwd === "string" &&
    typeof step.displayCommand === "string" && Array.isArray(step.commands) &&
    step.commands.length > 0 &&
    Array.from(step.commands as unknown[]).every(isValidCommand);
}

export function parseJobJournal(value: unknown): JobJournal {
  const job = value as Partial<JobJournal> | null;
  if (job === null || typeof job !== "object" || job.schemaVersion !== 1) {
    throw new Error("invalid fdh job journal: schemaVersion must be 1");
  }
  if (typeof job.jobId !== "string") {
    throw new Error("invalid fdh job journal: jobId");
  }
  validateJobId(job.jobId);
  if (typeof job.taskId !== "string") {
    throw new Error("invalid fdh job journal: taskId");
  }
  validateTaskId(job.taskId);
  if (!JOB_KINDS.includes(job.kind as JobKind)) {
    throw new Error("invalid fdh job journal: kind");
  }
  if (!TABLE.states.includes(job.state as State)) {
    throw new Error("invalid fdh job journal: state");
  }
  if (!Number.isInteger(job.epoch) || (job.epoch as number) < 1) {
    throw new Error("invalid fdh job journal: epoch");
  }
  if (
    !Array.isArray(job.attempts) || !Array.isArray(job.transitions) ||
    !Array.isArray(job.events)
  ) {
    throw new Error("invalid fdh job journal: attempts/transitions/events");
  }
  if (
    !Number.isInteger(job.maxAttempts) || (job.maxAttempts as number) < 1 ||
    !Number.isInteger(job.leaseTtlMs) ||
    (job.leaseTtlMs as number) < MIN_LEASE_TTL_MS
  ) {
    throw new Error("invalid fdh job journal: maxAttempts/leaseTtlMs");
  }
  if (typeof job.executionDigest !== "string") {
    throw new Error("invalid fdh job journal: executionDigest");
  }
  for (const attempt of job.attempts as unknown[]) {
    if (
      !isRecord(attempt) || !Number.isInteger(attempt.attempt) ||
      !Number.isInteger(attempt.epoch) ||
      !ATTEMPT_RESULTS.includes(attempt.result as AttemptResult) ||
      typeof attempt.notes !== "string"
    ) {
      throw new Error("invalid fdh job journal: attempts entry");
    }
  }
  const commandCount = job.kind === "intent" || !isRecord(job.step) ||
      !Array.isArray(job.step.commands)
    ? 0
    : job.step.commands.length;
  if (
    !Number.isInteger(job.checkpoint) || (job.checkpoint as number) < 0 ||
    (job.checkpoint as number) > commandCount ||
    !Array.isArray(job.commandResults) ||
    typeof job.recordedToTask !== "boolean"
  ) {
    throw new Error(
      "invalid fdh job journal: checkpoint/commandResults/recordedToTask",
    );
  }
  for (const result of job.commandResults as unknown[]) {
    if (!isCommandResult(result)) {
      throw new Error("invalid fdh job journal: commandResults entry");
    }
  }
  for (let i = 1; i < job.attempts.length; i++) {
    if (job.attempts[i].epoch <= job.attempts[i - 1].epoch) {
      throw new Error(
        "invalid fdh job journal: attempt epochs must strictly increase",
      );
    }
  }
  const last = job.attempts[job.attempts.length - 1];
  if (last === undefined || last.epoch !== job.epoch) {
    throw new Error(
      "invalid fdh job journal: the last attempt must belong to the current epoch",
    );
  }
  if (job.kind === "intent") {
    if (
      !isRecord(job.intent) ||
      !INTENT_OPS.includes(job.intent.op as IntentOp) || job.step != null
    ) {
      throw new Error("invalid fdh job journal: intent");
    }
  } else if (!isValidStep(job.step) || job.intent != null) {
    throw new Error("invalid fdh job journal: step");
  }
  const lease = job.lease as unknown;
  if (
    lease !== null &&
    (!isRecord(lease) || typeof lease.holder !== "string" ||
      !Number.isInteger(lease.epoch) || typeof lease.heartbeatAt !== "string" ||
      !Number.isInteger(lease.ttlMs))
  ) {
    throw new Error("invalid fdh job journal: lease");
  }
  return job as JobJournal;
}

/** Read and validate one journal (no lock; use `updateJob` to change it). */
export function readJob(
  worktree: string,
  taskId: string,
  jobId: string,
): JobJournal {
  const file = jobJournalPath(worktree, taskId, jobId);
  let text: string;
  try {
    text = Deno.readTextFileSync(file);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      throw new Error(`no fdh job ${jobId} for task ${taskId}`);
    }
    throw error;
  }
  try {
    return parseJobJournal(JSON.parse(text));
  } catch (error) {
    throw new Error(
      `invalid fdh job journal at ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
}

/** Write a new journal; refuses to overwrite an existing job. */
export async function writeNewJob(job: JobJournal): Promise<string> {
  const file = jobJournalPath(job.worktree, job.taskId, job.jobId);
  await withFileLock(lockPathFor(file), () => {
    try {
      Deno.statSync(file);
      throw new Error(`fdh job ${job.jobId} already exists`);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
    }
    writeJsonAtomic(file, job);
  });
  return file;
}

/**
 * Read, change and write one journal under its lock. `fn` may return a value
 * or a promise; the journal is written only after `fn` settled without
 * throwing, still under the lock.
 */
export async function updateJob<T>(
  worktree: string,
  taskId: string,
  jobId: string,
  fn: (job: JobJournal) => T | Promise<T>,
): Promise<{ job: JobJournal; value: T }> {
  const file = jobJournalPath(worktree, taskId, jobId);
  return await withFileLock(lockPathFor(file), async () => {
    const job = readJob(worktree, taskId, jobId);
    const value = await fn(job);
    writeJsonAtomic(file, job);
    return { job, value };
  });
}

/** Job ids of a task, sorted; outbox, lock and tmp files are not jobs. */
export function listJobIds(worktree: string, taskId: string): string[] {
  const dir = jobsDir(worktree, taskId);
  const ids: string[] = [];
  try {
    for (const entry of Deno.readDirSync(dir)) {
      const match = /^([a-z0-9][a-z0-9-]*)\.json$/.exec(entry.name);
      if (entry.isFile && match) ids.push(match[1]);
    }
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return [];
    throw error;
  }
  return ids.sort();
}

/** Write an executor's report to its outbox (before the ACK). */
export function writeOutbox(
  worktree: string,
  taskId: string,
  report: JobReport,
): string {
  const file = outboxPath(worktree, taskId, report.jobId, report.epoch);
  writeJsonAtomic(file, report);
  return file;
}

/** Read the outbox of `epoch`; null when there is none, an error when invalid. */
export function readOutbox(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
): JobReport | null {
  const file = outboxPath(worktree, taskId, jobId, epoch);
  let text: string;
  try {
    text = Deno.readTextFileSync(file);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    throw error;
  }
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch (error) {
    throw new Error(
      `invalid fdh job outbox at ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
  if (
    !isRecord(value) || value.jobId !== jobId || value.epoch !== epoch ||
    !Number.isInteger(value.exitCode) || typeof value.holder !== "string" ||
    value.holder.trim() === "" ||
    typeof value.endedAt !== "string" ||
    !Number.isFinite(Date.parse(value.endedAt)) ||
    !Array.isArray(value.results) ||
    !(value.results as unknown[]).every(isCommandResult) ||
    (value.logRef !== null && typeof value.logRef !== "string")
  ) {
    throw new Error(
      `invalid fdh job outbox at ${file}: expected the report of ${jobId} epoch ${epoch}`,
    );
  }
  return value as unknown as JobReport;
}

/**
 * Reconcile one journal under its lock, reading the current epoch's outbox
 * inside the same lock (see `ReconcileContext.outbox`).
 */
export async function reconcileJobFile(
  worktree: string,
  taskId: string,
  jobId: string,
  context: Omit<ReconcileContext, "outbox"> = {},
): Promise<{ job: JobJournal; value: ReconcileDecision }> {
  return await updateJob(worktree, taskId, jobId, (job) =>
    reconcileJob(job, {
      ...context,
      // A settled job never uses its outbox; do not let a damaged one stop a
      // sweep over the task's jobs.
      outbox: isTerminal(job.state) || job.state === "awaiting-approval"
        ? null
        : readOutbox(worktree, taskId, jobId, job.epoch),
    }));
}

// ---------------------------------------------------------------------------
// executor loop
// ---------------------------------------------------------------------------

export interface RunningStep {
  /** Resolves with the command's exit code. */
  status: Promise<number>;
  /** Stop the command (and, for real processes, its process tree). */
  kill(): void | Promise<void>;
  /** Pid of the command's root process, when it is a real process. */
  pid?: number;
  /** Identity of the root process, when the runner already looks it up. */
  identity?: Promise<ProcessIdentity | null>;
}

/** Fault points for the B-02 live tests (never set in normal runs). */
export type ExecutorFault =
  /** Exit right after the outbox is written, before the ACK (E-3). */
  | { kind: "exit-after-outbox" }
  /** Stop touching the journal for `ms` after the step starts (E-4/E-5). */
  | { kind: "stall"; ms: number };

/** Parse `FDH_JOB_FAULT` (`exit-after-outbox` or `stall:<ms>`); null if unset. */
export function parseExecutorFault(
  raw: string | undefined,
): ExecutorFault | null {
  if (raw === undefined || raw === "") return null;
  if (raw === "exit-after-outbox") return { kind: "exit-after-outbox" };
  const stall = /^stall:(\d+)$/.exec(raw);
  if (stall) return { kind: "stall", ms: Number(stall[1]) };
  throw new Error(
    `FDH_JOB_FAULT must be exit-after-outbox or stall:<ms>, got ${
      JSON.stringify(raw)
    }`,
  );
}

export interface ExecutorOptions {
  worktree: string;
  taskId: string;
  jobId: string;
  epoch: number;
  holder: string;
  runStep: (command: StepCommand, cwd: string, logPath: string) => RunningStep;
  /** Identify a started command's root process (recorded on the attempt). */
  identify?: (pid: number) => Promise<ProcessIdentity | null>;
  heartbeatMs?: number;
  now?: () => string;
  sleep?: (ms: number) => Promise<void>;
  /** Called instead of returning when the fault says to exit (tests). */
  exit?: (code: number) => never;
  fault?: ExecutorFault | null;
}

export type ExecutorOutcome =
  | { outcome: "applied"; state: State }
  | { outcome: "refused"; reason: string }
  | { outcome: "fenced" }
  | { outcome: "cancelled" }
  | { outcome: "stale-rejected" };

/**
 * Run one attempt as its executor: take the attempt, run the step's commands
 * from the job's checkpoint with heartbeats (checkpointing each success but
 * the last), then write the outbox and apply it under one journal lock.
 * Stops the running command when a newer epoch fences this executor or a
 * cancel is requested.
 */
export async function runExecutor(
  options: ExecutorOptions,
): Promise<ExecutorOutcome> {
  const now = options.now ?? nowIso;
  // An injected sleep (tests) cannot be cancelled; the default one is, so a
  // finished executor does not linger on a pending heartbeat timer.
  const sleepFor = (ms: number): { promise: Promise<void>; cancel(): void } => {
    if (options.sleep !== undefined) {
      return { promise: options.sleep(ms), cancel: () => {} };
    }
    let id: ReturnType<typeof setTimeout> | undefined;
    const promise = new Promise<void>((resolve) => {
      id = setTimeout(resolve, ms);
    });
    return { promise, cancel: () => clearTimeout(id) };
  };
  const { worktree, taskId, jobId, epoch } = options;
  const holder = normalizeHolder(options.holder);

  const begun = await updateJob(
    worktree,
    taskId,
    jobId,
    (job) => {
      if (job.step === null) {
        return { ok: false as const, reason: "intent jobs have no executor" };
      }
      const result = beginAttempt(job, epoch, holder, now());
      return result.ok
        ? {
          ok: true as const,
          step: job.step,
          ttlMs: job.leaseTtlMs,
          checkpoint: job.checkpoint,
        }
        : { ok: false as const, reason: result.reason as string };
    },
  );
  if (!begun.value.ok) {
    return { outcome: "refused", reason: begun.value.reason };
  }
  const { step, ttlMs, checkpoint } = begun.value;
  // Beat well inside the job's TTL, whatever the caller asked for.
  const heartbeatMs = Math.max(
    1,
    Math.min(
      options.heartbeatMs ?? DEFAULT_HEARTBEAT_MS,
      Math.floor(ttlMs / 3),
    ),
  );
  const stallUntil = options.fault?.kind === "stall"
    ? Date.parse(now()) + options.fault.ms
    : 0;
  let lastGoodBeat = Date.parse(now());

  const stopAndWait = async (
    running: RunningStep,
    status: Promise<number>,
  ): Promise<void> => {
    await running.kill();
    const wait = sleepFor(5_000);
    await Promise.race([status, wait.promise]);
    wait.cancel();
  };

  const results: CommandResult[] = [];
  let exitCode = 0;
  let lastLogRef: string | null = null;
  for (let index = checkpoint; index < step.commands.length; index++) {
    const command = step.commands[index];
    // Refs use "/" on every OS so evidence from different machines compares.
    const logRef = posixJoin(
      ...FDH_JOBS_DIR.split(/[\\/]/),
      taskId,
      `${jobId}-${epoch}-${index}.log`,
    );
    lastLogRef = logRef;
    const logPath = path.join(worktree, ...logRef.split("/"));
    const startedAt = now();
    let running: RunningStep;
    try {
      running = options.runStep(command, step.cwd, logPath);
    } catch (error) {
      // The command could not be started: a failed attempt (exit 127).
      try {
        Deno.writeTextFileSync(
          logPath,
          `fdh executor: could not start ${command.displayCommand}: ${
            error instanceof Error ? error.message : String(error)
          }\n`,
        );
      } catch {
        // The log is best-effort; the report below still records the failure.
      }
      running = { status: Promise.resolve(127), kill: () => {} };
    }
    const status = running.status.catch(() => 127);
    const done: { code: number | null } = { code: null };
    status.then((code) => {
      done.code = code;
    });

    try {
      // The identity lookup (PowerShell on Windows, can take seconds) runs
      // alongside the heartbeats, so a short TTL does not expire meanwhile;
      // it is recorded at the first tick after it resolves. Identity is
      // best-effort: without it, a controller cannot stop a lost step tree.
      const found: { identity: ProcessIdentity | null; recorded: boolean } = {
        identity: null,
        recorded: false,
      };
      const lookup = running.identity ??
        (running.pid !== undefined && options.identify !== undefined
          ? options.identify(running.pid)
          : undefined);
      lookup?.then((identity) => {
        found.identity = identity;
      }).catch(() => {});
      while (done.code === null) {
        const tick = sleepFor(heartbeatMs);
        await Promise.race([status, tick.promise]);
        tick.cancel();
        if (done.code !== null) break;
        if (found.identity !== null && !found.recorded) {
          // Recorded even while stalled: it belongs to starting the step.
          const identity = found.identity;
          await updateJob(
            worktree,
            taskId,
            jobId,
            (job) => recordStepProcess(job, epoch, holder, identity),
          );
          found.recorded = true;
        }
        if (Date.parse(now()) < stallUntil) continue;
        let beat: HeartbeatResult;
        try {
          beat = (await updateJob(
            worktree,
            taskId,
            jobId,
            (job) => heartbeat(job, epoch, holder, now()),
          )).value;
          lastGoodBeat = Date.parse(now());
        } catch (error) {
          // A short failure (a Windows sharing violation that outlasts the
          // rename retries) is tolerated while the lease cannot have expired.
          if (Date.parse(now()) - lastGoodBeat + heartbeatMs < ttlMs) continue;
          throw error;
        }
        if (beat === "fenced") {
          await stopAndWait(running, status);
          return { outcome: "fenced" };
        }
        if (beat === "cancel") {
          await stopAndWait(running, status);
          await updateJob(
            worktree,
            taskId,
            jobId,
            (job) => finishCancel(job, epoch, holder, now()),
          );
          return { outcome: "cancelled" };
        }
      }
    } catch (error) {
      // A heartbeat that cannot be written must not leave the step running
      // unowned: the lease will expire and a new epoch would run it again.
      if (done.code === null) await running.kill();
      throw error;
    }

    const result: CommandResult = {
      index,
      epoch,
      exitCode: done.code,
      startedAt,
      endedAt: now(),
      logRef,
    };
    if (done.code !== 0 || index === step.commands.length - 1) {
      // The last command's result (or the failure) goes into the report, so
      // it lands atomically with the job's outcome.
      exitCode = done.code;
      results.push(result);
      break;
    }
    if (Date.parse(now()) < stallUntil) {
      // A stalled executor does not touch the journal; keep the result for
      // the report instead of a checkpoint.
      results.push(result);
      continue;
    }
    const advanced = (await updateJob(
      worktree,
      taskId,
      jobId,
      (job) => advanceCheckpoint(job, epoch, holder, result, now()),
    )).value;
    if (advanced === "fenced") return { outcome: "fenced" };
    if (advanced === "cancel") {
      await updateJob(
        worktree,
        taskId,
        jobId,
        (job) => finishCancel(job, epoch, holder, now()),
      );
      return { outcome: "cancelled" };
    }
  }
  while (Date.parse(now()) < stallUntil) {
    const wait = sleepFor(
      Math.min(heartbeatMs, stallUntil - Date.parse(now())),
    );
    await wait.promise;
  }

  const report: JobReport = {
    jobId,
    epoch,
    holder,
    exitCode,
    endedAt: now(),
    logRef: lastLogRef,
    results,
  };
  // The outbox is written under the journal lock, so a reconcile cannot read
  // "no outbox" and then declare this attempt lost while the result lands in
  // between. A crash after the outbox and before the ACK (the same lock hold)
  // leaves the outbox for the next reconcile (E-3).
  let applied: { job: JobJournal; value: ApplyResult };
  try {
    applied = await updateJob(worktree, taskId, jobId, (job) => {
      writeOutbox(worktree, taskId, report);
      if (options.fault?.kind === "exit-after-outbox") {
        (options.exit ?? Deno.exit)(70);
      }
      return applyReport(job, report, now());
    });
  } catch (error) {
    // The journal could not be read or written: keep the result durable so
    // a reconcile can still recover it (best-effort).
    try {
      writeOutbox(worktree, taskId, report);
    } catch {
      // nothing more to do; the error below is the report
    }
    throw error;
  }
  if (applied.value.applied) {
    return { outcome: "applied", state: applied.value.state };
  }
  if (applied.value.reason === "already-applied") {
    // A controller recovered this outbox first (reconcile); same result.
    return { outcome: "applied", state: applied.job.state };
  }
  return applied.value.reason === "stale-epoch"
    ? { outcome: "stale-rejected" }
    : { outcome: "refused", reason: applied.value.reason };
}
