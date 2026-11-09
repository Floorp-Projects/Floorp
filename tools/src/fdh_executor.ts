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
 * B-02 test case (E-1..E-7) is exercised without real processes. It does not
 * claim exactly-once execution: a safe step may run again after its executor
 * is lost, and an old executor's local writes cannot be undone.
 */

import * as path from "@std/path";
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
  | "stale-rejected"
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
  /** Worktree-relative log of the step command, when there is one. */
  logRef: string | null;
  notes: string;
}

export interface JobLease {
  holder: string;
  epoch: number;
  heartbeatAt: string;
  ttlMs: number;
}

/** What a safe step runs; recorded in full so a takeover runs the same thing. */
export interface StepSpec {
  argv: string[];
  cwd: string;
  displayCommand: string;
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
  | "gave-up";

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
}

// ---------------------------------------------------------------------------
// state machine (frozen table)
// ---------------------------------------------------------------------------

const TABLE = loadTransitions();
const TERMINAL = new Set<State>(TABLE.terminalStates);

export function isTerminal(state: State): boolean {
  return TERMINAL.has(state);
}

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
  if (isIntent !== (input.intent !== undefined)) {
    throw new Error(
      "an intent job needs an intent spec, a step job needs none",
    );
  }
  if (!isIntent && input.step === undefined) {
    throw new Error(`a ${input.kind} job needs a step spec`);
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
  const job: JobJournal = {
    schemaVersion: 1,
    jobId: input.jobId,
    taskId: input.taskId,
    worktree: input.worktree,
    kind: input.kind,
    step: input.step ?? null,
    intent: input.intent ?? null,
    inputs: input.inputs,
    executionDigest: input.executionDigest,
    state: "pending",
    epoch: 1,
    lease: null,
    leaseTtlMs: ttl,
    maxAttempts,
    attempts: [],
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
    notes: "",
  });
  if (isIntent) {
    startAttempt(job, 1, input.holder!.trim(), at);
  }
  return job;
}

function currentAttempt(job: JobJournal): JobAttempt {
  const attempt = job.attempts[job.attempts.length - 1];
  if (attempt === undefined || attempt.epoch !== job.epoch) {
    throw new Error(`job ${job.jobId} has no attempt for epoch ${job.epoch}`);
  }
  return attempt;
}

function attemptAt(job: JobJournal, epoch: number): JobAttempt | undefined {
  return job.attempts.find((attempt) => attempt.epoch === epoch);
}

export type BeginResult =
  | { ok: true }
  | { ok: false; reason: "stale-epoch" | "not-scheduled" | "terminal" };

/**
 * The executor of `epoch` takes its attempt: only when that epoch is the
 * journal's epoch and the attempt has not started. Otherwise nothing changes
 * except a recorded `stale-epoch` event, and the executor must exit.
 */
