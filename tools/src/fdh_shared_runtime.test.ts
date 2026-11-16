// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertRejects } from "@std/assert";
import { createSharedRuntime } from "./fdh_shared_runtime.ts";
const settings = () => ({
  publicOrigin: "https://fdh.example.test",
  issuer: "https://as.example.test/auth/realms/fdh",
  gatewayLoginUrl: "https://fdh.example.test/gateway/login",
  expectedGoogleClientId: "fixture.apps.googleusercontent.com",
  readerClientId: "reader",
  readerSecretEnv: "FDH_READER_SECRET",
});
const unavailable = {
  mode: "unconfigured",
  productionQualified: false,
  protectedOperationsAllowed: false,
};
Deno.test("invalid config and non-Keycloak issuer reject before private IO", async () => {
  let calls = 0;
  const io = {
    backend: "keycloak-readonly" as const,
    lookupSecret: () => {
      calls++;
      return "fixture-secret";
    },
    fetch: () => {
      calls++;
      throw Error("PRIVATE");
    },
    now: () => 1000,
  };
  for (
    const issuer of [
      "http://as.example.test/realms/fdh",
      "https://as.example.test/realm/fdh",
      "https://as.example.test/realms/%66dh",
      "https://as.example.test/a//realms/fdh",
      "https://as.example.test/realms/..",
    ]
  ) {
    await assertRejects(
      () => createSharedRuntime({ ...settings(), issuer }, io),
      Error,
    );
  }
  assertEquals(calls, 0);
});
Deno.test("missing secret yields no hooks and only one named lookup", async () => {
  const names: string[] = [];
  const result = await createSharedRuntime(settings(), {
    backend: "keycloak-readonly",
    lookupSecret: (name) => {
      names.push(name);
      return undefined;
    },
    fetch: () => {
      throw Error("must not fetch");
    },
    now: () => 1000,
  });
  assertEquals(result, unavailable);
  assertEquals(names, ["FDH_READER_SECRET"]);
});

import { exportJWK, generateKeyPair, SignJWT } from "jose";
import type {
  SharedRuntimeFixtureCall,
  SharedRuntimePrivateIO,
} from "./fdh_shared_runtime_types.ts";
const issuer = settings().issuer;
const tokenUrl = issuer + "/protocol/openid-connect/token";
const admin = "https://as.example.test/auth/admin/realms/fdh";
const keys = await generateKeyPair("RS256");
const jwk = {
  ...await exportJWK(keys.publicKey),
  kid: "synthetic-key",
  alg: "RS256",
  use: "sig",
};
const access = {
  kind: "verified-shared-access-token" as const,
  issuer,
  subject: "realm-user",
  resourceAudience: settings().publicOrigin + "/mcp",
  expiresAt: 2000,
  scopes: ["fdh:probe"],
};
const json = (value: unknown, status = 200) =>
  new Response(JSON.stringify(value), {
    status,
    headers: { "content-type": "application/json" },
  });
