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
 *   field is held for an approver (`ext resolve`); if only the mirror
 *   changed, the edit is kept. After a write, the field waits for a poll made
 *   after the write was recorded (both times on the local clock).
 * - Operations are keyed. `ext start` of a planned action stores what it will
 *   write; whichever way the operation ends as succeeded (`ext done`, or an
 *   approver's `job resolve` after its agent was lost), that becomes the
 *   field's baseline. A key that succeeded is not performed again; one in
 *   flight is not started twice.
 *
 * The ledger is `<worktree>/_dist/fdh/ext/<task-id>.json`, updated under a
 * file lock. Lock order: ext ledger, then job journal, then task state.
 */

import * as path from "@std/path";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import {
  fdhDir,
  type FdhTaskState,
  nowIso,
  validateTaskId,
} from "./fdh_task.ts";
import {
  type IntentOp,
  isTerminal,
  jobJournalPath,
  newJobId,
  readJob,
} from "./fdh_executor.ts";
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
   * (a Jira issue someone already closed is not reopened by a PR event), and
   * does not write before it has seen the mirror's current value.
   */
  order?: readonly string[];
  /**
   * `idempotent`: writing the same value again is harmless, and each owner
   * change is a new operation (its key carries the owner's version).
   * `keyed-once`: the write must happen once per value (a comment); its key
   * is the value, and a succeeded key is never planned again.
   */
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
      "draft/open/merged move the Jira issue to 進行中/レビュー中/完了; closed changes nothing. Jira owns its status: a status a person set since fdh's last write is kept, and the status never moves back.",
  },
  {
    field: "ci.result",
    owner: "github",
    mirror: { system: "jira", field: "ci-comment" },
    derive: (value) =>
      /^[0-9a-f]{7,40}:(success|failure)$/.test(value) ? value : null,
    retry: "keyed-once",
    note:
      "`<head sha>:<success|failure>` (version: the check run's completion time): one fdh summary comment per head and result; pending is not posted.",
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

/** Fields an observation may name: owners, mirrors, and `object`. */
export const OBSERVABLE_FIELDS: ReadonlySet<string> = new Set([
  "object",
  ...FIELD_RULES.map((r) => r.field),
  ...FIELD_RULES.flatMap((r) => r.mirror === null ? [] : [r.mirror.field]),
]);

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

export function requireIso(value: string, name: string): number {
  const at = Date.parse(value);
  if (!Number.isFinite(at)) {
    throw new Error(
      `${name} must be an ISO time, got ${JSON.stringify(value)}`,
    );
  }
  return at;
}

export interface Observation {
  value: string | null;
  /** The object's update time from the API (ISO); orders observations. */
  version: string;
  /** Local time of the latest poll that returned this value. */
  observedAt: string;
  deleted: boolean;
}

export interface SyncedField {
  ownerValue: string;
  mirrorValue: string;
  /** Local time the write was recorded; a later poll can show it. */
  at: string;
}

export interface ExtLink {
  /** The Jira issue key is the link id. */
  jira: string;
  pr: number | null;
  branch: string | null;
  synced: Record<string, SyncedField>;
}

/** What a planned operation writes (stored at `ext start`). */
export interface OpSync {
  link: string;
  field: string;
  ownerValue: string;
  mirrorValue: string;
}

export interface ExtOp {
  key: string;
  jobId: string;
  epoch: number;
  startedAt: string;
  outcome: "succeeded" | "failed" | null;
  sync: OpSync | null;
}

export interface ExtEvent {
  at: string;
  kind:
    | "duplicate"
    | "stale"
    | "conflict"
    | "deleted"
    | "resolved"
    | "op-healed";
  ref: string;
  field: string;
  note: string;
}

export interface ExtLedger {
  version: 1;
  taskId: string;
  links: Record<string, ExtLink>;
  observations: Record<string, Record<string, Observation>>;
  /** `<field>\0<event id>` already taken, per ref (bounded). */
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
        `${jira} is already linked to PR #${link.pr}; a link's PR does not change (use another task for a replacement PR)`,
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

/** Validate an observation's ref, field and version (before any lock). */
export function validateObservation(input: ObservationInput): void {
  parseRef(input.ref);
  if (!OBSERVABLE_FIELDS.has(input.field)) {
    throw new Error(
      `unknown field ${JSON.stringify(input.field)}: one of ${
        [...OBSERVABLE_FIELDS].join(", ")
      }`,
    );
  }
  requireIso(input.version, "version");
  if (input.deleted === true && input.value !== null) {
    throw new Error("a deletion has no value");
  }
  if (input.deleted !== true && input.value === null) {
    throw new Error("an observation needs a value unless it is a deletion");
  }
}

/**
 * Take one polled value. Ordering is by `version` (the API's update time):
 * an older version than the stored one is stale and ignored, a repeat of the
 * stored value or of a seen event id is a duplicate (it refreshes the poll
 * time), and the same version with another value is a conflict (kept out,
 * recorded). A deletion of `object` older than the newest observation of the
 * object is stale; a deletion is never undone by a later observation.
 */
export function ingestObservation(
  ledger: ExtLedger,
  input: ObservationInput,
): IngestResult {
  validateObservation(input);
  const version = Date.parse(input.version);
  const at = input.at ?? nowIso();
  const seen = ledger.seenEvents[input.ref] ??= [];
  const eventKey = input.eventId === undefined
    ? undefined
    : `${input.field}\0${input.eventId}`;
  const fields = ledger.observations[input.ref] ??= {};
  const current = fields[input.field];
  const record = (kind: ExtEvent["kind"], text: string) =>
    note(ledger, { at, kind, ref: input.ref, field: input.field, note: text });
  if (eventKey !== undefined && seen.includes(eventKey)) {
    record("duplicate", `event ${input.eventId} seen before`);
    return "duplicate";
  }
  const remember = () => {
    if (eventKey === undefined) return;
    seen.push(eventKey);
    if (seen.length > MAX_SEEN_EVENTS) {
      seen.splice(0, seen.length - MAX_SEEN_EVENTS);
    }
  };
  const deleted = input.deleted === true;
  const value = deleted ? null : input.value;
  if (deleted && input.field === "object") {
    const newest = Math.max(
      ...Object.values(fields).map((o) => Date.parse(o.version)),
    );
    if (Number.isFinite(newest) && version < newest && !current?.deleted) {
      remember();
      record(
        "stale",
        `deletion at ${input.version} is older than an observation at ${
          new Date(newest).toISOString()
        }`,
      );
      return "stale";
    }
  }
  if (current !== undefined) {
    const stored = Date.parse(current.version);
    if (version < stored) {
      remember();
      record(
        "stale",
        `version ${input.version} is older than ${current.version}`,
      );
      return "stale";
    }
    if (version === stored) {
      remember();
      if (current.value === value && current.deleted === deleted) {
        current.observedAt = at;
        record("duplicate", `same value at ${input.version}`);
        return "duplicate";
      }
      record(
        "conflict",
        `${JSON.stringify(value)} and stored ${
          JSON.stringify(current.value)
        } share version ${input.version}`,
      );
      return "conflict";
    }
    if (current.deleted && !deleted) {
      // A deleted object does not come back: that would be a new object under
      // a reused id. Keep the deletion.
      remember();
      record(
        "conflict",
        `observed again after its deletion at ${current.version}`,
      );
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
    record("deleted", `deleted at ${input.version}`);
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
  /** Operation key for `ext start` (the same change has the same key). */
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
    | "mirror-unobserved"
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

/** The key of an operation that writes `to` for `rule` on `link`. */
export function opKey(
  rule: FieldRule,
  link: string,
  to: string,
  ownerVersion: string,
): string {
  return rule.retry === "keyed-once"
    ? `${link}:${rule.field}:${to}`
    : `${link}:${rule.field}:${to}@${ownerVersion}`;
}

/**
 * What the agent should change, hold or leave, from the latest observations
 * and operations. Pure: the ledger is not changed (`refreshOps` first, so
 * operations ended by `job resolve` count).
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
      const hold = (reason: SyncHold["reason"], text: string) =>
        plan.holds.push({
          link: link.jira,
          field: rule.field,
          reason,
          note: text,
        });
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
        hold("deleted", `${deletedSide} was deleted`);
        continue;
      }
      if (owner.value === null) continue;
      const desired = rule.derive ? rule.derive(owner.value) : owner.value;
      if (desired === null) continue;
      const key = opKey(rule, link.jira, desired, owner.version);
      const op = ledger.ops[key];
      if (op !== undefined && op.outcome === null) {
        hold(
          "op-in-flight",
          `operation ${key} (job ${op.jobId}) has no outcome yet`,
        );
        continue;
      }
      if (rule.retry === "keyed-once") {
        // Written once per value (a comment): the key decides, not a baseline
        // (the mirror field shows only the latest, if it is polled at all).
        if (op?.outcome === "succeeded" || mirror?.value === desired) {
          plan.inSync.push({ link: link.jira, field: rule.field });
        } else {
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
        continue;
      }
      const synced = link.synced[rule.field];
      if (
        synced !== undefined &&
        (mirror === undefined ||
          Date.parse(mirror.observedAt) <= Date.parse(synced.at))
      ) {
        // fdh wrote the mirror after its last poll: wait for a poll that can
        // show the write before deciding anything.
        hold(
          "awaiting-poll",
          `no poll of ${mirrorRef} ${rule.mirror.field} since fdh wrote it at ${synced.at}`,
        );
        continue;
      }
      if (mirror?.value === desired) {
        plan.inSync.push({ link: link.jira, field: rule.field });
        continue;
      }
      if (rule.order !== undefined) {
        if (mirror === undefined || mirror.value === null) {
          hold(
            "mirror-unobserved",
            `poll ${mirrorRef} ${rule.mirror.field} before fdh may change it`,
          );
          continue;
        }
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
      if (synced !== undefined) {
        const mirrorEdited = mirror?.value !== synced.mirrorValue;
        const ownerChanged = owner.value !== synced.ownerValue;
        if (mirrorEdited && ownerChanged) {
          hold(
            "concurrent-edit",
            `${mirrorRef} ${rule.mirror.field} became ${
              JSON.stringify(mirror?.value ?? null)
            } and ${ownerRef} ${rule.field} became ${
              JSON.stringify(owner.value)
            } since fdh wrote ${
              JSON.stringify(synced.mirrorValue)
            }; settle it with 'fdh ext resolve'`,
          );
          continue;
        }
        if (mirrorEdited) {
          plan.keptEdits.push({
            link: link.jira,
            field: rule.field,
            value: mirror?.value ?? null,
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
  return path.join(fdhDir(worktree), "ext", `${validateTaskId(taskId)}.json`);
}

function isRecordOf(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

export function readExtLedger(worktree: string, taskId: string): ExtLedger {
  const file = extLedgerPath(worktree, taskId);
  let text: string;
  try {
    text = Deno.readTextFileSync(file);
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return emptyLedger(taskId);
    throw error;
  }
  let value: unknown;
  try {
    value = JSON.parse(text);
  } catch (error) {
    throw new Error(
      `invalid fdh ext ledger ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
  const ok = isRecordOf(value) && value.version === 1 &&
    value.taskId === taskId && isRecordOf(value.links) &&
    Object.values(value.links).every((l) =>
      isRecordOf(l) && typeof l.jira === "string" && isRecordOf(l.synced)
    ) &&
    isRecordOf(value.observations) &&
    Object.values(value.observations).every(isRecordOf) &&
    isRecordOf(value.ops) && Object.values(value.ops).every(isRecordOf) &&
    (value.seenEvents === undefined || isRecordOf(value.seenEvents)) &&
    Array.isArray(value.events);
  if (!ok) throw new Error(`invalid fdh ext ledger ${file}`);
  const ledger = value as unknown as ExtLedger;
  ledger.seenEvents ??= {};
  return ledger;
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
    writeLedger(file, ledger);
    return value;
  });
}

function writeLedger(file: string, ledger: ExtLedger): void {
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.${Deno.pid}.tmp`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(ledger, null, 2)}\n`);
  Deno.renameSync(tmp, file);
}

// ---------------------------------------------------------------------------
// keyed operations
// ---------------------------------------------------------------------------

function recordSucceeded(ledger: ExtLedger, op: ExtOp, at: string): void {
  op.outcome = "succeeded";
  if (op.sync === null) return;
  const link = ledger.links[op.sync.link];
  if (link === undefined) return;
  link.synced[op.sync.field] = {
    ownerValue: op.sync.ownerValue,
    mirrorValue: op.sync.mirrorValue,
    at,
  };
}

/**
 * Bring operations without an outcome up to date with their intent jobs: a
 * job that succeeded (confirmed, or resolved by an approver) records the
 * operation's write as the field's baseline; one that failed or was
 * cancelled ends it as failed. Returns the keys that changed.
 */
export function refreshOps(
  ledger: ExtLedger,
  worktree: string,
  at = nowIso(),
): string[] {
  const changed: string[] = [];
  for (const op of Object.values(ledger.ops)) {
    if (op.outcome !== null) continue;
    let state: string;
    try {
      Deno.statSync(jobJournalPath(worktree, ledger.taskId, op.jobId));
      state = readJob(worktree, ledger.taskId, op.jobId).state;
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
      // Recorded, but its intent was never written (crash in between):
      // nothing was handed to an agent, so it ends as failed.
      state = "failed";
    }
    if (state === "succeeded") recordSucceeded(ledger, op, at);
    else if (isTerminal(state as Parameters<typeof isTerminal>[0])) {
      op.outcome = "failed";
    } else continue;
    changed.push(op.key);
    note(ledger, {
      at,
      kind: "op-healed",
      ref: op.sync === null ? "" : `jira:${op.sync.link}`,
      field: op.sync?.field ?? "",
      note:
        `operation ${op.key} ended ${op.outcome} (job ${op.jobId} ${state})`,
    });
  }
  return changed;
}

/** `ext plan`: refresh operations from their jobs, then plan. */
export async function planExt(
  worktree: string,
  taskId: string,
): Promise<SyncPlan> {
  return await withExtLedger(worktree, taskId, (ledger) => {
    refreshOps(ledger, worktree);
    return planSync(ledger);
  });
}

export type OpStart =
  | { status: "started"; key: string; jobId: string; epoch: number }
  | { status: "already-done"; key: string; jobId: string };

/**
 * Start a keyed external operation (an intent job). A key of a planned sync
 * action stores what it will write. A key that succeeded is not performed
 * again; a key whose operation has no outcome yet (running, or awaiting
 * approval after its agent was lost) is refused; a key that failed may be
 * started again. The operation is recorded before its intent is written, so
 * a crash cannot leave an intent that no key knows.
 */
export async function startExtOp(
  state: FdhTaskState,
  key: string,
  agent: string,
  op: IntentOp = "external",
  target = key,
): Promise<OpStart> {
  if (key.trim() === "") throw new Error("--key must not be empty");
  const file = extLedgerPath(state.worktree, state.taskId);
  return await withFileLock(lockPathFor(file), async () => {
    const ledger = readExtLedger(state.worktree, state.taskId);
    refreshOps(ledger, state.worktree);
    const existing = ledger.ops[key];
    if (existing?.outcome === "succeeded") {
      writeLedger(file, ledger);
      return { status: "already-done" as const, key, jobId: existing.jobId };
    }
    if (existing !== undefined && existing.outcome === null) {
      writeLedger(file, ledger);
      throw new Error(
        `operation ${key} is in flight (job ${existing.jobId}); confirm it with 'fdh ext done', or resolve it with 'fdh job resolve'`,
      );
    }
    const action = planSync(ledger).actions.find((a) => a.key === key);
    const jobId = newJobId("intent");
    ledger.ops[key] = {
      key,
      jobId,
      epoch: 1,
      startedAt: nowIso(),
      outcome: null,
      sync: action === undefined ? null : {
        link: action.link,
        field: action.field,
        ownerValue: action.ownerValue,
        mirrorValue: action.to,
      },
    };
    writeLedger(file, ledger);
    const job = await startIntent(
      state,
      op,
      `${target} [key ${key}]`,
      agent,
      undefined,
      jobId,
    );
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
}

/**
 * Record the outcome of a keyed operation (fenced by the intent's epoch). A
 * repeat after a crash between the confirm and the ledger write records the
 * job's outcome again instead of failing.
 */
export async function finishExtOp(
  state: FdhTaskState,
  input: OpDoneInput,
): Promise<ExtOp> {
  return await withExtLedger(state.worktree, state.taskId, async (ledger) => {
    // An approver may have resolved it already (its agent was lost).
    refreshOps(ledger, state.worktree);
    const op = ledger.ops[input.key];
    if (op === undefined) {
      throw new Error(`no operation with key ${input.key}; start it first`);
    }
    if (op.outcome !== null) {
      if (op.outcome === input.outcome) return op;
      throw new Error(
        `operation ${input.key} already ended ${op.outcome}`,
      );
    }
    const { job, value } = await confirmJob(
      state,
      op.jobId,
      input.epoch,
      input.agent,
      input.outcome,
    );
    const repeated = !value.applied && value.reason === "already-applied" &&
      job.state === input.outcome;
    if (!value.applied && !repeated) {
      throw new Error(
        `operation ${input.key}: confirm not applied (${value.reason}); job ${job.jobId} is ${job.state}`,
      );
    }
    op.epoch = input.epoch;
    if (input.outcome === "succeeded") recordSucceeded(ledger, op, nowIso());
    else op.outcome = "failed";
    return op;
  });
}

/**
 * An approver settles a held field (`concurrent-edit`): `mirror` keeps the
 * mirror's current value as the new baseline (fdh writes nothing), `owner`
 * drops the baseline so the plan writes the owner's value again.
 */
export function resolveField(
  ledger: ExtLedger,
  linkId: string,
  field: string,
  keep: "mirror" | "owner",
  at = nowIso(),
): void {
  const link = ledger.links[linkId];
  if (link === undefined) throw new Error(`no link ${linkId}`);
  const rule = FIELD_RULES.find((r) => r.field === field && r.mirror !== null);
  if (rule === undefined || rule.mirror === null) {
    throw new Error(`${field} is not a synced field`);
  }
  if (keep === "owner") {
    delete link.synced[field];
  } else {
    const ownerRef = refFor(link, rule.owner);
    const mirrorRef = refFor(link, rule.mirror.system);
    const owner = ownerRef === null
      ? undefined
      : ledger.observations[ownerRef]?.[field];
    const mirror = mirrorRef === null
      ? undefined
      : ledger.observations[mirrorRef]?.[rule.mirror.field];
    if (owner?.value == null || mirror?.value == null) {
      throw new Error(
        `${field} of ${linkId} has not been observed on both sides`,
      );
    }
    // The baseline is as of the mirror's last poll: no write is pending.
    link.synced[field] = {
      ownerValue: owner.value,
      mirrorValue: mirror.value,
      at: new Date(Date.parse(mirror.observedAt) - 1).toISOString(),
    };
  }
  note(ledger, {
    at,
    kind: "resolved",
    ref: `jira:${linkId}`,
    field,
    note: `kept the ${keep} side`,
  });
}
