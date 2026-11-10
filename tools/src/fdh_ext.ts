// SPDX-License-Identifier: MPL-2.0

/**
 * External GitHub / Jira operations for fdh (D-19, KAN-131, audit I-04).
 *
 * D-19: agents operate GitHub (`gh`) and Jira (Atlassian MCP) directly; fdh
 * records those operations and reconciles by polling. There is no resident
 * webhook service, and fdh itself calls no GitHub or Jira API here: an agent
 * (or a poller) feeds observations in, fdh decides what is in sync, what to
 * change and what to hold, and the agent performs the change and records it.
 *
 * - `FIELD_RULES` is the I-04 table: for each synced field, the system that
 *   owns it, where it is mirrored, and the conflict / deletion / retry rule.
 * - The link table ties a Jira issue to its GitHub pull request and branch.
 * - Observations are versioned (the API's update time). A repeated event is a
 *   duplicate, an older version than the one stored is stale (ordering
 *   inversion), and the same version with another value is a conflict.
 * - The planner never overwrites a mirror that changed since fdh last wrote
 *   it (no generic compare-and-set is assumed): if the owner changed too, the
 *   field is held for an approver; if only the mirror changed, the edit is
 *   kept.
 * - Operations are keyed. A key that already succeeded is not performed
 *   again (no duplicate PR or comment); an operation in flight is not
 *   started twice; an outcome that is unknown goes to the approval queue
 *   through the intent job (B-02, `job resolve`).
 *
 * The ledger is `<worktree>/_dist/fdh/ext/<task-id>.json`, updated under a
 * file lock.
 */

import * as path from "@std/path";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import { fdhDir, type FdhTaskState, nowIso } from "./fdh_task.ts";
import { type IntentOp, isTerminal, readJob } from "./fdh_executor.ts";
import { confirmJob, startIntent } from "./fdh_job.ts";

export type ExtSystem = "github" | "jira";

/** One row of the I-04 field table. */
export interface FieldRule {
  /** Field on the owner's object (`pr.*` on GitHub, `jira.*` on Jira). */
  field: string;
  /** The system of record for this field. */
  owner: ExtSystem;
  /** Where fdh reflects it, or null when it is not synced at all. */
  mirror: { system: ExtSystem; field: string } | null;
  /** Owner value to mirror value; null means "no change for this value". */
  derive?: (ownerValue: string) => string | null;
  /**
   * Mirror values in workflow order: fdh never moves the mirror backwards
   * (a Jira issue someone already closed is not reopened by a PR event).
   */
  order?: readonly string[];
  /** How a retried write behaves. */
  retry: "idempotent" | "keyed-once" | "none";
  note: string;
}

/** Jira statuses of the KAN project that a pull request's state moves to. */
export const PR_STATE_TO_JIRA_STATUS: Readonly<Record<string, string>> = {
  draft: "進行中",
  open: "レビュー中",
  merged: "完了",
};

export const FIELD_RULES: readonly FieldRule[] = [
  {
    field: "pr.url",
    owner: "github",
    mirror: { system: "jira", field: "remote-link" },
    retry: "idempotent",
    note:
      "A Jira remote link to the PR (keyed by URL, so a retry updates the same link).",
  },
  {
    field: "pr.state",
    owner: "github",
    mirror: { system: "jira", field: "status" },
    derive: (state) => PR_STATE_TO_JIRA_STATUS[state] ?? null,
    order: ["未着手", "進行中", "レビュー中", "完了"],
    retry: "idempotent",
    note:
      "draft/open/merged move the Jira issue to 進行中/レビュー中/完了; closed changes nothing. Jira owns its status: a status a person set since fdh's last write is kept.",
  },
  {
    field: "ci.result",
    owner: "github",
    mirror: { system: "jira", field: "ci-comment" },
    derive: (value) =>
      /^[0-9a-f]{7,40}:(success|failure)$/.test(value) ? value : null,
    retry: "keyed-once",
    note:
      "`<head sha>:<success|failure>`: one fdh summary comment per head and result (pending is not posted).",
  },
  {
    field: "pr.review-receipts",
    owner: "github",
    mirror: null,
    retry: "none",
    note: "D-14 receipts stay on the PR (review-gate reads them there).",
  },
  {
    field: "jira.sprint",
    owner: "jira",
    mirror: null,
    retry: "none",
    note:
      "Jira owns sprint, assignee, priority and QA state; not copied to GitHub.",
  },
  {
    field: "jira.assignee",
    owner: "jira",
    mirror: null,
    retry: "none",
    note: "See jira.sprint.",
  },
  {
    field: "jira.priority",
    owner: "jira",
    mirror: null,
    retry: "none",
    note: "See jira.sprint.",
  },
  {
    field: "jira.qa-state",
    owner: "jira",
    mirror: null,
    retry: "none",
    note: "See jira.sprint.",
  },
];

