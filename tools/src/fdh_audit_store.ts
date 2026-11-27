// SPDX-License-Identifier: MPL-2.0
import { createHmac, timingSafeEqual } from "node:crypto";
import { backup as sqliteBackup, DatabaseSync } from "node:sqlite";
import {
  isAuditDigest,
  isAuditId,
  parseAuditEvent,
} from "./fdh_audit_event.ts";
import type {
  AuditCheckpoint,
  AuditDenial,
  AuditEvent,
  AuditRecord,
} from "./fdh_audit_types.ts";

const schema = `
  CREATE TABLE ledger (id TEXT PRIMARY KEY, mac TEXT NOT NULL) STRICT;
  CREATE TABLE events (
    sequence INTEGER PRIMARY KEY CHECK(sequence > 0),
    id TEXT NOT NULL UNIQUE,
    trace_id TEXT,
    intent_id TEXT UNIQUE,
    received_at TEXT NOT NULL,
    actor TEXT,
    payload TEXT NOT NULL,
    mac TEXT NOT NULL
  ) STRICT;
  CREATE INDEX events_by_trace ON events(trace_id, sequence);
  CREATE TRIGGER events_no_replace BEFORE INSERT ON events
    WHEN EXISTS (
      SELECT 1 FROM events WHERE sequence = NEW.sequence OR id = NEW.id
        OR intent_id = NEW.intent_id
    )
    BEGIN SELECT RAISE(ABORT, 'audit rows are immutable'); END;
  CREATE TRIGGER events_no_update BEFORE UPDATE ON events
    BEGIN SELECT RAISE(ABORT, 'audit rows are immutable'); END;
  CREATE TRIGGER events_no_delete BEFORE DELETE ON events
    BEGIN SELECT RAISE(ABORT, 'audit rows are immutable'); END;
`;
const columns =
  "sequence, id, trace_id, intent_id, received_at, actor, payload, mac";

interface Row {
  sequence: number;
  id: string;
  trace_id: string | null;
  intent_id: string | null;
  received_at: string;
  actor: string | null;
  payload: string;
  mac: string;
}

interface LedgerState {
  head: AuditCheckpoint;
  // Committed operation rows since rotation plus every unresolved intent.
  operationSlots: number;
  pendingResults: number;
  denials: number;
}

interface StoreOptions {
  anchor?: AuditCheckpoint;
  maxDenials?: number;
  maxEntries?: number;
}

export class AuditStoreError extends Error {
  constructor(readonly code: "conflict" | "capacity" | "integrity") {
    super("Audit ledger " + code);
  }
}

function sameMac(a: string, b: string): boolean {
  return isAuditDigest(a) && isAuditDigest(b) &&
    timingSafeEqual(new TextEncoder().encode(a), new TextEncoder().encode(b));
}

function sign(key: Uint8Array, fields: unknown[]): string {
  return createHmac("sha256", key).update(JSON.stringify(fields)).digest("hex");
}

function validateKey(key: Uint8Array): void {
  if (key.byteLength !== 32) throw new Error("Invalid audit key");
}

function rowMac(
  key: Uint8Array,
  previous: string,
  row: Omit<Row, "mac">,
): string {
  return sign(key, [
    "fdh-audit-event-v1",
    previous,
    row.sequence,
    row.id,
    row.trace_id,
    row.intent_id,
    row.received_at,
    row.actor,
    row.payload,
  ]);
}

function createPrivateFile(path: string): void {
  Deno.openSync(path, { createNew: true, write: true, mode: 0o600 }).close();
}

export function initializeAuditStore(
  path: string,
  key: Uint8Array,
): AuditCheckpoint {
  validateKey(key);
  createPrivateFile(path);
  using db = new DatabaseSync(path);
  const id = crypto.randomUUID();
  const mac = sign(key, ["fdh-audit-genesis-v1", id]);
  db.exec("BEGIN IMMEDIATE");
  db.exec(schema);
  db.prepare("INSERT INTO ledger VALUES (?, ?)").run(id, mac);
  db.exec("COMMIT");
  return { sequence: 0, mac };
}

function genesis(db: DatabaseSync, key: Uint8Array): AuditCheckpoint {
  const rows = db.prepare("SELECT id, mac FROM ledger LIMIT 2").all();
  const row = rows[0];
  if (
    rows.length !== 1 || !isAuditId(row.id) || typeof row.mac !== "string" ||
    !sameMac(row.mac, sign(key, ["fdh-audit-genesis-v1", row.id]))
  ) {
    throw new AuditStoreError("integrity");
  }
  return { sequence: 0, mac: row.mac };
}

