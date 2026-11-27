// SPDX-License-Identifier: MPL-2.0
import { parseAuditEvent } from "./fdh_audit_event.ts";
import { AuditStoreError } from "./fdh_audit_store.ts";
import type { AuditStore } from "./fdh_audit_store.ts";
import type { AuditDenial } from "./fdh_audit_types.ts";

export const AUDIT_PATH = "/v1/events";
export const AUDIT_SCOPE = "fdh:audit:write";
const MAX_JSON_BYTES = 4096;

export function auditResource(value: string): URL {
  const url = new URL(value);
  if (value !== url.origin + AUDIT_PATH || url.protocol !== "https:") {
    throw new Error("Invalid audit resource");
  }
  return url;
}

/** Bounds both HTTP bodies and CLI input files, including streaming input. */
export async function readAuditJson(
  stream: ReadableStream<Uint8Array> | null,
): Promise<unknown> {
  if (!stream) throw new Error("Missing audit input");
  const buffer = new Uint8Array(MAX_JSON_BYTES);
  const reader = stream.getReader();
  let offset = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      if (offset + value.byteLength > buffer.byteLength) {
        void reader.cancel().catch(() => {});
        throw new Error("Audit input exceeds limit");
      }
      buffer.set(value, offset);
      offset += value.byteLength;
    }
    return JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(
        buffer.subarray(0, offset),
      ),
    );
  } finally {
    reader.releaseLock();
  }
}

export class AuditAdmissionError extends Error {
  constructor(readonly code: "unauthenticated" | "unauthorized") {
    super("Audit authorization rejected");
  }
}

function failure(error: unknown): Response {
  const full =
    (error instanceof AuditStoreError && error.code === "capacity") ||
    (error instanceof Error && "errcode" in error && error.errcode === 13);
  // Server logs may be retained independently of ledger availability. Emit no IO errors.
  console.error(
    full
      ? "[fdh audit] ledger capacity exhausted"
      : "[fdh audit] ledger unavailable",
  );
  return reply(full ? 507 : 503, {
    error: full ? "capacity" : "audit-unavailable",
  });
}

function reply(
  status: number,
  body: unknown,
  headers?: Record<string, string>,
): Response {
  return Response.json(body, {
    status,
    headers: { "cache-control": "no-store", ...headers },
  });
}

export function createAuditHandler(
  resource: URL,
  authenticate: (token: string) => Promise<string>,
  store: Pick<AuditStore, "append" | "deny">,
): (request: Request) => Promise<Response> {
  const origin = resource.origin;
  function denied(reason: AuditDenial, actor?: string): Response {
    const status = {
      unauthenticated: 401,
      unauthorized: 403,
      "invalid-input": 400,
      conflict: 409,
    }[reason];
    try {
      store.deny(reason, actor);
    } catch (error) {
      return failure(error);
    }
    const headers = status === 401
      ? { "www-authenticate": "Bearer" }
      : undefined;
    return reply(status, { error: reason }, headers);
  }

  return async (request) => {
    const path = new URL(request.url).pathname;
    if (path !== AUDIT_PATH || /[?#]/.test(request.url)) {
      return reply(404, { error: "not-found" });
    }
    if (request.method !== "POST") {
      return reply(405, { error: "method-not-allowed" }, { allow: "POST" });
    }
    const requestOrigin = request.headers.get("origin");
    if (requestOrigin !== null && requestOrigin !== origin) {
      return denied("unauthorized");
    }
    const authorization = request.headers.get("authorization") ?? "";
    const token = /^Bearer ([A-Za-z0-9_.-]{1,16384})$/i.exec(authorization)
      ?.[1];
    if (!token) return denied("unauthenticated");

    let actor: string;
    try {
      actor = await authenticate(token);
    } catch (error) {
      if (error instanceof AuditAdmissionError) return denied(error.code);
      console.error("[fdh audit] authorization unavailable");
      return reply(503, { error: "authorization-unavailable" });
    }
    let event;
    try {
      if (
        request.headers.get("content-type")?.split(";")[0].trim()
          .toLowerCase() !== "application/json"
      ) {
        return denied("invalid-input", actor);
      }
      event = parseAuditEvent(await readAuditJson(request.body));
    } catch {
      return denied("invalid-input", actor);
    }
    try {
      return reply(200, store.append(actor, event));
    } catch (error) {
      if (error instanceof AuditStoreError && error.code === "conflict") {
        return denied("conflict", actor);
      }
      return failure(error);
    }
  };
}
