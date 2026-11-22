// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertThrows } from "@std/assert";
import { join } from "@std/path";
import { DatabaseSync } from "node:sqlite";
import { exportJWK, generateKeyPair, SignJWT } from "jose";
import { createAuditAuthenticator } from "./fdh_audit_auth.ts";
import {
  AUDIT_SCOPE,
  auditResource,
  createAuditHandler,
} from "./fdh_audit_http.ts";
import {
  AuditStore,
  AuditStoreError,
  backupAuditStore,
  initializeAuditStore,
  inspectAuditStore,
} from "./fdh_audit_store.ts";
import type { AuditEvent } from "./fdh_audit_types.ts";

const actor = "workspace-member";
const resource = auditResource("https://audit.example.test/v1/events");

function intent(): AuditEvent {
  return {
    id: crypto.randomUUID(),
    traceId: crypto.randomUUID(),
    operation: "review",
    target: {
      kind: "pull-request",
      repo: "Floorp",
      pr: 123,
      head: "a".repeat(40),
    },
    agentRunId: crypto.randomUUID(),
    toolRunId: crypto.randomUUID(),
    outcome: { kind: "intent" },
  };
}

Deno.test("FDH audit checks durable intents, receipts, replay, and correlated results", () => {
  const dir = Deno.makeTempDirSync();
  try {
    const path = join(dir, "audit.sqlite3");
    const key = crypto.getRandomValues(new Uint8Array(32));
    const anchor = initializeAuditStore(path, key);
    const first = intent();
    const receipt: AuditEvent = {
      ...first,
      id: crypto.randomUUID(),
      runnerRunId: crypto.randomUUID(),
      outcome: {
        kind: "receipt",
        artifactSha256: "b".repeat(64),
        reviewVerdict: "request_changes",
      },
    };
    const result: AuditEvent = {
      ...first,
      id: crypto.randomUUID(),
      outcome: { kind: "result", intentId: first.id, status: "denied" },
    };
    {
      using left = new AuditStore(path, key, { anchor });
      using right = new AuditStore(path, key, { anchor });
      const committed = left.append(actor, first);
      assertEquals(right.append(actor, first), committed);
      assertThrows(
        () => right.append("different-member", first),
        AuditStoreError,
        "conflict",
      );
      assertThrows(
        () => right.append("different-member", result),
        AuditStoreError,
        "conflict",
      );
      assertThrows(
        () => right.append(actor, { ...result, traceId: crypto.randomUUID() }),
        AuditStoreError,
        "conflict",
      );
      assertEquals(right.append(actor, receipt).sequence, 2);
      assertEquals(left.append(actor, result).sequence, 3);
      assertThrows(
        () => right.append(actor, { ...result, id: crypto.randomUUID() }),
        AuditStoreError,
        "conflict",
      );
    }
    using restarted = new AuditStore(path, key, { anchor });
    assertEquals(restarted.append(actor, result).sequence, 3);
    const trace = inspectAuditStore(path, key, {
      anchor,
      traceId: first.traceId,
    });
    assertEquals(trace.head.sequence, 3);
    assertEquals(
      JSON.parse(JSON.stringify(trace.records?.map((record) => record.event))),
      [
        first,
        receipt,
        result,
      ],
    );
    assert(trace.records?.every((record) => record.actor === actor));
    assertThrows(
      () => initializeAuditStore(path, key),
      Deno.errors.AlreadyExists,
    );
    assertThrows(
      () => inspectAuditStore(path, new Uint8Array(32)),
      AuditStoreError,
      "integrity",
    );
  } finally {
    Deno.removeSync(dir, { recursive: true });
  }
});

