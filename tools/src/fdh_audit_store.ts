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

function scan(
  db: DatabaseSync,
  key: Uint8Array,
  start: AuditCheckpoint,
  required = start,
): AuditCheckpoint {
  let head = start;
  let found = start.sequence === required.sequence &&
    sameMac(start.mac, required.mac);
  const rows = db.prepare(
    `SELECT ${columns} FROM events WHERE sequence > ? ORDER BY sequence`,
  ).iterate(start.sequence);
  for (const value of rows) {
    const row = value as unknown as Row;
    if (
      row.sequence !== head.sequence + 1 ||
      !sameMac(row.mac, rowMac(key, head.mac, row))
    ) {
      throw new AuditStoreError("integrity");
    }
    head = { sequence: row.sequence, mac: row.mac };
    if (head.sequence === required.sequence) {
      found = sameMac(head.mac, required.mac);
    }
  }
  if (!found) throw new AuditStoreError("integrity");
  return head;
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
    const head = scan(db, key, genesis(db, key), options.anchor);
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
  #head: AuditCheckpoint;

  constructor(
    path: string,
    key: Uint8Array,
    private readonly maxEntries: number,
    anchor?: AuditCheckpoint,
  ) {
    validateKey(key);
    if (
      !Number.isSafeInteger(maxEntries) || maxEntries < 1 ||
      maxEntries > 1_000_000
    ) {
      throw new Error("Invalid audit capacity");
    }
    // Require explicit initialization. A missing ledger must not silently reset.
    this.#key = key.slice();
    this.#head = inspectAuditStore(path, this.#key, { anchor }).head;
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
    return this.#append(actor, parseAuditEvent(input));
  }

  deny(reason: AuditDenial, actor: string | null = null): AuditCheckpoint {
    return this.#append(actor, { id: crypto.randomUUID(), denial: reason });
  }

  #append(actor: string | null, event: AuditRecord["event"]): AuditCheckpoint {
    const db = this.#db;
    db.exec("BEGIN IMMEDIATE");
    try {
      const result = this.#insert(actor, event);
      db.exec("COMMIT");
      if (result.sequence > this.#head.sequence) this.#head = result;
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

  #insert(actor: string | null, event: AuditRecord["event"]): AuditCheckpoint {
    const db = this.#db;
    const previous = this.#head;
    const stored = previous.sequence === 0
      ? genesis(db, this.#key)
      : db.prepare(`SELECT ${columns} FROM events WHERE sequence = ?`)
        .get(previous.sequence) as unknown as Row | undefined;
    if (!stored || !sameMac(stored.mac, previous.mac)) {
      throw new AuditStoreError("integrity");
    }
    const head = scan(db, this.#key, previous);
    const payload = JSON.stringify(event);
    const duplicate = db.prepare(`SELECT ${columns} FROM events WHERE id = ?`)
      .get(event.id) as unknown as Row | undefined;
    if (duplicate) {
      if (duplicate.actor !== actor || duplicate.payload !== payload) {
        throw new AuditStoreError("conflict");
      }
      return { sequence: duplicate.sequence, mac: duplicate.mac };
    }
    if (head.sequence >= this.maxEntries) throw new AuditStoreError("capacity");
    const intentId = "outcome" in event && event.outcome.kind === "result"
      ? event.outcome.intentId
      : null;
    if (intentId !== null) {
      const intent = db.prepare(`SELECT ${columns} FROM events WHERE id = ?`)
        .get(intentId) as unknown as Row | undefined;
      if (!("outcome" in event) || !intent || intent.actor !== actor) {
        throw new AuditStoreError("conflict");
      }
      const rawIntent = JSON.parse(intent.payload);
      if ("denial" in rawIntent) throw new AuditStoreError("conflict");
      const intentEvent = parseAuditEvent(rawIntent);
      if (
        intentEvent.outcome.kind !== "intent" ||
        intentEvent.traceId !== event.traceId ||
        intentEvent.operation !== event.operation ||
        JSON.stringify(intentEvent.target) !== JSON.stringify(event.target) ||
        db.prepare("SELECT 1 FROM events WHERE intent_id = ?").get(intentId)
      ) {
        throw new AuditStoreError("conflict");
      }
    }
    const row: Omit<Row, "mac"> = {
      sequence: head.sequence + 1,
      id: event.id,
      trace_id: "traceId" in event ? event.traceId : null,
      intent_id: intentId,
      received_at: new Date().toISOString(),
      actor,
      payload,
    };
    const mac = rowMac(this.#key, head.mac, row);
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
    // Publish this head in memory only after the COMMIT in #append succeeds.
    return { sequence: row.sequence, mac };
  }
}
