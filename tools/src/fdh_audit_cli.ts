// SPDX-License-Identifier: MPL-2.0
import { decodeHex } from "@std/encoding/hex";
import { createAuditAuthenticator } from "./fdh_audit_auth.ts";
import {
  isAuditDigest,
  isAuditId,
  parseAuditEvent,
} from "./fdh_audit_event.ts";
import {
  auditResource,
  createAuditHandler,
  readAuditJson,
} from "./fdh_audit_http.ts";
import {
  AuditStore,
  backupAuditStore,
  initializeAuditStore,
  inspectAuditStore,
} from "./fdh_audit_store.ts";
import type { AuditCheckpoint } from "./fdh_audit_types.ts";

const usage = `Usage: deno task fdh audit <command>

Client:
  append <event.json>                       Post an event using FDH_AUDIT_RESOURCE and FDH_AUDIT_ACCESS_TOKEN

Operator (database commands use FDH_AUDIT_HMAC_KEY):
  init <db>                                 Create a new ledger and print its genesis checkpoint
  serve <db> <checkpoint.json>              Listen on loopback for the TLS reverse proxy
  verify <db> [<checkpoint.json>]           Verify the ledger and print its head
  trace <db> <trace-uuid> [<checkpoint.json>] Verify the ledger and print one correlated trace
  backup <db> <new-backup-db>                Save an online snapshot and print its checkpoint`;

function required(name: string): string {
  const value = Deno.env.get(name);
  if (!value) throw new Error("Missing " + name);
  return value;
}

async function readJsonFile(path: string): Promise<unknown> {
  return await readAuditJson((await Deno.open(path, { read: true })).readable);
}

export function parseAuditCheckpoint(input: unknown): AuditCheckpoint {
  if (
    !input || typeof input !== "object" || !("sequence" in input) ||
    !("mac" in input) || Object.keys(input).length !== 2 ||
    typeof input.sequence !== "number" ||
    !Number.isSafeInteger(input.sequence) ||
    input.sequence < 0 || !isAuditDigest(input.mac)
  ) {
    throw new Error("Invalid audit checkpoint");
  }
  return { sequence: input.sequence, mac: input.mac };
}

async function append(file: string): Promise<AuditCheckpoint> {
  const event = parseAuditEvent(await readJsonFile(file));
  const resource = auditResource(required("FDH_AUDIT_RESOURCE"));
  const token = required("FDH_AUDIT_ACCESS_TOKEN");
  if (!/^[A-Za-z0-9_.-]{1,16384}$/.test(token)) {
    throw new Error("Invalid audit access token");
  }
  const response = await fetch(resource, {
    method: "POST",
    headers: {
      "content-type": "application/json",
      authorization: "Bearer " + token,
    },
    body: JSON.stringify(event),
    signal: AbortSignal.timeout(45000),
    redirect: "error",
  });
  if (!response.ok) {
    await response.body?.cancel();
    throw new Error("Audit append refused (HTTP " + response.status + ")");
  }
  return parseAuditCheckpoint(await readAuditJson(response.body));
}

async function serve(
  db: string,
  key: Uint8Array,
  anchor: AuditCheckpoint,
): Promise<void> {
  const resource = auditResource(required("FDH_AUDIT_RESOURCE"));
  const authenticate = createAuditAuthenticator({
    resource: resource.href,
    issuer: required("FDH_AUDIT_ISSUER"),
    googleClientId: required("FDH_AUDIT_GOOGLE_CLIENT_ID"),
    readerId: required("FDH_AUDIT_READER_CLIENT_ID"),
    readerSecret: required("FDH_AUDIT_READER_CLIENT_SECRET"),
  });
  const port = Number(Deno.env.get("FDH_AUDIT_PORT") ?? "18884");
  const max = Number(Deno.env.get("FDH_AUDIT_MAX_ENTRIES") ?? "100000");
  if (!Number.isInteger(port) || port < 1024 || port > 65535) {
    throw new Error("Invalid audit port");
  }
  using store = new AuditStore(db, key, max, anchor);
  const server = Deno.serve({
    hostname: "127.0.0.1",
    port,
    onListen: () =>
      console.error("[fdh audit] listening on loopback port " + port),
  }, createAuditHandler(resource, authenticate, store));
  await server.finished;
}

export async function runAuditCli(args: readonly string[]): Promise<void> {
  if (args.length === 0 || args.length === 1 && args[0] === "--help") {
    console.log(usage);
    return;
  }
  const [action, db, argument, traceAnchor] = args;
  if (
    !db ||
    args.slice(1).some((arg) => arg.length === 0 || arg.startsWith("--")) ||
    !(
      (action === "append" || action === "init") && args.length === 2 ||
      (action === "serve" || action === "backup") && args.length === 3 ||
      action === "verify" && (args.length === 2 || args.length === 3) ||
      action === "trace" && isAuditId(argument) &&
        (args.length === 3 || args.length === 4)
    )
  ) throw new Error("Invalid audit command. See fdh audit --help");

  // Suppress paths, raw upstream responses and runtime diagnostics from command errors.
  try {
    if (action === "append") {
      console.log(JSON.stringify(await append(db)));
      return;
    }
    const hexKey = required("FDH_AUDIT_HMAC_KEY");
    if (!isAuditDigest(hexKey)) throw new Error("Invalid audit HMAC key");
    const key = decodeHex(hexKey);
    if (action === "init") {
      console.log(JSON.stringify(initializeAuditStore(db, key)));
      return;
    }
    if (action === "backup") {
      console.log(JSON.stringify(await backupAuditStore(db, argument, key)));
      return;
    }
    const anchorPath = action === "trace" ? traceAnchor : argument;
    const anchor = anchorPath
      ? parseAuditCheckpoint(await readJsonFile(anchorPath))
      : undefined;
    if (action === "serve" && anchor) {
      await serve(db, key, anchor);
      return;
    }
    const { head, records } = inspectAuditStore(db, key, {
      anchor,
      traceId: action === "trace" ? argument : undefined,
    });
    console.log(JSON.stringify(action === "trace" ? { head, records } : head));
  } catch (error) {
    if (
      error instanceof Error &&
      /^Audit append refused \(HTTP [1-5][0-9]{2}\)$/.test(error.message)
    ) {
      throw new Error(error.message);
    }
    throw new Error("Audit " + action + " failed");
  }
}