Deno.test("FDH audit rejects SQL mutation and verifies snapshot restoration against an independent checkpoint", async () => {
  const dir = Deno.makeTempDirSync();
  try {
    const path = join(dir, "audit.sqlite3");
    const stale = join(dir, "stale.sqlite3");
    const backup = join(dir, "backup.sqlite3");
    const restored = join(dir, "restored.sqlite3");
    const key = crypto.getRandomValues(new Uint8Array(32));
    initializeAuditStore(path, key);
    using store = new AuditStore(path, key);
    const first = intent();
    store.append(actor, first);
    await backupAuditStore(path, stale, key);
    const latest = store.append(actor, intent());
    using sql = new DatabaseSync(path);
    for (
      const mutation of [
        "UPDATE events SET actor = 'other' WHERE sequence = 1",
        "DELETE FROM events WHERE sequence = 1",
        "INSERT OR REPLACE INTO events SELECT * FROM events WHERE sequence = 1",
      ]
    ) assertThrows(() => sql.exec(mutation), Error, "immutable");
    assertEquals(await backupAuditStore(path, backup, key), latest);
    Deno.copyFileSync(backup, restored);
    assertEquals(
      inspectAuditStore(restored, key, { anchor: latest }).head,
      latest,
    );
    assertThrows(
      () => inspectAuditStore(stale, key, { anchor: latest }),
      AuditStoreError,
      "integrity",
    );
    sql.exec(
      "DROP TRIGGER events_no_update; UPDATE events SET payload = '{}' WHERE sequence = 1",
    );
    assertThrows(
      () => inspectAuditStore(path, key, { anchor: latest }),
      AuditStoreError,
      "integrity",
    );
  } finally {
    Deno.removeSync(dir, { recursive: true });
  }
});

Deno.test("FDH audit HTTP fails closed on storage failure and capacity, including retry at capacity", async () => {
  const dir = Deno.makeTempDirSync();
  try {
    const path = join(dir, "audit.sqlite3");
    const key = crypto.getRandomValues(new Uint8Array(32));
    const anchor = initializeAuditStore(path, key);
    const options = { maxEntries: 2, maxDenials: 1 };
    using store = new AuditStore(path, key, options);
    using sql = new DatabaseSync(path);
    const request = (value: unknown) =>
      new Request(resource, {
        method: "POST",
        headers: {
          authorization: "Bearer test-token",
          "content-type": "application/json",
        },
        body: JSON.stringify(value),
      });
    const handle = createAuditHandler(
      resource,
      () => Promise.resolve(actor),
      store,
    );
    const first = intent();
    sql.exec(`
      CREATE TRIGGER injected_write_failure BEFORE INSERT ON events
      BEGIN SELECT RAISE(ABORT, 'sensitive fixture diagnostic'); END;
    `);
    const unavailable = await handle(request(first));
    assertEquals(unavailable.status, 503);
    assertEquals(await unavailable.json(), { error: "audit-unavailable" });
    assertEquals(inspectAuditStore(path, key).head, anchor);
    sql.exec("DROP TRIGGER injected_write_failure");
    const deny = () => handle(new Request(resource, { method: "POST" }));
    assertEquals((await deny()).status, 401);
    assertEquals((await deny()).status, 401);
    assertEquals(inspectAuditStore(path, key).head.sequence, 1);
    const accepted = await handle(request(first));
    assertEquals(accepted.status, 200);
    const replay = await handle(request(first));
    assertEquals(replay.status, 200);
    assertEquals(await accepted.json(), await replay.json());
    const receipt: AuditEvent = {
      ...intent(),
      outcome: {
        kind: "receipt",
        artifactSha256: "b".repeat(64),
        reviewVerdict: "approve",
      },
    };
    const full = await handle(request(receipt));
    assertEquals(full.status, 507);
    assertEquals(await full.json(), { error: "capacity" });
    assertEquals((await deny()).status, 401);
    assertEquals(inspectAuditStore(path, key).head.sequence, 2);
    using restarted = new AuditStore(path, key, options);
    const restartedHandle = createAuditHandler(
      resource,
      () => Promise.resolve(actor),
      restarted,
    );
    const result: AuditEvent = {
      ...first,
      id: crypto.randomUUID(),
      outcome: { kind: "result", intentId: first.id, status: "succeeded" },
    };
    assertEquals((await restartedHandle(request(result))).status, 200);
    assertEquals((await handle(request(result))).status, 200);
    assertEquals((await handle(request(intent()))).status, 507);
    assertEquals(inspectAuditStore(path, key).head.sequence, 3);
  } finally {
    Deno.removeSync(dir, { recursive: true });
  }
});