/** A reference to an object: `github:pr/<n>` or `jira:<KEY-n>`. */
export function parseRef(ref: string): { system: ExtSystem; id: string } {
  const github = /^github:pr\/([1-9][0-9]*)$/.exec(ref);
  if (github) return { system: "github", id: github[1] };
  const jira = /^jira:([A-Z][A-Z0-9]*-[1-9][0-9]*)$/.exec(ref);
  if (jira) return { system: "jira", id: jira[1] };
  throw new Error(
    `invalid ref ${JSON.stringify(ref)}: use github:pr/<n> or jira:<KEY-n>`,
  );
}

export interface Observation {
  value: string | null;
  /** The object's update time from the API (ISO); orders observations. */
  version: string;
  observedAt: string;
  deleted: boolean;
}

export interface SyncedField {
  ownerValue: string;
  mirrorValue: string;
  at: string;
}

export interface ExtLink {
  /** The Jira issue key is the link id. */
  jira: string;
  pr: number | null;
  branch: string | null;
  synced: Record<string, SyncedField>;
}

export interface ExtOp {
  key: string;
  jobId: string;
  epoch: number;
  startedAt: string;
  outcome: "succeeded" | "failed" | null;
}

export interface ExtEvent {
  at: string;
  kind: "duplicate" | "stale" | "conflict" | "deleted" | "held" | "kept-edit";
  ref: string;
  field: string;
  note: string;
}

export interface ExtLedger {
  version: 1;
  taskId: string;
  links: Record<string, ExtLink>;
  observations: Record<string, Record<string, Observation>>;
  /** Event ids already applied, per ref (bounded). */
  seenEvents: Record<string, string[]>;
  ops: Record<string, ExtOp>;
  events: ExtEvent[];
}

const MAX_SEEN_EVENTS = 200;
const MAX_EVENTS = 500;

export function emptyLedger(taskId: string): ExtLedger {
  return {
    version: 1,
    taskId,
    links: {},
    observations: {},
    seenEvents: {},
    ops: {},
    events: [],
  };
}

function requireIso(value: string, name: string): number {
  const at = Date.parse(value);
  if (!Number.isFinite(at)) {
    throw new Error(
      `${name} must be an ISO time, got ${JSON.stringify(value)}`,
    );
  }
  return at;
}

function note(ledger: ExtLedger, event: ExtEvent): void {
  ledger.events.push(event);
  if (ledger.events.length > MAX_EVENTS) {
    ledger.events.splice(0, ledger.events.length - MAX_EVENTS);
  }
}

// ---------------------------------------------------------------------------
// links
// ---------------------------------------------------------------------------

export function upsertLink(
  ledger: ExtLedger,
  jira: string,
  pr?: number,
  branch?: string,
): ExtLink {
  parseRef(`jira:${jira}`);
  const link = ledger.links[jira] ??
    { jira, pr: null, branch: null, synced: {} };
  if (pr !== undefined) {
    if (link.pr !== null && link.pr !== pr) {
      throw new Error(
        `${jira} is already linked to PR #${link.pr}; unlink it first`,
      );
    }
    const other = Object.values(ledger.links).find((l) =>
      l.jira !== jira && l.pr === pr
    );
    if (other) throw new Error(`PR #${pr} is already linked to ${other.jira}`);
    link.pr = pr;
  }
  if (branch !== undefined) link.branch = branch;
  ledger.links[jira] = link;
  return link;
}

// ---------------------------------------------------------------------------
// observations
// ---------------------------------------------------------------------------

export type IngestResult =
  | "applied"
  | "duplicate"
  | "stale"
  | "conflict"
  | "deleted";

export interface ObservationInput {
  ref: string;
  field: string;
  value: string | null;
  version: string;
  eventId?: string;
  deleted?: boolean;
  at?: string;
}