function advance(
  state: LedgerState,
  head: AuditCheckpoint,
  event: AuditRecord["event"],
): void {
  state.head = head;
  if ("rotation" in event) {
    state.denials = 0;
    state.operationSlots = state.pendingResults;
  } else if ("denial" in event) {
    state.denials++;
  } else if (event.outcome.kind === "intent") {
    state.pendingResults++;
    state.operationSlots += 2;
  } else if (event.outcome.kind === "result") {
    state.pendingResults--;
  } else {
    state.operationSlots++;
  }
}

function scan(
  db: DatabaseSync,
  key: Uint8Array,
  start: LedgerState,
  required = start.head,
): LedgerState {
  const state = { ...start };
  let found = state.head.sequence === required.sequence &&
    sameMac(state.head.mac, required.mac);
  const rows = db.prepare(
    `SELECT ${columns} FROM events WHERE sequence > ? ORDER BY sequence`,
  ).iterate(state.head.sequence);
  for (const value of rows) {
    const row = value as unknown as Row;
    if (
      row.sequence !== state.head.sequence + 1 ||
      !sameMac(row.mac, rowMac(key, state.head.mac, row))
    ) {
      throw new AuditStoreError("integrity");
    }
    advance(
      state,
      { sequence: row.sequence, mac: row.mac },
      JSON.parse(row.payload),
    );
    if (row.sequence === required.sequence) {
      found = sameMac(row.mac, required.mac);
    }
  }
  if (!found) throw new AuditStoreError("integrity");
  return state;
}

function readState(
  db: DatabaseSync,
  key: Uint8Array,
  anchor?: AuditCheckpoint,
): LedgerState {
  return scan(db, key, {
    head: genesis(db, key),
    operationSlots: 0,
    pendingResults: 0,
    denials: 0,
  }, anchor);
}

function withReadTransaction<T>(db: DatabaseSync, read: () => T): T {
  db.exec("BEGIN");
  try {
    return read();
  } finally {
    db.exec("ROLLBACK");
  }
}

export function inspectAuditStore(
  path: string,
  key: Uint8Array,
  options: { anchor?: AuditCheckpoint; traceId?: string } = {},
): { head: AuditCheckpoint; records?: AuditRecord[] } {
  validateKey(key);
  using db = new DatabaseSync(path, { readOnly: true });
  return withReadTransaction(db, () => {
    const { head } = readState(db, key, options.anchor);
    if (!options.traceId) return { head };
    const rows = db.prepare(
      `SELECT ${columns} FROM events WHERE trace_id = ? ORDER BY sequence`,
    ).iterate(options.traceId);
    const records: AuditRecord[] = [];
    for (const value of rows) {
      const row = value as unknown as Row;
      records.push({
        sequence: row.sequence,
        mac: row.mac,
        receivedAt: row.received_at,
        actor: row.actor,
        event: JSON.parse(row.payload),
      });
    }
    return { head, records };
  });
}

export async function backupAuditStore(
  path: string,
  destination: string,
  key: Uint8Array,
): Promise<AuditCheckpoint> {
  const { head: anchor } = inspectAuditStore(path, key);
  createPrivateFile(destination);
  using db = new DatabaseSync(path, { readOnly: true });
  await sqliteBackup(db, destination);
  return inspectAuditStore(destination, key, { anchor }).head;
}

export class AuditStore {
  #db: DatabaseSync;
  #key: Uint8Array;
  #state: LedgerState;
  #maxEntries: number;
  #maxDenials: number;

  constructor(
    path: string,
    key: Uint8Array,
    { maxEntries = 100_000, maxDenials = 1_000, anchor }: StoreOptions = {},
  ) {
    validateKey(key);
    if (
      !Number.isSafeInteger(maxEntries) || maxEntries < 1 ||
      maxEntries > 1_000_000 ||
      !Number.isSafeInteger(maxDenials) || maxDenials < 0 ||
      maxDenials > 1_000_000
    ) {
      throw new Error("Invalid audit capacity");
    }
    // Require explicit initialization. A missing ledger must not silently reset.
    this.#key = key.slice();
    this.#maxEntries = maxEntries;
    this.#maxDenials = maxDenials;
    using readOnly = new DatabaseSync(path, { readOnly: true });
    this.#state = withReadTransaction(
      readOnly,
      () => readState(readOnly, this.#key, anchor),
    );
    this.#db = new DatabaseSync(path);
    try {
      this.#db.exec("PRAGMA busy_timeout=5000; PRAGMA synchronous=FULL");
    } catch (error) {
      this.#db.close();
      throw error;
    }
  }

  [Symbol.dispose](): void {
    this.#db.close();
  }