export function beginAttempt(
  job: JobJournal,
  epoch: number,
  holder: string,
  at = nowIso(),
): BeginResult {
  if (isTerminal(job.state)) return { ok: false, reason: "terminal" };
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

export type HeartbeatResult = "renewed" | "fenced";

/** Renew the lease. `fenced` means a newer epoch took over: stop the step. */
export function heartbeat(
  job: JobJournal,
  epoch: number,
  holder: string,
  at = nowIso(),
): HeartbeatResult {
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
  return "renewed";
}

export type ApplyResult =
  | { applied: true; state: State }
  | {
    applied: false;
    reason: "stale-epoch" | "holder-lost" | "already-applied" | "terminal";
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
  if (report.jobId !== job.jobId) {
    throw new Error(`report for ${report.jobId} applied to ${job.jobId}`);
  }
  const attempt = attemptAt(job, report.epoch);
  if (report.epoch !== job.epoch) {
    if (attempt !== undefined && attempt.result !== "stale-rejected") {
      attempt.result = "stale-rejected";
      attempt.endedAt = report.endedAt;
      attempt.exitCode = report.exitCode;
      attempt.notes =
        `report from epoch ${report.epoch} arrived after epoch ${job.epoch} took over; rejected`;
    }
    event(
      job,
      "stale-epoch",
      at,
      report.epoch,
      `report by ${report.holder} (exit ${report.exitCode}) rejected; current epoch ${job.epoch}`,
    );
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
  attempt.result = report.exitCode === 0 ? "succeeded" : "failed";
  attempt.endedAt = report.endedAt;
  attempt.exitCode = report.exitCode;
  attempt.logRef = report.logRef;
  job.lease = null;
  transition(
    job,
    report.exitCode === 0 ? "succeeded" : "failed",
    at,
    `attempt ${attempt.attempt} exited ${report.exitCode}`,
  );
  return { applied: true, state: job.state };
}

export function leaseExpired(job: JobJournal, at: string): boolean {
  if (job.lease === null) return true;
  const age = Date.parse(at) - Date.parse(job.lease.heartbeatAt);
  return !(age <= job.lease.ttlMs);
}

/** Has a scheduled attempt waited longer than one TTL for its executor? */
function startOverdue(job: JobJournal, at: string): boolean {
  const attempt = currentAttempt(job);
  const age = Date.parse(at) - Date.parse(attempt.scheduledAt);
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
  | { action: "gave-up"; state: State };

export interface ReconcileContext {
  at?: string;
  /** Outbox report of the current epoch, if the executor wrote one. */
  outbox?: JobReport | null;
  /** Digest of the inputs as they are now (safe steps only). */
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
  if (isTerminal(job.state) || job.state === "awaiting-approval") {
    return { action: "none", state: job.state };
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

  const expired = context.holderGone === true ||
    (attempt.result === "scheduled"
      ? startOverdue(job, at)
      : leaseExpired(job, at));
  if (!expired) return { action: "alive", epoch: job.epoch };

  event(
    job,
    "lease-expired",
    at,
    job.epoch,
    attempt.result === "scheduled"
      ? `executor for attempt ${attempt.attempt} never started`
      : `holder ${attempt.holder ?? "?"} lost`,
  );
  attempt.result = "lost";
  attempt.endedAt = at;
  attempt.notes = "holder lost; outcome unknown";
  job.lease = null;

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

  if (
    context.currentDigest !== undefined &&
    context.currentDigest !== job.executionDigest
  ) {
    event(job, "input-changed", at, job.epoch, "digest differs; not rerun");
    transition(job, "failed", at, "inputs changed since the job was recorded");
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
    transition(job, "failed", at, `${job.attempts.length} attempt(s) lost`);
    return { action: "gave-up", state: job.state };
  }
  scheduleNextAttempt(job, at, `attempt ${attempt.attempt} lost`);
  return { action: "respawn", epoch: job.epoch };
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
    if ((holder ?? "").trim() === "") {
      throw new Error("retrying an intent needs the agent that performs it");
    }
    startAttempt(job, job.epoch, holder!.trim(), at);
  }
  return job;
}

// ---------------------------------------------------------------------------
// journal and outbox I/O
// ---------------------------------------------------------------------------

export function jobsDir(worktree: string, taskId: string): string {
  validateTaskId(taskId);
  return path.join(worktree, FDH_JOBS_DIR, taskId);
}

export function jobJournalPath(
  worktree: string,
  taskId: string,
  jobId: string,
): string {
  return path.join(jobsDir(worktree, taskId), `${validateJobId(jobId)}.json`);
}

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

function writeJsonAtomic(file: string, value: unknown): void {
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.tmp-${Deno.pid}`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(value, null, 2)}\n`);
  Deno.renameSync(tmp, file);
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
  return job as JobJournal;
}

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
  return parseJobJournal(JSON.parse(text));
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
 * Read, change and write one journal under its lock. `fn` may return a value;
 * the journal is written only when `fn` did not throw.
 */
export async function updateJob<T>(
  worktree: string,
  taskId: string,
  jobId: string,
  fn: (job: JobJournal) => T,
): Promise<{ job: JobJournal; value: T }> {
  const file = jobJournalPath(worktree, taskId, jobId);
  return await withFileLock(lockPathFor(file), () => {
    const job = readJob(worktree, taskId, jobId);
    const value = fn(job);
    writeJsonAtomic(file, job);
    return { job, value };
  });
}

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

export function writeOutbox(
  worktree: string,
  taskId: string,
  report: JobReport,
): string {
  const file = outboxPath(worktree, taskId, report.jobId, report.epoch);
  writeJsonAtomic(file, report);
  return file;
}

export function readOutbox(
  worktree: string,
  taskId: string,
  jobId: string,
  epoch: number,
): JobReport | null {
  try {
    const value = JSON.parse(
      Deno.readTextFileSync(outboxPath(worktree, taskId, jobId, epoch)),
    ) as JobReport;
    if (
      value.jobId !== jobId || value.epoch !== epoch ||
      !Number.isInteger(value.exitCode)
    ) {
      throw new Error(`invalid outbox for ${jobId} epoch ${epoch}`);
    }
    return value;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return null;
    throw error;
  }
}

// ---------------------------------------------------------------------------
// executor loop
// ---------------------------------------------------------------------------

export interface RunningStep {
  /** Resolves with the step's exit code. */
  status: Promise<number>;
  kill(): void;
}

/** Fault points for the B-02 live tests (never set in normal runs). */
export type ExecutorFault =
  /** Exit right after the outbox is written, before the ACK (E-3). */
  | { kind: "exit-after-outbox" }
  /** Stop touching the journal for `ms` after the step starts (E-4/E-5). */
  | { kind: "stall"; ms: number };

export interface ExecutorOptions {
  worktree: string;
  taskId: string;
  jobId: string;
  epoch: number;
  holder: string;
  runStep: (step: StepSpec, logPath: string) => RunningStep;
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
  | { outcome: "stale-rejected" };

/**
 * Run one attempt as its executor: take the attempt, run the step with
 * heartbeats, write the outbox, then apply it. Stops the step when a newer
 * epoch fences this executor.
 */
export async function runExecutor(
  options: ExecutorOptions,
): Promise<ExecutorOutcome> {
  const now = options.now ?? nowIso;
  const sleep = options.sleep ??
    ((ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms)));
  const heartbeatMs = options.heartbeatMs ?? DEFAULT_HEARTBEAT_MS;
  const { worktree, taskId, jobId, epoch, holder } = options;

  const begun = await updateJob(
    worktree,
    taskId,
    jobId,
    (job) => ({
      result: beginAttempt(job, epoch, holder, now()),
      step: job.step,
    }),
  );
  if (!begun.value.result.ok) {
    return { outcome: "refused", reason: begun.value.result.reason };
  }
  const step = begun.value.step;
  if (step === null) {
    return { outcome: "refused", reason: "intent jobs have no executor" };
  }

  const logRef = path.join(FDH_JOBS_DIR, taskId, `${jobId}-${epoch}.log`);
  let running: RunningStep;
  try {
    running = options.runStep(step, path.join(worktree, logRef));
  } catch (error) {
    // The step could not be started: report it as a failed attempt (127).
    try {
      Deno.writeTextFileSync(
        path.join(worktree, logRef),
        `fdh executor: could not start the step: ${
          error instanceof Error ? error.message : String(error)
        }
`,
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

  const stallUntil = options.fault?.kind === "stall"
    ? Date.parse(now()) + options.fault.ms
    : 0;
  while (done.code === null) {
    await Promise.race([status, sleep(heartbeatMs)]);
    if (done.code !== null) break;
    if (Date.parse(now()) < stallUntil) continue;
    const beat = await updateJob(
      worktree,
      taskId,
      jobId,
      (job) => heartbeat(job, epoch, holder, now()),
    );
    if (beat.value === "fenced") {
      running.kill();
      return { outcome: "fenced" };
    }
  }
  while (Date.parse(now()) < stallUntil) {
    await sleep(Math.min(heartbeatMs, stallUntil - Date.parse(now())));
  }

  const report: JobReport = {
    jobId,
    epoch,
    holder,
    exitCode: done.code,
    endedAt: now(),
    logRef,
  };
  writeOutbox(worktree, taskId, report);
  if (options.fault?.kind === "exit-after-outbox") {
    (options.exit ?? Deno.exit)(70);
  }
  const applied = await updateJob(
    worktree,
    taskId,
    jobId,
    (job) => applyReport(job, report, now()),
  );
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