/**
 * Take one polled value. Ordering is by `version` (the API's update time):
 * an older version than the stored one is stale and ignored, a repeat of the
 * stored value or of a seen event id is a duplicate, and the same version
 * with another value is a conflict (kept out, recorded). A deletion is
 * recorded as such and never undone by a stale observation.
 */
export function ingestObservation(
  ledger: ExtLedger,
  input: ObservationInput,
): IngestResult {
  parseRef(input.ref);
  if (input.field === "") throw new Error("field must not be empty");
  const version = requireIso(input.version, "version");
  const at = input.at ?? nowIso();
  const seen = ledger.seenEvents[input.ref] ??= [];
  if (input.eventId !== undefined && seen.includes(input.eventId)) {
    note(ledger, {
      at,
      kind: "duplicate",
      ref: input.ref,
      field: input.field,
      note: `event ${input.eventId} seen before`,
    });
    return "duplicate";
  }
  const fields = ledger.observations[input.ref] ??= {};
  const current = fields[input.field];
  const remember = () => {
    if (input.eventId === undefined) return;
    seen.push(input.eventId);
    if (seen.length > MAX_SEEN_EVENTS) {
      seen.splice(0, seen.length - MAX_SEEN_EVENTS);
    }
  };
  const deleted = input.deleted === true;
  const value = deleted ? null : input.value;
  if (current !== undefined) {
    const stored = Date.parse(current.version);
    if (version < stored) {
      remember();
      note(ledger, {
        at,
        kind: "stale",
        ref: input.ref,
        field: input.field,
        note: `version ${input.version} is older than ${current.version}`,
      });
      return "stale";
    }
    if (version === stored) {
      remember();
      if (current.value === value && current.deleted === deleted) {
        note(ledger, {
          at,
          kind: "duplicate",
          ref: input.ref,
          field: input.field,
          note: `same value at ${input.version}`,
        });
        return "duplicate";
      }
      note(ledger, {
        at,
        kind: "conflict",
        ref: input.ref,
        field: input.field,
        note: `${JSON.stringify(value)} and stored ${
          JSON.stringify(current.value)
        } share version ${input.version}`,
      });
      return "conflict";
    }
    if (current.deleted && !deleted) {
      // A deleted object does not come back from a later observation either:
      // that would be a new object under a reused id. Keep the deletion.
      remember();
      note(ledger, {
        at,
        kind: "conflict",
        ref: input.ref,
        field: input.field,
        note: `observed again after its deletion at ${current.version}`,
      });
      return "conflict";
    }
  }
  fields[input.field] = {
    value,
    version: input.version,
    observedAt: at,
    deleted,
  };
  remember();
  if (deleted) {
    note(ledger, {
      at,
      kind: "deleted",
      ref: input.ref,
      field: input.field,
      note: `deleted at ${input.version}`,
    });
    return "deleted";
  }
  return "applied";
}

// ---------------------------------------------------------------------------
// planning
// ---------------------------------------------------------------------------

export interface SyncAction {
  link: string;
  field: string;
  system: ExtSystem;
  ref: string;
  mirrorField: string;
  from: string | null;
  to: string;
  ownerValue: string;
  /** Operation key for `ext op start` (the same change has the same key). */
  key: string;
  retry: FieldRule["retry"];
}

export interface SyncHold {
  link: string;
  field: string;
  reason:
    | "concurrent-edit"
    | "deleted"
    | "awaiting-poll"
    | "op-in-flight";
  note: string;
}

export interface SyncPlan {
  actions: SyncAction[];
  holds: SyncHold[];
  inSync: Array<{ link: string; field: string }>;
  /** A mirror edited by someone else while the owner did not change. */
  keptEdits: Array<{ link: string; field: string; value: string | null }>;
}

function refFor(link: ExtLink, system: ExtSystem): string | null {
  if (system === "jira") return `jira:${link.jira}`;
  return link.pr === null ? null : `github:pr/${link.pr}`;
}

export function opKey(link: string, field: string, to: string): string {
  return `${link}:${field}:${to}`;
}

/**
 * What the agent should change, hold or leave, from the latest observations.
 * Pure: the ledger is not changed.
 */