Deno.test("FDH audit rotates a verified snapshot without losing reservations or replay protection", async () => {
  const dir = Deno.makeTempDirSync();
  try {
    const path = join(dir, "audit.sqlite3");
    const next = join(dir, "next.sqlite3");
    const key = crypto.getRandomValues(new Uint8Array(32));
    const anchor = initializeAuditStore(path, key);
    const limits = { maxEntries: 2, maxDenials: 1 };
    const first = intent();
    let accepted;
    {
      using store = new AuditStore(path, key, limits);
      accepted = store.append(actor, first);
      store.deny("unauthenticated");
    }
    const previousHead = await backupAuditStore(path, next, key);
    using writer = new AuditStore(next, key, limits);
    using operator = new AuditStore(next, key);
    assertThrows(() => operator.rotate(accepted), AuditStoreError, "integrity");
    const rotated = operator.rotate(previousHead);
    assertEquals(rotated.sequence, previousHead.sequence + 1);
    assertEquals(inspectAuditStore(next, key, { anchor }).head, rotated);
    assertEquals(inspectAuditStore(path, key, { anchor }).head, previousHead);
    assertThrows(
      () => inspectAuditStore(path, key, { anchor: rotated }),
      AuditStoreError,
      "integrity",
    );

    assertEquals(writer.append(actor, first), accepted);
    assertThrows(
      () => writer.append("different-member", first),
      AuditStoreError,
      "conflict",
    );
    writer.deny("unauthenticated");
    writer.deny("unauthenticated");
    assertThrows(
      () => writer.append(actor, intent()),
      AuditStoreError,
      "capacity",
    );
    const result: AuditEvent = {
      ...first,
      id: crypto.randomUUID(),
      outcome: { kind: "result", intentId: first.id, status: "succeeded" },
    };
    assertEquals(writer.append(actor, result).sequence, rotated.sequence + 2);
    const receipt: AuditEvent = {
      ...intent(),
      outcome: {
        kind: "receipt",
        artifactSha256: "b".repeat(64),
        reviewVerdict: "approve",
      },
    };
    assertEquals(writer.append(actor, receipt).sequence, rotated.sequence + 3);
    assertThrows(
      () => writer.append(actor, { ...receipt, id: crypto.randomUUID() }),
      AuditStoreError,
      "capacity",
    );
    const trace = inspectAuditStore(next, key, {
      anchor: previousHead,
      traceId: first.traceId,
    });
    assertEquals(trace.records?.map(({ event }) => event), [first, result]);
  } finally {
    Deno.removeSync(dir, { recursive: true });
  }
});

