// SPDX-License-Identifier: MPL-2.0

/**
 * fdh GUI-desktop lease (FDH Gate 4 4-A, KAN-108).
 *
 * A shared desktop GUI is a single resource: only one agent may hold the
 * desktop lease at a time. The lease lives beside the git common directory
 * (`<git-common-dir>/fdh/gui-desktop-lease.json`), so all worktrees of the
 * same repository share it, and it is never part of the tracked tree. The
 * holder may end (normal completion) or cancel the lease; another agent may
 * only claim after the holder released it. The lease is local-only and never
 * calls a shared service.
 */

import * as path from "@std/path";
import { nowIso, validateTaskId } from "./fdh_task.ts";

export const FDH_GUI_LEASE_FILE = path.join("fdh", "gui-desktop-lease.json");

/** How a GUI-desktop lease ended. */
export const GUI_LEASE_END_REASONS = ["ended", "cancelled"] as const;
export type GuiLeaseEndReason = (typeof GUI_LEASE_END_REASONS)[number];

export type GuiLeaseEventKind = "claim" | "end" | "cancel";

export interface GuiLeaseEvent {
  at: string;
  kind: GuiLeaseEventKind;
  agent: string;
  taskId: string;
  reason?: GuiLeaseEndReason;
  note?: string;
}

export interface FdhGuiLease {
  schemaVersion: 1;
  /** Agent currently holding the desktop; null when released. */
  holder: string | null;
  taskId: string | null;
  /** Worktree the holder is driving; null when released. */
  worktree: string | null;
  acquiredAt: string | null;
  status: "active" | "ended" | "cancelled";
  endedAt: string | null;
  events: GuiLeaseEvent[];
}

export interface CreateGuiLeaseInput {
  now?: () => string;
}

