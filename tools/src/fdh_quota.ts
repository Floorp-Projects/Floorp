// SPDX-License-Identifier: MPL-2.0

/**
 * fdh LLM-quota / assignment ledger (FDH Gate 4 4-D, KAN-122).
 *
 * Records who holds a task, quota/infra/manual stops, resumes, pending
 * non-idempotent external operations awaiting approval, and approvals. The
 * ledger is local-only (`<worktree>/_dist/fdh/quota/<task-id>.json`) and never
 * calls an LLM or a shared service. It prevents double assignment of a stopped
 * task: a claim by another agent while an active holder exists is rejected.
 */

import * as path from "@std/path";
import { FDH_DIR, nowIso, validateTaskId } from "./fdh_task.ts";

export const FDH_QUOTA_DIR = path.join(FDH_DIR, "quota");

/** Why a task was stopped (quota exhaustion, infrastructure, or manual). */
export const QUOTA_STOP_REASONS = ["quota", "infra", "manual"] as const;
export type QuotaStopReason = (typeof QUOTA_STOP_REASONS)[number];

/** Unknown non-idempotent external operations that must wait for approval. */
export const PENDING_EXTERNAL_OPS = [
  "push",
  "pr",
  "release",
  "external",
] as const;
export type PendingExternalOp = (typeof PENDING_EXTERNAL_OPS)[number];

export const QUOTA_EVENT_KINDS = [
  "claim",
  "release",
  "stop",
  "resume",
  "pending",
  "approve",
] as const;
export type QuotaEventKind = (typeof QUOTA_EVENT_KINDS)[number];

export interface QuotaEvent {
  at: string;
  kind: QuotaEventKind;
  agent?: string;
  reason?: QuotaStopReason;
  op?: PendingExternalOp;
  note?: string;
}

export interface PendingApproval {
  op: PendingExternalOp;
  note: string;
  at: string;
}

export interface FdhQuotaLedger {
  schemaVersion: 1;
  taskId: string;
  worktree: string;
  createdAt: string;
  updatedAt: string;
  /** Agent currently holding the task; null when stopped/released. */
  activeAgent: string | null;
  stopped: boolean;
  stopReason: QuotaStopReason | null;
  /** Non-idempotent external operations that must not be rerun before approval. */
  approvalPending: PendingApproval[];
  events: QuotaEvent[];
}

export interface CreateQuotaLedgerInput {
  taskId: string;
  worktree: string;
  now?: () => string;
}

export function quotaLedgerPath(worktree: string, taskId: string): string {
  validateTaskId(taskId);
  return path.join(worktree, FDH_QUOTA_DIR, `${taskId}.json`);
}