function fixture() {
  const state = {
    now: 1000,
    grants: 0,
    lookups: [] as string[],
    calls: [] as SharedRuntimeFixtureCall[],
    active: true,
    expires: 100,
    subject: "realm-user",
    serviceAccount: false,
    intercept: undefined as
      | undefined
      | ((
        url: string,
        init: RequestInit,
      ) => Response | Promise<Response> | undefined),
  };
  const metadata = {
    issuer,
    token_endpoint: tokenUrl,
    jwks_uri: issuer + "/protocol/openid-connect/certs",
    introspection_endpoint: tokenUrl + "/introspect",
    authorization_endpoint: issuer + "/protocol/openid-connect/auth",
    revocation_endpoint: issuer + "/protocol/openid-connect/revoke",
  };
  const provider = {
    alias: "google",
    providerId: "google",
    enabled: true,
    trustEmail: true,
    storeToken: false,
    config: {
      syncMode: "FORCE",
      validateSignature: "true",
      disableUserInfo: "true",
      filteredByClaim: "true",
      claimFilterName: "email_verified",
      claimFilterValue: "true",
      disableNonce: "false",
      hostedDomain: "floorp.app",
      clientId: settings().expectedGoogleClientId,
    },
  };
  const io: SharedRuntimePrivateIO = {
    backend: "keycloak-readonly",
    lookupSecret: (name) => {
      state.lookups.push(name);
      return "PRIVATE_FIXTURE_SECRET";
    },
    now: () => state.now,
    fetch: async (input, init = {}) => {
      const url = String(input);
      state.calls.push({ url, method: init.method ?? "GET" });
      assertEquals(init.redirect, "error");
      assertEquals(init.credentials, "omit");
      assertEquals(init.cache, "no-store");
      const intercepted = await state.intercept?.(url, init);
      if (intercepted) return intercepted;
      if (url.endsWith("/.well-known/openid-configuration")) {
        return json(
          metadata,
        );
      }
      if (url === metadata.jwks_uri) return json({ keys: [jwk] });
      if (url === tokenUrl) {
        state.grants++;
        const form = new URLSearchParams(String(init.body));
        assertEquals(form.get("client_id"), "reader");
        assertEquals(form.get("client_secret"), "PRIVATE_FIXTURE_SECRET");
        assertEquals(form.get("grant_type"), "client_credentials");
        return json({
          access_token: "PRIVATE_READER_TOKEN",
          token_type: "Bearer",
          expires_in: state.expires,
        });
      }
      if (url === metadata.introspection_endpoint) {
        assertEquals(
          new URLSearchParams(String(init.body)).get("client_secret"),
          "PRIVATE_FIXTURE_SECRET",
        );
        return json({
          active: state.active,
          iss: issuer,
          sub: state.subject,
          aud: access.resourceAudience,
          scope: "fdh:probe",
          exp: 2000,
          identity_provider: "google",
        });
      }
      assertEquals(
        new Headers(init.headers).get("authorization"),
        "Bearer PRIVATE_READER_TOKEN",
      );
      if (url === admin + "/identity-provider/instances/google") {
        return json(
          provider,
        );
      }
      if (
        url === admin + "/identity-provider/instances/google/mappers"
      ) return json([]);
      if (url === admin + "/users/profile") {
        return json({
          unmanagedAttributePolicy: "DISABLED",
          attributes: [{
            name: "email",
            permissions: { view: ["admin", "user"], edit: ["admin"] },
          }, { name: "username", permissions: { edit: ["admin"] } }],
        });
      }
      if (url === admin + "/users/" + encodeURIComponent(state.subject)) {
        return json({
          id: state.subject,
          email: "fixture@floorp.app",
          emailVerified: true,
          ...(state.serviceAccount
            ? { serviceAccountClientId: "service" }
            : {}),
        });
      }
      if (
        url ===
          admin + "/users/" + encodeURIComponent(state.subject) +
            "/federated-identity"
      ) return json([{ identityProvider: "google", userId: "google-sub" }]);
      throw Error("UNEXPECTED PRIVATE DESTINATION");
    },
  };
  return { state, io, metadata, provider };
}
async function deps(f = fixture()) {
  const result = await createSharedRuntime(settings(), f.io);
  if (result.mode !== "configured") throw Error("expected configured");
  assertEquals(result.productionQualified, false);
  assertEquals(result.protectedOperationsAllowed, false);
  assertEquals(JSON.stringify(result).includes("PRIVATE"), false);
  return result.dependencies;
}
async function signed(exp = 2000, changes: Record<string, unknown> = {}) {
  return await new SignJWT({ typ: "Bearer", scope: "fdh:probe", ...changes })
    .setProtectedHeader({ alg: "RS256", kid: jwk.kid }).setIssuer(issuer)
    .setAudience(access.resourceAudience).setSubject(access.subject)
    .setExpirationTime(exp).sign(keys.privateKey);
}
Deno.test("synthetic signed JWT + maintained clients + online Google fixture; NOT live Google proof", async () => {
  const f = fixture();
  const d = await deps(f);
  const token = await signed();
  const verified = await d.verifyAccessToken(token);
  assertEquals(verified, access);
  assertEquals(
    (await d.resolveOnlineWorkspaceBinding(token, verified))?.identityKey,
    "google-sub",
  );
  assertEquals(f.state.grants, 1);
  assertEquals(f.state.calls.map((c) => c.method), [
    "GET",
    "GET",
    "POST",
    "POST",
    "GET",
    "GET",
    "GET",
    "GET",
    "GET",
  ]);
  assertEquals(f.state.lookups, ["FDH_READER_SECRET"]);
});
Deno.test("parallel online checks singleflight reader grant; online inactive never cached", async () => {
  const f = fixture();
  const d = await deps(f);
  const bindings = await Promise.all(
    Array.from(
      { length: 12 },
      () => d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    ),
  );
  assertEquals(bindings.every((b) => b?.identityKey === "google-sub"), true);
  assertEquals(f.state.grants, 1);
  f.state.active = false;
  assertEquals(
    await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    null,
  );
  assertEquals(
    f.state.calls.filter((c) => c.url === tokenUrl + "/introspect").length,
    13,
  );
});
Deno.test("reader early renewal bounded to 60 seconds and never beyond AS expiry", async () => {
  const f = fixture();
  f.state.expires = 10;
  const d = await deps(f);
  await d.resolveOnlineWorkspaceBinding("synthetic-token", access);
  assertEquals(f.state.grants, 1);
  f.state.now = 1008;
  await d.resolveOnlineWorkspaceBinding("synthetic-token", access);
  assertEquals(f.state.grants, 1);
  f.state.now = 1009;
  await d.resolveOnlineWorkspaceBinding("synthetic-token", access);
  assertEquals(f.state.grants, 2);
  f.state.expires = 1000;
  f.state.now = 1020;
  await d.resolveOnlineWorkspaceBinding("synthetic-token", access);
  assertEquals(f.state.grants, 3);
  f.state.now = 1080;
  await d.resolveOnlineWorkspaceBinding("synthetic-token", access);
  assertEquals(f.state.grants, 4);
});
for (const expires of [0, -1, 0.5, NaN, Infinity, 86401]) {
  Deno.test("invalid reader grant expiry fails closed " + expires, async () => {
    const f = fixture();
    f.state.expires = expires;
    const d = await deps(f);
    assertEquals(
      await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
      null,
    );
    assertEquals(
      f.state.calls.some((c) => c.url.startsWith(admin)),
      false,
    );
  });
}
Deno.test("stale JWT, wrong scope and malformed token fail with constant private diagnostics", async () => {
  const f = fixture();
  const d = await deps(f);
  for (
    const token of [
      await signed(1000),
      await signed(2000, { scope: "openid" }),
      "broken.private.token",
    ]
  ) {
    const error = await assertRejects(
      () => d.verifyAccessToken(token),
      Error,
      "Shared reader runtime rejected",
    );
    assertEquals(Object.hasOwn(error, "cause"), false);
    assertEquals(String(error).includes("PRIVATE"), false);
  }
});
Deno.test("every call rechecks service-account denial and pinned Google client", async () => {
  const f = fixture();
  const d = await deps(f);
  assertEquals(
    (await d.resolveOnlineWorkspaceBinding("synthetic-token", access))?.active,
    true,
  );
  f.state.serviceAccount = true;
  assertEquals(
    await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    null,
  );
  f.state.serviceAccount = false;
  f.provider.config.clientId = "wrong.apps.googleusercontent.com";
  assertEquals(
    await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    null,
  );
});
for (const status of [301, 302, 307, 401, 403, 500]) {
  Deno.test(
    "provider refusal " + status + " never falls back to admin",
    async () => {
      const f = fixture();
      const d = await deps(f);
      f.state.intercept = (url) =>
        url.startsWith(admin)
          ? json({ error: "PRIVATE_ERROR" }, status)
          : undefined;
      assertEquals(
        await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
        null,
      );
      assertEquals(
        f.state.calls.filter((c) => c.url.startsWith(admin)).length,
        1,
      );
    },
  );
}
for (
  const key of [
    "issuer",
    "token_endpoint",
    "jwks_uri",
    "introspection_endpoint",
    "authorization_endpoint",
    "revocation_endpoint",
  ]
) {
  for (
    const bad of [
      "https://evil.example/steal",
      "http://as.example.test/realms/fdh",
      "https://as.example.test/auth/realms/other",
    ]
  ) {
    Deno.test(
      "metadata " + key + " mismatch rejects before credential POST",
      async () => {
        const f = fixture();
        Object.assign(f.metadata, { [key]: bad });
        assertEquals(await createSharedRuntime(settings(), f.io), unavailable);
        assertEquals(f.state.calls.length, 1);
        assertEquals(f.state.grants, 0);
      },
    );
  }
}
for (const mode of ["redirected", "url", "declared", "stream", "error"]) {
  Deno.test(
    "strict fetch rejects " + mode + " without returning raw provider errors",
    async () => {
      const f = fixture();
      f.state.intercept = () => {
        if (mode === "error") throw Error("PRIVATE_UPSTREAM_SECRET");
        if (mode === "declared") {
          return new Response("{}", {
            headers: { "content-length": "262145" },
          });
        }
        if (mode === "stream") return new Response("x".repeat(262145));
        const response = json(f.metadata);
        Object.defineProperty(response, mode === "url" ? "url" : "redirected", {
          value: mode === "url" ? "https://evil.example" : true,
        });
        return response;
      };
      assertEquals(await createSharedRuntime(settings(), f.io), unavailable);
    },
  );
}
Deno.test("timeout rejects even an injected fetch that ignores abort (no network)", async () => {
  const f = fixture();
  f.state.intercept = () => new Promise<Response>(() => {});
  assertEquals(await createSharedRuntime(settings(), f.io), unavailable);
});
Deno.test("private capability getters and Proxy are rejected without invocation", async () => {
  const f = fixture();
  let calls = 0;
  const accessor = { ...f.io };
  Object.defineProperty(accessor, "fetch", {
    get() {
      calls++;
      throw Error("PRIVATE");
    },
  });
  assertEquals(await createSharedRuntime(settings(), accessor), unavailable);
  const proxy = new Proxy(f.io, {
    get() {
      calls++;
      throw Error("PRIVATE");
    },
    ownKeys() {
      calls++;
      throw Error("PRIVATE");
    },
    getPrototypeOf() {
      calls++;
      throw Error("PRIVATE");
    },
  });
  assertEquals(await createSharedRuntime(settings(), proxy), unavailable);
  assertEquals(calls, 0);
});
Deno.test("snapshot pins config and private functions before lookup awaits", async () => {
  const f = fixture();
  const cfg = settings();
  let release: ((v: unknown) => void) | undefined;
  const io = {
    ...f.io,
    lookupSecret: () =>
      new Promise<unknown>((resolve) => {
        release = resolve;
      }),
  };
  const pending = createSharedRuntime(cfg, io);
  cfg.issuer = "https://evil.example/realms/evil";
  io.fetch = () => {
    throw Error("mutated");
  };
  io.now = () => -1;
  release?.("PRIVATE_FIXTURE_SECRET");
  const result = await pending;
  assertEquals(result.mode, "configured");
  if (result.mode === "configured") {
    assertEquals(
      (await result.dependencies.resolveOnlineWorkspaceBinding(
        "synthetic-token",
        access,
      ))?.active,
      true,
    );
  }
});
for (
  const secret of [
    undefined,
    null,
    "",
    "space secret",
    "new\nline",
    42,
    {},
    "x".repeat(4097),
  ]
) {
  Deno.test(
    "invalid secret returns no hooks " + typeof secret + String(secret).length,
    async () => {
      const f = fixture();
      assertEquals(
        await createSharedRuntime(settings(), {
          ...f.io,
          lookupSecret: () => secret,
        }),
        unavailable,
      );
      assertEquals(f.state.calls.length, 0);
    },
  );
}
Deno.test("lookup rejection and absent configuration do not touch fetch", async () => {
  const f = fixture();
  const io = {
    ...f.io,
    lookupSecret: () => {
      throw Error("PRIVATE_SECRET");
    },
  };
  assertEquals(await createSharedRuntime(settings(), io), unavailable);
  assertEquals(await createSharedRuntime(undefined, io), unavailable);
  assertEquals(f.state.calls.length, 0);
});
for (
  const subject of [
    "a/b?c#d",
    "a/b",
    "a\\b",
    "a%b",
    "a?b",
    "a#b",
    "a;b",
    "%2f",
    "%5c",
    "%2e%2e",
    "%252e%252e",
    "a%252fb",
    ".",
    "..",
    "profile",
  ]
) {
  Deno.test(
    "subject routing delimiter/reserved path denied before subject GET: " +
      subject,
    async () => {
      const f = fixture();
      const d = await deps(f);
      f.state.subject = subject;
      assertEquals(
        await d.resolveOnlineWorkspaceBinding("synthetic-token", {
          ...access,
          subject,
        }),
        null,
      );
      assertEquals(
        f.state.calls.filter((c) => c.url.startsWith(admin + "/users/")).map(
          (c) => c.url,
        ),
        [admin + "/users/profile"],
      );
      assertEquals(
        f.state.calls.some((c) =>
          c.url === admin + "/identity-provider/instances/google"
        ),
        true,
      );
      assertEquals(
        f.state.calls.some((c) => c.url.endsWith("/federated-identity")),
        false,
      );
    },
  );
}
for (
  const subject of [
    "d7c3dc36-220e-47d8-8c15-0ab2cf6ea946",
    "opaque:subject",
    "opaque@subject",
    "opaque:subject@realm",
  ]
) {
  Deno.test(
    "single-segment canonical/safe opaque subject remains supported: " +
      subject,
    async () => {
      const f = fixture();
      const d = await deps(f);
      f.state.subject = subject;
      assertEquals(
        (await d.resolveOnlineWorkspaceBinding("synthetic-token", {
          ...access,
          subject,
        }))?.active,
        true,
      );
      assertEquals(
        f.state.calls.some((c) =>
          c.url === admin + "/users/" + encodeURIComponent(subject)
        ),
        true,
      );
      assertEquals(
        f.state.calls.some((c) =>
          c.url ===
            admin + "/users/" + encodeURIComponent(subject) +
              "/federated-identity"
        ),
        true,
      );
    },
  );
}
Deno.test("JWT expiring during remote key fetch is rejected after verification", async () => {
  const f = fixture();
  const d = await deps(f);
  f.state.intercept = (url) => {
    if (url.endsWith("/certs")) f.state.now = 2000;
    return undefined;
  };
  const token = await signed();
  await assertRejects(
    () => d.verifyAccessToken(token),
    Error,
    "Shared reader runtime rejected",
  );
});