Deno.test("FDH audit uses signed resource-scoped JWT plus live Workspace binding and persists only allowed fields", async () => {
  const dir = Deno.makeTempDirSync();
  try {
    const path = join(dir, "audit.sqlite3");
    const key = crypto.getRandomValues(new Uint8Array(32));
    initializeAuditStore(path, key);
    using store = new AuditStore(path, key);
    const issuer = "https://oidc.example.test/realms/fdh";
    const googleClientId = "fdh-test.apps.googleusercontent.com";
    const subject = "keycloak-subject";
    const expiry = Math.floor(Date.now() / 1000) + 120;
    let active = true;
    let brokerFailure: "introspection" | "admin" | undefined;
    const { publicKey, privateKey } = await generateKeyPair("RS256", {
      extractable: true,
    });
    const jwk = { ...await exportJWK(publicKey), kid: "fixture" };
    const endpoints = new Map<string, unknown>([
      ["/realms/fdh/protocol/openid-connect/certs", { keys: [jwk] }],
      ["/realms/fdh/protocol/openid-connect/token", {
        access_token: "fake-reader-secret",
        expires_in: 60,
        token_type: "Bearer",
      }],
      ["/admin/realms/fdh/identity-provider/instances/google", {
        alias: "google",
        providerId: "google",
        enabled: true,
        trustEmail: true,
        storeToken: false,
        config: {
          clientId: googleClientId,
          hostedDomain: "floorp.app",
          syncMode: "FORCE",
          validateSignature: "true",
          disableUserInfo: "true",
          filteredByClaim: "true",
          claimFilterName: "email_verified",
          claimFilterValue: "true",
        },
      }],
      ["/admin/realms/fdh/identity-provider/instances/google/mappers", []],
      ["/admin/realms/fdh/users/profile", {
        unmanagedAttributePolicy: "ADMIN_VIEW",
        attributes: [
          {
            name: "email",
            permissions: { view: ["admin", "user"], edit: ["admin"] },
          },
          { name: "username", permissions: { edit: ["admin"] } },
        ],
      }],
      ["/admin/realms/fdh/users/" + subject, {
        id: subject,
        email: "audit-test@floorp.app",
        emailVerified: true,
      }],
      ["/admin/realms/fdh/users/" + subject + "/federated-identity", [{
        identityProvider: "google",
        userId: actor,
      }]],
    ]);
    const authenticate = createAuditAuthenticator({
      issuer,
      resource: resource.href,
      googleClientId,
      readerId: "audit-reader",
      readerSecret: "synthetic-reader-credential",
    }, (input, options) => {
      const url = new URL(String(input));
      assertEquals(url.origin, "https://oidc.example.test");
      assertEquals(options?.redirect, "error");
      if (url.pathname.endsWith("/token/introspect")) {
        assertEquals(options?.method, "POST");
        if (brokerFailure === "introspection") {
          return Promise.reject(new Error("sensitive broker timeout"));
        }
        return Promise.resolve(Response.json({
          active,
          exp: expiry,
          iss: issuer,
          sub: subject,
          aud: [resource.href],
          scope: AUDIT_SCOPE,
          identity_provider: "google",
        }));
      }
      if (brokerFailure === "admin" && url.pathname.includes("/admin/")) {
        return Promise.resolve(
          new Response("sensitive broker error", {
            status: 502,
          }),
        );
      }
      const payload = endpoints.get(url.pathname);
      return Promise.resolve(
        payload === undefined
          ? new Response(null, { status: 404 })
          : Response.json(payload),
      );
    });
    const sign = (aud: string, scope: string, exp = expiry) =>
      new SignJWT({
        iss: issuer,
        sub: subject,
        aud,
        scope,
        exp,
        typ: "Bearer",
      }).setProtectedHeader({ alg: "RS256", kid: "fixture" }).sign(privateKey);
    const handle = createAuditHandler(resource, authenticate, store);
    const request = (token: string, value: unknown) =>
      new Request(resource, {
        method: "POST",
        headers: {
          authorization: "Bearer " + token,
          "content-type": "application/json",
        },
        body: JSON.stringify(value),
      });
    const event = intent();
    const token = await sign(resource.href, AUDIT_SCOPE);
    const accepted = await handle(request(token, event));
    assertEquals(accepted.status, 200);
    const acceptedHead = inspectAuditStore(path, key).head;
    for (const failure of ["introspection", "admin"] as const) {
      brokerFailure = failure;
      const unavailable = await handle(request(token, intent()));
      assertEquals(unavailable.status, 503);
      assertEquals(await unavailable.json(), {
        error: "authorization-unavailable",
      });
      assertEquals(inspectAuditStore(path, key).head, acceptedHead);
    }
    brokerFailure = undefined;
    assertEquals(
      (await handle(request(await sign(resource.href, "fdh:probe"), intent())))
        .status,
      403,
    );
    assertEquals(
      (await handle(
        request(
          await sign("https://elsewhere.test/mcp", AUDIT_SCOPE),
          intent(),
        ),
      )).status,
      401,
    );
    assertEquals(
      (await handle(
        request(await sign(resource.href, AUDIT_SCOPE, 1), intent()),
      )).status,
      401,
    );
    assertEquals(
      (await handle(request(token, { ...event, actor: "user-selected-actor" })))
        .status,
      400,
    );
    assertEquals(
      (await handle(
        request(token, { ...event, secret: "untrusted-payload-secret" }),
      )).status,
      400,
    );
    active = false;
    assertEquals((await handle(request(token, intent()))).status, 403);
    assertEquals(
      (await handle(new Request(resource, { method: "DELETE" }))).status,
      405,
    );
    const trace = inspectAuditStore(path, key, { traceId: event.traceId });
    assertEquals(trace.records?.map((record) => record.actor), [actor]);
    assertEquals(trace.head.sequence, 7);
    using sql = new DatabaseSync(path, { readOnly: true });
    const rows = JSON.stringify(
      sql.prepare("SELECT * FROM events ORDER BY sequence").all(),
    );
    for (
      const secret of [
        token,
        subject,
        "audit-test@floorp.app",
        "fake-reader-secret",
        "synthetic-reader-credential",
        "untrusted-payload-secret",
        "user-selected-actor",
      ]
    ) {
      assert(!rows.includes(secret));
    }
    assert(
      rows.includes("unauthorized") && rows.includes("unauthenticated") &&
        rows.includes("invalid-input"),
    );
  } finally {
    Deno.removeSync(dir, { recursive: true });
  }
});