export function createQuotaLedger(
  input: CreateQuotaLedgerInput,
): FdhQuotaLedger {
  validateTaskId(input.taskId);
  const at = (input.now ?? nowIso)();
  return {
    schemaVersion: 1,
    taskId: input.taskId,
    worktree: input.worktree,
    createdAt: at,
    updatedAt: at,
    activeAgent: null,
    stopped: false,
    stopReason: null,
    approvalPending: [],
    events: [],
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function assertString(
  value: unknown,
  name: string,
  allowEmpty = false,
): void {
  if (typeof value !== "string" || (!allowEmpty && value === "")) {
    throw new Error(
      `invalid fdh quota ledger: ${name} must be a ${
        allowEmpty ? "" : "non-empty "
      }string`,
    );
  }
}

/** Runtime shape check for a ledger read back from disk. */
export function parseQuotaLedger(value: unknown): FdhQuotaLedger {
  if (!isRecord(value)) {
    throw new Error("invalid fdh quota ledger: expected a JSON object");
  }
  if (value.schemaVersion !== 1) {
    throw new Error("invalid fdh quota ledger: schemaVersion must be 1");
  }
  assertString(value.taskId, "taskId");
  validateTaskId(value.taskId as string);
  for (const key of ["worktree", "createdAt", "updatedAt"] as const) {
    assertString(value[key], key);
  }
  if (value.activeAgent !== null && typeof value.activeAgent !== "string") {
    throw new Error(
      "invalid fdh quota ledger: activeAgent must be a string or null",
    );
  }
  if (typeof value.stopped !== "boolean") {
    throw new Error("invalid fdh quota ledger: stopped must be a boolean");
  }
  if (
    value.stopReason !== null &&
    !QUOTA_STOP_REASONS.includes(value.stopReason as QuotaStopReason)
  ) {
    throw new Error("invalid fdh quota ledger: stopReason is not admissible");
  }
  if (!Array.isArray(value.approvalPending) || !Array.isArray(value.events)) {
    throw new Error(
      "invalid fdh quota ledger: approvalPending and events must be arrays",
    );
  }
  for (const pending of value.approvalPending) {
    if (!isRecord(pending)) {
      throw new Error(
        "invalid fdh quota ledger: approvalPending entries must be objects",
      );
    }
    if (!PENDING_EXTERNAL_OPS.includes(pending.op as PendingExternalOp)) {
      throw new Error(
        "invalid fdh quota ledger: approvalPending.op is not admissible",
      );
    }
    // `quota pending --note` is optional, so an empty note is admissible.
    assertString(pending.note, "approvalPending.note", true);
    assertString(pending.at, "approvalPending.at");
  }
  for (const event of value.events) {
    if (!isRecord(event)) {
      throw new Error(
        "invalid fdh quota ledger: events entries must be objects",
      );
    }
    if (!QUOTA_EVENT_KINDS.includes(event.kind as QuotaEventKind)) {
      throw new Error(
        "invalid fdh quota ledger: events.kind is not admissible",
      );
    }
    assertString(event.at, "events.at");
    if (event.agent !== undefined) assertString(event.agent, "events.agent");
    if (
      event.reason !== undefined &&
      !QUOTA_STOP_REASONS.includes(event.reason as QuotaStopReason)
    ) {
      throw new Error(
        "invalid fdh quota ledger: events.reason is not admissible",
      );
    }
    if (
      event.op !== undefined &&
      !PENDING_EXTERNAL_OPS.includes(event.op as PendingExternalOp)
    ) {
      throw new Error(
        "invalid fdh quota ledger: events.op is not admissible",
      );
    }
    if (event.note !== undefined) {
      assertString(event.note, "events.note", true);
    }
  }
  return value as unknown as FdhQuotaLedger;
}

export function quotaLedgerExists(worktree: string, taskId: string): boolean {
  try {
    return Deno.statSync(quotaLedgerPath(worktree, taskId)).isFile;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false;
    throw error;
  }
}

/** Read an existing ledger, or create one when `create` is true. */
export function readQuotaLedger(
  worktree: string,
  taskId: string,
  create = false,
): FdhQuotaLedger {
  const file = quotaLedgerPath(worktree, taskId);
  try {
    const text = Deno.readTextFileSync(file);
    return parseQuotaLedger(JSON.parse(text));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      if (!create) throw new Error(`no fdh quota ledger for task ${taskId}`);
      return createQuotaLedger({ taskId, worktree });
    }
    throw new Error(
      `invalid fdh quota ledger at ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
}

/** Write the ledger atomically (tmp file + rename). */
export function writeQuotaLedger(ledger: FdhQuotaLedger): string {
  const file = quotaLedgerPath(ledger.worktree, ledger.taskId);
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.tmp-${Deno.pid}`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(ledger, null, 2)}\n`);
  Deno.renameSync(tmp, file);
  return file;
}

function appendQuotaEvent(ledger: FdhQuotaLedger, event: QuotaEvent): void {
  ledger.events.push(event);
  ledger.updatedAt = event.at;
}

/** Validate `--agent`; returns it trimmed. */
export function requireAgent(agent: string): string {
  if (typeof agent !== "string" || agent.trim() === "") {
    throw new Error("--agent must be a non-empty string");
  }
  return agent.trim();
}

/** Validate `--reason` for `quota stop`. */
export function assertQuotaStopReason(
  reason: string,
): asserts reason is QuotaStopReason {
  if (!QUOTA_STOP_REASONS.includes(reason as QuotaStopReason)) {
    throw new Error(
      `--reason must be one of ${QUOTA_STOP_REASONS.join("|")}, got ${
        JSON.stringify(reason)
      }`,
    );
  }
}

/** Validate `--op` for `quota pending`. */
export function assertPendingExternalOp(
  op: string,
): asserts op is PendingExternalOp {
  if (!PENDING_EXTERNAL_OPS.includes(op as PendingExternalOp)) {
    throw new Error(
      `--op must be one of ${PENDING_EXTERNAL_OPS.join("|")}, got ${
        JSON.stringify(op)
      }`,
    );
  }
}

/**
 * Claim a task for `agent`. Rejects a second holder while the first is active,
 * which prevents double assignment of the same work.
 */
export function claimQuota(
  ledger: FdhQuotaLedger,
  agent: string,
  at = nowIso(),
): FdhQuotaLedger {
  const holder = requireAgent(agent);
  if (ledger.activeAgent !== null && ledger.activeAgent !== holder) {
    throw new Error(
      `task ${ledger.taskId} is already assigned to ${ledger.activeAgent}; ` +
        "release it before assigning another agent",
    );
  }
  if (ledger.stopped) {
    throw new Error(
      `task ${ledger.taskId} is stopped; resume it before claiming`,
    );
  }
  ledger.activeAgent = holder;
  appendQuotaEvent(ledger, { at, kind: "claim", agent: holder });
  return ledger;
}

/** Stop a task for a quota/infra/manual reason; only the active holder may stop. */
export function stopQuota(
  ledger: FdhQuotaLedger,
  agent: string,
  reason: QuotaStopReason,
  note = "",
  at = nowIso(),
): FdhQuotaLedger {
  const holder = requireAgent(agent);
  assertQuotaStopReason(reason);
  if (ledger.activeAgent !== holder) {
    throw new Error(
      `agent ${holder} does not hold task ${ledger.taskId}` +
        (ledger.activeAgent
          ? ` (holder is ${ledger.activeAgent})`
          : " (no holder)"),
    );
  }
  ledger.activeAgent = null;
  ledger.stopped = true;
  ledger.stopReason = reason;
  appendQuotaEvent(ledger, { at, kind: "stop", agent: holder, reason, note });
  return ledger;
}

/** Resume a stopped task for `agent`. */
export function resumeQuota(
  ledger: FdhQuotaLedger,
  agent: string,
  note = "",
  at = nowIso(),
): FdhQuotaLedger {
  const holder = requireAgent(agent);
  if (!ledger.stopped) {
    throw new Error(`task ${ledger.taskId} is not stopped`);
  }
  ledger.stopped = false;
  ledger.stopReason = null;
  ledger.activeAgent = holder;
  appendQuotaEvent(ledger, { at, kind: "resume", agent: holder, note });
  return ledger;
}

/**
 * Record an unknown non-idempotent external operation awaiting approval.
 *
 * Idempotent per (op, note): when the same operation is already pending, the
 * ledger is left unchanged and `false` is returned. `fdh quota pending`
 * writes the ledger and then the task-state event in two steps, so a retry
 * after the second step failed would otherwise queue the operation twice.
 * `approveQuota` clears every entry at once, so a second identical entry
 * would carry no information. Returns `true` when an entry was added.
 */
export function pendingQuota(
  ledger: FdhQuotaLedger,
  op: PendingExternalOp,
  note = "",
  at = nowIso(),
): boolean {
  assertPendingExternalOp(op);
  if (
    ledger.approvalPending.some((entry) =>
      entry.op === op && entry.note === note
    )
  ) {
    return false;
  }
  ledger.approvalPending.push({ op, note, at });
  appendQuotaEvent(ledger, { at, kind: "pending", op, note });
  return true;
}

/**
 * Approve one pending entry (same op and note), e.g. the one an fdh job put
 * there when its intent's outcome became unknown and an approver resolved
 * it. Returns false when no such entry is pending.
 */
export function approvePendingEntry(
  ledger: FdhQuotaLedger,
  op: PendingExternalOp,
  note: string,
  approval = "",
  at = nowIso(),
): boolean {
  const index = ledger.approvalPending.findIndex((entry) =>
    entry.op === op && entry.note === note
  );
  if (index < 0) return false;
  ledger.approvalPending.splice(index, 1);
  appendQuotaEvent(ledger, {
    at,
    kind: "approve",
    op,
    note: `${approval} ${note}`.trim(),
  });
  return true;
}

/** Approve all pending external operations; they may be retried afterwards. */
export function approveQuota(
  ledger: FdhQuotaLedger,
  note = "",
  at = nowIso(),
): FdhQuotaLedger {
  if (ledger.approvalPending.length === 0) {
    throw new Error(`task ${ledger.taskId} has no pending approval`);
  }
  const approved = ledger.approvalPending.map((entry) => entry.op);
  ledger.approvalPending = [];
  appendQuotaEvent(ledger, {
    at,
    kind: "approve",
    note: `${note} ${approved.join(",")}`.trim(),
  });
  return ledger;
}

/** Release the active holder without stopping the task. */
export function releaseQuota(
  ledger: FdhQuotaLedger,
  agent: string,
  at = nowIso(),
): FdhQuotaLedger {
  const holder = requireAgent(agent);
  if (ledger.activeAgent !== holder) {
    throw new Error(`agent ${holder} does not hold task ${ledger.taskId}`);
  }
  ledger.activeAgent = null;
  appendQuotaEvent(ledger, { at, kind: "release", agent: holder });
  return ledger;
}

export interface QuotaSummary {
  taskId: string;
  activeAgent: string | null;
  stopped: boolean;
  stopReason: QuotaStopReason | null;
  approvalPending: PendingApproval[];
  events: number;
  updatedAt: string;
}

export function summarizeQuota(ledger: FdhQuotaLedger): QuotaSummary {
  return {
    taskId: ledger.taskId,
    activeAgent: ledger.activeAgent,
    stopped: ledger.stopped,
    stopReason: ledger.stopReason,
    approvalPending: ledger.approvalPending,
    events: ledger.events.length,
    updatedAt: ledger.updatedAt,
  };
}