Deno.test("backend must be explicitly keycloak-readonly before lookup", async () => {
  const f = fixture();
  for (const backend of [undefined, "generic-oidc", "keycloak", ""]) {
    assertEquals(
      await createSharedRuntime(
        settings(),
        { ...f.io, backend } as unknown as SharedRuntimePrivateIO,
      ),
      unavailable,
    );
  }
  assertEquals(f.state.lookups.length, 0);
  assertEquals(f.state.calls.length, 0);
});
for (const now of [-1, NaN, Infinity, 1000.1, 8_640_000_000_001]) {
  Deno.test("invalid trusted clock before lookup " + now, async () => {
    const f = fixture();
    f.state.now = now;
    assertEquals(await createSharedRuntime(settings(), f.io), unavailable);
    assertEquals(f.state.lookups.length, 0);
  });
}
Deno.test("clock rollback after initialization fails closed", async () => {
  const f = fixture();
  const d = await deps(f);
  f.state.now = 999;
  assertEquals(
    await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    null,
  );
  assertEquals(f.state.calls.length, 1);
});
Deno.test("slow grant expires before arrival and is never used for admin GET", async () => {
  const f = fixture();
  f.state.expires = 1;
  const d = await deps(f);
  f.state.intercept = (url) => {
    if (url === tokenUrl) f.state.now++;
    return undefined;
  };
  assertEquals(
    await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    null,
  );
  assertEquals(f.state.calls.some((c) => c.url.startsWith(admin)), false);
  f.state.intercept = undefined;
  assertEquals(
    (await d.resolveOnlineWorkspaceBinding("synthetic-token", access))?.active,
    true,
  );
  assertEquals(f.state.grants, 2);
});
Deno.test("failed parallel grant clears singleflight and never caches a failure", async () => {
  const f = fixture();
  const d = await deps(f);
  let attempted = 0;
  f.state.intercept = (url) => {
    if (url === tokenUrl) {
      attempted++;
      return json({ error: "PRIVATE" }, 401);
    }
    return undefined;
  };
  const bindings = await Promise.all(
    Array.from(
      { length: 8 },
      () => d.resolveOnlineWorkspaceBinding("synthetic-token", access),
    ),
  );
  assertEquals(bindings.every((b) => b === null), true);
  assertEquals(attempted, 1);
  f.state.intercept = undefined;
  assertEquals(
    (await d.resolveOnlineWorkspaceBinding("synthetic-token", access))?.active,
    true,
  );
  assertEquals(f.state.grants, 1);
});
for (const value of [undefined, null, "", "has space", 42]) {
  Deno.test(
    "malformed reader token is never sent to admin " + typeof value +
      String(value).length,
    async () => {
      const f = fixture();
      const d = await deps(f);
      f.state.intercept = (url) =>
        url === tokenUrl
          ? json({ access_token: value, token_type: "Bearer", expires_in: 100 })
          : undefined;
      assertEquals(
        await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
        null,
      );
      assertEquals(f.state.calls.some((c) => c.url.startsWith(admin)), false);
    },
  );
}
for (
  const endpoint of [
    "/protocol/openid-connect/token/introspect",
    "/identity-provider/instances/google",
    "/protocol/openid-connect/certs",
  ]
) {
  Deno.test(
    "shared strict fetch enforces byte budget for " + endpoint,
    async () => {
      const f = fixture();
      const d = await deps(f);
      f.state.intercept = (url) =>
        url.endsWith(endpoint) ? new Response("x".repeat(262145)) : undefined;
      if (endpoint.endsWith("certs")) {
        const token = await signed();
        await assertRejects(
          () => d.verifyAccessToken(token),
          Error,
          "Shared reader runtime rejected",
        );
      } else {assertEquals(
          await d.resolveOnlineWorkspaceBinding("synthetic-token", access),
          null,
        );}
    },
  );
}
Deno.test("real request methods stay in fixed GET/read and POST/token lanes only", async () => {
  const f = fixture();
  const d = await deps(f);
  const token = await signed();
  await d.verifyAccessToken(token);
  await d.resolveOnlineWorkspaceBinding(token, access);
  for (const call of f.state.calls) {
    assertEquals(
      call.method,
      [tokenUrl, tokenUrl + "/introspect"].includes(call.url) ? "POST" : "GET",
    );
    const u = new URL(call.url);
    assertEquals(u.origin, "https://as.example.test");
    assertEquals(u.search, "");
    assertEquals(u.hash, "");
    assertEquals(call.url.includes("client-secret"), false);
  }
  assertEquals(
    f.state.calls.some((c) => c.url.includes("/admin/realms/fdh/clients")),
    false,
  );
});
Deno.test("lookup and clock private exceptions cannot escape diagnostics", async () => {
  const f = fixture();
  assertEquals(
    await createSharedRuntime(settings(), {
      ...f.io,
      now: () => {
        throw Error("PRIVATE_CLOCK");
      },
    }),
    unavailable,
  );
  const d = await deps(f);
  Object.defineProperty(f.io, "now", {
    value: () => {
      throw Error("mutated ignored");
    },
  });
  // Runtime captured original function, not the mutable capabilities record.
  assertEquals(
    (await d.resolveOnlineWorkspaceBinding("synthetic-token", access))?.active,
    true,
  );
});
Deno.test("access policy pins exact audience, issuer and scope despite signed token", async () => {
  const f = fixture();
  const d = await deps(f);
  const tokens = await Promise.all([
    new SignJWT({ typ: "Bearer", scope: "fdh:probe" }).setProtectedHeader({
      alg: "RS256",
      kid: jwk.kid,
    }).setIssuer("https://other.example/realms/fdh").setAudience(
      access.resourceAudience,
    ).setSubject(access.subject).setExpirationTime(2000).sign(keys.privateKey),
    new SignJWT({ typ: "Bearer", scope: "fdh:probe" }).setProtectedHeader({
      alg: "RS256",
      kid: jwk.kid,
    }).setIssuer(issuer).setAudience("reader").setSubject(access.subject)
      .setExpirationTime(2000).sign(keys.privateKey),
    signed(2000, { scope: "openid" }),
  ]);
  for (const token of tokens) {
    await assertRejects(
      () => d.verifyAccessToken(token),
      Error,
      "Shared reader runtime rejected",
    );
  }
  assertEquals(f.state.calls.some((c) => c.method === "POST"), false);
});