export function planSync(ledger: ExtLedger): SyncPlan {
  const plan: SyncPlan = { actions: [], holds: [], inSync: [], keptEdits: [] };
  for (const link of Object.values(ledger.links)) {
    for (const rule of FIELD_RULES) {
      if (rule.mirror === null) continue;
      const ownerRef = refFor(link, rule.owner);
      const mirrorRef = refFor(link, rule.mirror.system);
      if (ownerRef === null || mirrorRef === null) continue;
      const owner = ledger.observations[ownerRef]?.[rule.field];
      if (owner === undefined) continue;
      const mirror = ledger.observations[mirrorRef]?.[rule.mirror.field];
      // A deleted object (field "object") or a deleted field: nothing more is
      // written to it or because of it.
      const gone = (ref: string, field: string) =>
        ledger.observations[ref]?.object?.deleted === true ||
        ledger.observations[ref]?.[field]?.deleted === true;
      const deletedSide = gone(ownerRef, rule.field)
        ? ownerRef
        : gone(mirrorRef, rule.mirror.field)
        ? mirrorRef
        : null;
      if (deletedSide !== null) {
        plan.holds.push({
          link: link.jira,
          field: rule.field,
          reason: "deleted",
          note:
            `${deletedSide} was deleted; nothing is written to it or for it`,
        });
        continue;
      }
      if (owner.value === null) continue;
      const desired = rule.derive ? rule.derive(owner.value) : owner.value;
      if (desired === null) continue;
      if (mirror?.value === desired) {
        plan.inSync.push({ link: link.jira, field: rule.field });
        continue;
      }
      if (rule.order !== undefined && mirror?.value != null) {
        const at = rule.order.indexOf(mirror.value);
        if (at >= 0 && at > rule.order.indexOf(desired)) {
          plan.keptEdits.push({
            link: link.jira,
            field: rule.field,
            value: mirror.value,
          });
          continue;
        }
      }
      const key = opKey(link.jira, rule.field, desired);
      const op = ledger.ops[key];
      if (op !== undefined && op.outcome === null) {
        plan.holds.push({
          link: link.jira,
          field: rule.field,
          reason: "op-in-flight",
          note: `operation ${key} (job ${op.jobId}) has no outcome yet`,
        });
        continue;
      }
      const synced = link.synced[rule.field];
      if (synced !== undefined) {
        const syncedAt = Date.parse(synced.at);
        if (mirror === undefined || Date.parse(mirror.version) < syncedAt) {
          // fdh wrote the mirror after the last observation of it: wait for
          // a poll that shows the write before deciding anything.
          plan.holds.push({
            link: link.jira,
            field: rule.field,
            reason: "awaiting-poll",
            note:
              `no observation of ${mirrorRef} ${rule.mirror.field} since fdh wrote it at ${synced.at}`,
          });
          continue;
        }
        const mirrorEdited = mirror.value !== synced.mirrorValue;
        const ownerChanged = owner.value !== synced.ownerValue;
        if (mirrorEdited && ownerChanged) {
          plan.holds.push({
            link: link.jira,
            field: rule.field,
            reason: "concurrent-edit",
            note: `${mirrorRef} ${rule.mirror.field} became ${
              JSON.stringify(mirror.value)
            } and ${ownerRef} ${rule.field} became ${
              JSON.stringify(owner.value)
            } since fdh wrote ${JSON.stringify(synced.mirrorValue)}`,
          });
          continue;
        }
        if (mirrorEdited) {
          plan.keptEdits.push({
            link: link.jira,
            field: rule.field,
            value: mirror.value,
          });
          continue;
        }
        if (!ownerChanged) continue;
      }
      plan.actions.push({
        link: link.jira,
        field: rule.field,
        system: rule.mirror.system,
        ref: mirrorRef,
        mirrorField: rule.mirror.field,
        from: mirror?.value ?? null,
        to: desired,
        ownerValue: owner.value,
        key,
        retry: rule.retry,
      });
    }
  }
  return plan;
}

// ---------------------------------------------------------------------------
// ledger file
// ---------------------------------------------------------------------------

export function extLedgerPath(worktree: string, taskId: string): string {
  return path.join(fdhDir(worktree), "ext", `${taskId}.json`);
}

export function readExtLedger(worktree: string, taskId: string): ExtLedger {
  let text: string;
  try {
    text = Deno.readTextFileSync(extLedgerPath(worktree, taskId));
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return emptyLedger(taskId);
    throw error;
  }
  const value = JSON.parse(text) as ExtLedger;
  if (
    value === null || typeof value !== "object" || value.version !== 1 ||
    value.taskId !== taskId || typeof value.links !== "object" ||
    typeof value.observations !== "object" || typeof value.ops !== "object" ||
    !Array.isArray(value.events)
  ) {
    throw new Error(
      `invalid fdh ext ledger: ${extLedgerPath(worktree, taskId)}`,
    );
  }
  value.seenEvents ??= {};
  return value;
}