  append(actor: string, input: AuditEvent): AuditCheckpoint {
    if (!/^[A-Za-z0-9_-]{1,255}$/.test(actor)) {
      throw new Error("Invalid audit actor");
    }
    const event = parseAuditEvent(input);
    return this.#transaction((state) =>
      this.#appendOperation(state, actor, event)
    );
  }

  deny(reason: AuditDenial, actor: string | null = null): void {
    this.#transaction((state) => {
      if (state.denials < this.#maxDenials) {
        this.#write(state, actor, { id: crypto.randomUUID(), denial: reason });
      }
    });
  }

  rotate(expectedHead: AuditCheckpoint): AuditCheckpoint {
    return this.#transaction((state) => {
      if (
        state.head.sequence !== expectedHead.sequence ||
        !sameMac(state.head.mac, expectedHead.mac)
      ) throw new AuditStoreError("integrity");
      return this.#write(state, null, {
        id: crypto.randomUUID(),
        rotation: true,
      });
    });
  }

  #transaction<T>(write: (state: LedgerState) => T): T {
    const db = this.#db;
    db.exec("BEGIN IMMEDIATE");
    try {
      const previous = this.#state.head;
      const stored = previous.sequence === 0
        ? genesis(db, this.#key)
        : db.prepare("SELECT mac FROM events WHERE sequence = ?")
          .get(previous.sequence);
      if (
        typeof stored?.mac !== "string" || !sameMac(stored.mac, previous.mac)
      ) {
        throw new AuditStoreError("integrity");
      }
      const state = scan(db, this.#key, this.#state);
      const result = write(state);
      db.exec("COMMIT");
      this.#state = state;
      return result;
    } catch (error) {
      try {
        db.exec("ROLLBACK");
      } catch {
        // SQLite may have rolled back the entire transaction itself (e.g. FULL).
      }
      throw error;
    }
  }

  #appendOperation(
    state: LedgerState,
    actor: string,
    event: AuditEvent,
  ): AuditCheckpoint {
    const db = this.#db;
    const duplicate = db.prepare(`SELECT ${columns} FROM events WHERE id = ?`)
      .get(event.id) as unknown as Row | undefined;
    if (duplicate) {
      if (
        duplicate.actor !== actor || duplicate.payload !== JSON.stringify(event)
      ) {
        throw new AuditStoreError("conflict");
      }
      return { sequence: duplicate.sequence, mac: duplicate.mac };
    }
    if (event.outcome.kind === "result") {
      // The intent already reserved this slot, even across restarts or rotation.
      const { intentId } = event.outcome;
      const intent = db.prepare(`SELECT ${columns} FROM events WHERE id = ?`)
        .get(intentId) as unknown as Row | undefined;
      if (!intent || intent.actor !== actor || intent.trace_id === null) {
        throw new AuditStoreError("conflict");
      }
      const intentEvent = parseAuditEvent(JSON.parse(intent.payload));
      if (
        intentEvent.outcome.kind !== "intent" ||
        intentEvent.traceId !== event.traceId ||
        intentEvent.operation !== event.operation ||
        JSON.stringify(intentEvent.target) !== JSON.stringify(event.target) ||
        db.prepare("SELECT 1 FROM events WHERE intent_id = ?").get(intentId)
      ) {
        throw new AuditStoreError("conflict");
      }
    } else {
      const slots = event.outcome.kind === "intent" ? 2 : 1;
      if (state.operationSlots + slots > this.#maxEntries) {
        throw new AuditStoreError("capacity");
      }
    }
    return this.#write(state, actor, event);
  }

  #write(
    state: LedgerState,
    actor: string | null,
    event: AuditRecord["event"],
  ): AuditCheckpoint {
    const db = this.#db;
    const row: Omit<Row, "mac"> = {
      sequence: state.head.sequence + 1,
      id: event.id,
      trace_id: "traceId" in event ? event.traceId : null,
      intent_id: "outcome" in event && event.outcome.kind === "result"
        ? event.outcome.intentId
        : null,
      received_at: new Date().toISOString(),
      actor,
      payload: JSON.stringify(event),
    };
    const mac = rowMac(this.#key, state.head.mac, row);
    db.prepare(
      `INSERT INTO events (${columns}) VALUES (?, ?, ?, ?, ?, ?, ?, ?)`,
    )
      .run(
        row.sequence,
        row.id,
        row.trace_id,
        row.intent_id,
        row.received_at,
        row.actor,
        row.payload,
        mac,
      );
    const head = Object.freeze({ sequence: row.sequence, mac });
    advance(state, head, event);
    return head;
  }
}