import { createSharedEntryHandler } from "./fdh_shared_entry.ts";
Deno.test("runtime exposes exactly entry's two private hooks; Web/API remain 503", async () => {
  const f = fixture();
  const result = await createSharedRuntime(settings(), f.io);
  if (result.mode !== "configured") throw Error("expected configured");
  assertEquals(Reflect.ownKeys(result.dependencies).sort(), [
    "resolveOnlineWorkspaceBinding",
    "verifyAccessToken",
  ]);
  assertEquals(Object.isFrozen(result.dependencies), true);
  const handler = createSharedEntryHandler(settings(), result.dependencies);
  const bootstrap = await handler(
    new Request(settings().publicOrigin + "/auth/bootstrap"),
  );
  assertEquals(bootstrap.status, 200);
  const text = await bootstrap.text();
  assertEquals(text.includes("PRIVATE"), false);
  assertEquals(text.includes("FDH_READER_SECRET"), false);
  for (const path of ["/workspace", "/api/auth/status"]) {
    assertEquals(
      (await handler(new Request(settings().publicOrigin + path))).status,
      503,
    );
  }
  assertEquals(f.state.calls.length, 1);
});
Deno.test("root-context Keycloak issuer is explicitly accepted with pinned metadata only", async () => {
  const f = fixture();
  const rootIssuer = "https://as.example.test/realms/fdh";
  const metadata = Object.fromEntries(
    Object.entries(f.metadata).map((
      [k, v],
    ) => [k, v.replace("/auth/realms/", "/realms/")]),
  );
  const result = await createSharedRuntime({
    ...settings(),
    issuer: rootIssuer,
  }, {
    ...f.io,
    fetch: (input, init) => {
      assertEquals(
        String(input),
        rootIssuer + "/.well-known/openid-configuration",
      );
      assertEquals(init?.method, "GET");
      return Promise.resolve(json(metadata));
    },
  });
  assertEquals(result.mode, "configured");
  assertEquals(f.state.lookups, ["FDH_READER_SECRET"]);
});