/** Read, change and write the ledger under its lock. */
export async function withExtLedger<T>(
  worktree: string,
  taskId: string,
  fn: (ledger: ExtLedger) => T | Promise<T>,
): Promise<T> {
  const file = extLedgerPath(worktree, taskId);
  return await withFileLock(lockPathFor(file), async () => {
    const ledger = readExtLedger(worktree, taskId);
    const value = await fn(ledger);
    Deno.mkdirSync(path.dirname(file), { recursive: true });
    const tmp = `${file}.${Deno.pid}.tmp`;
    Deno.writeTextFileSync(tmp, `${JSON.stringify(ledger, null, 2)}\n`);
    Deno.renameSync(tmp, file);
    return value;
  });
}

// ---------------------------------------------------------------------------
// keyed operations
// ---------------------------------------------------------------------------

export type OpStart =
  | { status: "started"; key: string; jobId: string; epoch: number }
  | { status: "already-done"; key: string; jobId: string };

/**
 * Start a keyed external operation (an intent job). A key that succeeded is
 * not performed again; a key whose operation has no outcome yet (running, or
 * awaiting approval after its agent was lost) is refused; a key that failed
 * may be started again.
 */
export async function startExtOp(
  state: FdhTaskState,
  key: string,
  agent: string,
  op: IntentOp = "external",
  target = key,
): Promise<OpStart> {
  if (key.trim() === "") throw new Error("--key must not be empty");
  return await withExtLedger(state.worktree, state.taskId, async (ledger) => {
    const existing = ledger.ops[key];
    if (existing !== undefined) {
      if (existing.outcome === "succeeded") {
        return {
          status: "already-done" as const,
          key,
          jobId: existing.jobId,
        };
      }
      if (existing.outcome === null) {
        const job = readJob(state.worktree, state.taskId, existing.jobId);
        if (job.state === "succeeded") {
          existing.outcome = "succeeded";
          return {
            status: "already-done" as const,
            key,
            jobId: existing.jobId,
          };
        }
        if (!isTerminal(job.state)) {
          throw new Error(
            `operation ${key} is in flight (job ${job.jobId} is ${job.state}); confirm it, or resolve it with 'fdh job resolve'`,
          );
        }
      }
    }
    const job = await startIntent(state, op, `${target} [key ${key}]`, agent);
    ledger.ops[key] = {
      key,
      jobId: job.jobId,
      epoch: job.epoch,
      startedAt: nowIso(),
      outcome: null,
    };
    return {
      status: "started" as const,
      key,
      jobId: job.jobId,
      epoch: job.epoch,
    };
  });
}

export interface OpDoneInput {
  key: string;
  epoch: number;
  agent: string;
  outcome: "succeeded" | "failed";
  /** For a planned sync action: record what fdh wrote. */
  sync?: {
    link: string;
    field: string;
    ownerValue: string;
    mirrorValue: string;
  };
}

/** Record the outcome of a keyed operation (fenced by the intent's epoch). */
export async function finishExtOp(
  state: FdhTaskState,
  input: OpDoneInput,
): Promise<ExtOp> {
  return await withExtLedger(state.worktree, state.taskId, async (ledger) => {
    const op = ledger.ops[input.key];
    if (op === undefined) {
      throw new Error(`no operation with key ${input.key}; start it first`);
    }
    const { job, value } = await confirmJob(
      state,
      op.jobId,
      input.epoch,
      input.agent,
      input.outcome,
    );
    if (!value.applied) {
      throw new Error(
        `operation ${input.key}: confirm not applied (${value.reason}); job ${job.jobId} is ${job.state}`,
      );
    }
    op.outcome = input.outcome;
    op.epoch = input.epoch;
    if (input.outcome === "succeeded" && input.sync !== undefined) {
      const link = ledger.links[input.sync.link];
      if (link === undefined) throw new Error(`no link ${input.sync.link}`);
      link.synced[input.sync.field] = {
        ownerValue: input.sync.ownerValue,
        mirrorValue: input.sync.mirrorValue,
        at: nowIso(),
      };
    }
    return op;
  });
}