export function createGuiLease(input: CreateGuiLeaseInput = {}): FdhGuiLease {
  return {
    schemaVersion: 1,
    holder: null,
    taskId: null,
    worktree: null,
    acquiredAt: null,
    status: "ended",
    endedAt: (input.now ?? nowIso)(),
    events: [],
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function assertString(value: unknown, name: string): void {
  if (typeof value !== "string" || value === "") {
    throw new Error(
      `invalid fdh gui lease: ${name} must be a non-empty string`,
    );
  }
}

export function parseGuiLease(value: unknown): FdhGuiLease {
  if (!isRecord(value)) {
    throw new Error("invalid fdh gui lease: expected a JSON object");
  }
  if (value.schemaVersion !== 1) {
    throw new Error("invalid fdh gui lease: schemaVersion must be 1");
  }
  if (value.holder !== null && typeof value.holder !== "string") {
    throw new Error("invalid fdh gui lease: holder must be a string or null");
  }
  if (value.taskId !== null) {
    assertString(value.taskId, "taskId");
    validateTaskId(value.taskId as string);
  }
  if (value.worktree !== null) assertString(value.worktree, "worktree");
  if (value.acquiredAt !== null) assertString(value.acquiredAt, "acquiredAt");
  if (!["active", "ended", "cancelled"].includes(value.status as string)) {
    throw new Error("invalid fdh gui lease: status is not admissible");
  }
  if (value.endedAt !== null) assertString(value.endedAt, "endedAt");
  if (!Array.isArray(value.events)) {
    throw new Error("invalid fdh gui lease: events must be an array");
  }
  for (const event of value.events) {
    if (!isRecord(event)) {
      throw new Error("invalid fdh gui lease: events entries must be objects");
    }
    if (!["claim", "end", "cancel"].includes(event.kind as string)) {
      throw new Error("invalid fdh gui lease: events.kind is not admissible");
    }
    assertString(event.agent, "events.agent");
    assertString(event.taskId, "events.taskId");
    assertString(event.at, "events.at");
    if (
      event.reason !== undefined &&
      !GUI_LEASE_END_REASONS.includes(event.reason as GuiLeaseEndReason)
    ) {
      throw new Error("invalid fdh gui lease: events.reason is not admissible");
    }
    if (event.note !== undefined && typeof event.note !== "string") {
      throw new Error("invalid fdh gui lease: events.note must be a string");
    }
  }
  if (value.status === "active") {
    if (value.holder === null || value.taskId === null) {
      throw new Error(
        "invalid fdh gui lease: an active lease requires holder and taskId",
      );
    }
  }
  return value as unknown as FdhGuiLease;
}

/** The lease path beside the git common directory (shared by all worktrees). */
export async function guiLeasePathForWorktree(
  worktree: string,
): Promise<string> {
  const result = await new Deno.Command("git", {
    args: ["rev-parse", "--git-common-dir"],
    cwd: worktree,
    stdout: "piped",
    stderr: "piped",
  }).output();
  if (!result.success) {
    throw new Error(
      `cannot resolve git common dir for ${worktree}: ${
        new TextDecoder().decode(result.stderr).trim()
      }`,
    );
  }
  const raw = new TextDecoder().decode(result.stdout).trim();
  const common = path.resolve(worktree, raw);
  return path.join(common, FDH_GUI_LEASE_FILE);
}

export function readGuiLease(file: string): FdhGuiLease {
  try {
    return parseGuiLease(JSON.parse(Deno.readTextFileSync(file)));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) {
      return createGuiLease();
    }
    throw new Error(
      `invalid fdh gui lease at ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
}

/** Write the lease atomically (tmp file + rename). */
export function writeGuiLease(file: string, lease: FdhGuiLease): string {
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.tmp-${Deno.pid}`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(lease, null, 2)}\n`);
  Deno.renameSync(tmp, file);
  return file;
}

function requireAgent(agent: string): string {
  if (typeof agent !== "string" || agent.trim() === "") {
    throw new Error("--agent must be a non-empty string");
  }
  return agent.trim();
}

/** Claim the shared desktop for `agent`/`taskId`; a second holder is rejected. */
export function claimGuiLease(
  lease: FdhGuiLease,
  agent: string,
  taskId: string,
  worktree: string,
  at = nowIso(),
): FdhGuiLease {
  const holder = requireAgent(agent);
  validateTaskId(taskId);
  if (lease.status === "active") {
    if (lease.holder === holder && lease.taskId === taskId) {
      return lease; // idempotent re-claim by the same holder
    }
    throw new Error(
      `GUI desktop lease is held by ${lease.holder} for task ${lease.taskId}` +
        `; end or cancel it before claiming`,
    );
  }
  lease.holder = holder;
  lease.taskId = taskId;
  lease.worktree = worktree;
  lease.acquiredAt = at;
  lease.status = "active";
  lease.endedAt = null;
  lease.events.push({ at, kind: "claim", agent: holder, taskId });
  return lease;
}

/** End or cancel the lease; only the current holder may release it. */
export function endGuiLease(
  lease: FdhGuiLease,
  agent: string,
  reason: GuiLeaseEndReason,
  note = "",
  at = nowIso(),
): FdhGuiLease {
  const holder = requireAgent(agent);
  if (!GUI_LEASE_END_REASONS.includes(reason)) {
    throw new Error(
      `--reason must be one of ${GUI_LEASE_END_REASONS.join("|")}, got ${
        JSON.stringify(reason)
      }`,
    );
  }
  if (lease.status !== "active") {
    throw new Error("GUI desktop lease is not active");
  }
  if (lease.holder !== holder) {
    throw new Error(
      `agent ${holder} does not hold the GUI desktop lease (holder is ${lease.holder})`,
    );
  }
  const taskId = lease.taskId as string;
  lease.status = reason === "cancelled" ? "cancelled" : "ended";
  lease.endedAt = at;
  lease.events.push({
    at,
    kind: reason === "cancelled" ? "cancel" : "end",
    agent: holder,
    taskId,
    reason,
    note,
  });
  lease.holder = null;
  lease.taskId = null;
  lease.worktree = null;
  return lease;
}

/** Compact status summary for CLI output. */
export function summarizeGuiLease(lease: FdhGuiLease): {
  schemaVersion: 1;
  status: FdhGuiLease["status"];
  holder: string | null;
  taskId: string | null;
  acquiredAt: string | null;
  endedAt: string | null;
  events: number;
} {
  return {
    schemaVersion: 1,
    status: lease.status,
    holder: lease.holder,
    taskId: lease.taskId,
    acquiredAt: lease.acquiredAt,
    endedAt: lease.endedAt,
    events: lease.events.length,
  };
}
