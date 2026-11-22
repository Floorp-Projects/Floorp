// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import { createSharedEntryHandler } from "./fdh_shared_entry.ts";
import type { SharedMcpTrustedDependencies } from "./fdh_shared_mcp_types.ts";

const origin = "https://fdh.example.test";
const config = {
  publicOrigin: origin,
  issuer: "https://issuer.example.test/realm/fdh",
  gatewayLoginUrl: origin + "/gateway/google/login",
  expectedGoogleClientId: "synthetic.apps.googleusercontent.com",
  readerClientId: "synthetic-reader",
  readerSecretEnv: "FDH_SYNTHETIC_READER_SECRET",
};
const metadataPath = "/.well-known/oauth-protected-resource/mcp";
function request(path: string, init?: RequestInit): Request {
  return new Request(origin + path, init);
}
function dependencies(
  onCall: () => void = () => {},
): SharedMcpTrustedDependencies {
  return {
    verifyAccessToken: () => {
      onCall();
      return Promise.reject(Error("PRIVATE_BROKER_ERROR"));
    },
    resolveOnlineWorkspaceBinding: () => {
      onCall();
      return Promise.reject(Error("PRIVATE_BINDING_ERROR"));
    },
  };
}
function secure(response: Response): void {
  assertEquals(response.headers.get("cache-control"), "no-store");
  assertEquals(response.headers.get("referrer-policy"), "no-referrer");
  assertEquals(response.headers.get("x-content-type-options"), "nosniff");
  assertEquals(
    response.headers.get("content-security-policy"),
    "default-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'",
  );
  assertEquals(response.headers.has("set-cookie"), false);
}

Deno.test("entry landing and exact public bootstrap require private hooks", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(config, dependencies(() => calls++));
  const response = await handler(request("/auth/bootstrap"));
  assertEquals(response.status, 200);
  secure(response);
  assertEquals(await response.json(), {
    mode: "configured",
    publicOrigin: origin,
    loginAvailable: true,
    loginPath: "/auth/login",
    mcpResource: origin + "/mcp",
    apiAuthStatusPath: "/api/auth/status",
    productionQualified: false,
    protectedOperationsAllowed: false,
  });
  const landing = await handler(request("/"));
  assertEquals(landing.status, 200);
  secure(landing);
  assertEquals(
    await landing.text(),
    '<!doctype html><html lang="en"><meta charset="utf-8"><title>FDH shared workspace</title><h1>FDH shared workspace</h1><p>Protected workspace access is not available here.</p><a href="/auth/login">Sign in with Google</a></html>',
  );
  assertEquals(calls, 0);
});

for (const input of [undefined, null, config]) {
  Deno.test(
    "entry without configured private hooks fails closed: " +
      String(input === config),
    async () => {
      const handler = createSharedEntryHandler(input);
      for (
        const path of [
          "/",
          "/auth/bootstrap",
          "/auth/login",
          "/mcp",
          metadataPath,
          "/workspace",
          "/api/auth/status",
          "/unknown",
        ]
      ) {
        const response = await handler(
          request(path, { headers: { authorization: "Bearer synthetic" } }),
        );
        assertEquals(response.status, 503);
        secure(response);
        assertEquals(response.headers.has("location"), false);
        const text = await response.text();
        assertEquals(text.includes('href="/auth/login"'), false);
        assertEquals(text.includes("FDH_SYNTHETIC_READER_SECRET"), false);
        if (path === "/auth/bootstrap") {
          assertEquals(JSON.parse(text), {
            mode: "unconfigured",
            loginAvailable: false,
            productionQualified: false,
            protectedOperationsAllowed: false,
          });
        }
      }
    },
  );
}

Deno.test("entry login redirects exactly to pinned operator gateway without claiming session", async () => {
  let calls = 0;
  const mutable = { ...config };
  const handler = createSharedEntryHandler(
    mutable,
    dependencies(() => calls++),
  );
  mutable.gatewayLoginUrl = "https://evil.example/login";
  const response = await handler(
    request("/auth/login", {
      headers: { cookie: "fake=session", "x-forwarded-host": "evil.example" },
    }),
  );
  assertEquals(response.status, 303);
  assertEquals(response.headers.get("location"), config.gatewayLoginUrl);
  secure(response);
  assertEquals(calls, 0);
});

Deno.test("entry guards every route before private IO", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(config, dependencies(() => calls++));
  for (
    const path of [
      "/",
      "/auth/bootstrap",
      "/auth/login",
      "/mcp",
      metadataPath,
      "/workspace",
      "/api/auth/status",
    ]
  ) {
    for (
      const headers of [
        new Headers({ host: "evil.example" }),
        new Headers({ origin: "https://evil.example" }),
        new Headers({ origin: "null" }),
      ]
    ) {
      const response = await handler(request(path, { headers }));
      assertEquals(response.status, 403);
      secure(response);
      assertEquals(response.headers.has("location"), false);
    }
    const response = await handler(new Request("https://evil.example" + path));
    assertEquals(response.status, 403);
  }
  assertEquals(calls, 0);
});

Deno.test("entry rejects query, fragment and encoded route aliases without IO or redirect", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(config, dependencies(() => calls++));
  for (
    const path of [
      "/auth/login?next=https://evil.example",
      "/auth/login?",
      "/auth/login#",
      "/auth/login#fragment",
      "/auth/bootstrap?x=1",
      "/mcp?access_token=synthetic",
      metadataPath + "?",
      "/%61uth/login",
      "//auth/login",
    ]
  ) {
    const response = await handler(
      request(path, { headers: { authorization: "Bearer synthetic" } }),
    );
    assertEquals(response.status, 404);
    assertEquals(response.headers.has("location"), false);
    secure(response);
  }
  assertEquals(calls, 0);
});

Deno.test("entry public routes are GET only; protected router is explicitly unavailable", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(config, dependencies(() => calls++));
  for (const path of ["/", "/auth/bootstrap", "/auth/login"]) {
    for (const method of ["POST", "HEAD", "OPTIONS", "DELETE"]) {
      const response = await handler(request(path, { method }));
      assertEquals(response.status, 405);
      assertEquals(response.headers.get("allow"), "GET");
      secure(response);
    }
  }
  for (const path of ["/workspace", "/api/auth/status"]) {
    const response = await handler(
      request(path, {
        headers: { authorization: "Bearer synthetic", cookie: "session=fake" },
      }),
    );
    assertEquals(response.status, 503);
    assertEquals(await response.json(), {
      error: "authenticated-router-unavailable",
      productionQualified: false,
      protectedOperationsAllowed: false,
    });
    secure(response);
  }
  assertEquals(calls, 0);
});

Deno.test("entry composes existing MCP metadata/challenge and redacts private failure", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(config, dependencies(() => calls++));
  const metadata = await handler(request(metadataPath));
  assertEquals(metadata.status, 200);
  assertEquals(await metadata.json(), {
    resource: origin + "/mcp",
    authorization_servers: [config.issuer],
    scopes_supported: ["fdh:probe"],
    bearer_methods_supported: ["header"],
    resource_name: "FDH shared MCP local qualification probe",
  });
  secure(metadata);
  const missing = await handler(request("/mcp"));
  assertEquals(missing.status, 401);
  assertEquals(
    missing.headers.get("www-authenticate"),
    'Bearer resource_metadata="' + origin + metadataPath + '"',
  );
  assertEquals(calls, 0);
  const denied = await handler(
    request("/mcp", { headers: { authorization: "Bearer synthetic" } }),
  );
  assertEquals(denied.status, 401);
  assertEquals((await denied.text()).includes("PRIVATE_"), false);
  secure(denied);
  assertEquals(calls, 1);
});

Deno.test("entry rejects getters, proxies, callable proxies, inherited and extra private hooks without traps", async () => {
  let traps = 0;
  const hook = () => {
    traps++;
    throw Error("must not run");
  };
  const proxy = new Proxy(dependencies(), {
    get: hook,
    getOwnPropertyDescriptor: hook,
    getPrototypeOf: hook,
    ownKeys: hook,
  });
  const revoked = Proxy.revocable(dependencies(), {});
  revoked.revoke();
  const getter = Object.defineProperty({}, "verifyAccessToken", { get: hook });
  const callable = new Proxy(dependencies().verifyAccessToken, { apply: hook });
  for (
    const value of [
      proxy,
      revoked.proxy,
      getter,
      Object.create(dependencies()),
      { ...dependencies(), extra: hook },
      { ...dependencies(), verifyAccessToken: callable },
      { verifyAccessToken: hook },
    ]
  ) {
    const handler = createSharedEntryHandler(
      config,
      value as SharedMcpTrustedDependencies,
    );
    const response = await handler(request("/auth/bootstrap"));
    assertEquals(response.status, 503);
    assertEquals((await response.json()).loginAvailable, false);
  }
  assertEquals(traps, 0);
});

Deno.test("entry snapshots hook references and never serializes private settings", async () => {
  let calls = 0;
  const mutable = dependencies(() => calls++);
  const handler = createSharedEntryHandler(config, mutable);
  Object.defineProperty(mutable, "verifyAccessToken", {
    value: () => {
      throw Error("replacement");
    },
  });
  await handler(
    request("/mcp", { headers: { authorization: "Bearer synthetic" } }),
  );
  assertEquals(calls, 1);
  for (const path of ["/", "/auth/bootstrap", "/api/auth/status"]) {
    const text = await (await handler(request(path))).text();
    for (
      const secret of [
        "server",
        "readerSecretEnv",
        config.readerSecretEnv,
        config.readerClientId,
        config.expectedGoogleClientId,
        "privateErrors",
        config.gatewayLoginUrl,
      ]
    ) assertEquals(text.includes(secret), false);
  }
});

Deno.test("entry does not bypass MCP online binding after verified access", async () => {
  const calls: string[] = [];
  const handler = createSharedEntryHandler(config, {
    verifyAccessToken: (token) => {
      calls.push("verify:" + token);
      return Promise.resolve({
        kind: "verified-shared-access-token",
        issuer: config.issuer,
        subject: "synthetic-subject",
        resourceAudience: origin + "/mcp",
        expiresAt: Math.floor(Date.now() / 1000) + 60,
        scopes: ["fdh:probe"],
      });
    },
    resolveOnlineWorkspaceBinding: (token, access) => {
      calls.push("binding:" + token + ":" + access.subject);
      return Promise.resolve(null);
    },
  });
  const response = await handler(
    request("/mcp", { headers: { authorization: "Bearer synthetic" } }),
  );
  assertEquals(response.status, 403);
  assertEquals(calls, [
    "verify:synthetic",
    "binding:synthetic:synthetic-subject",
  ]);
  secure(response);
});

Deno.test("entry with hooks but no operator config never invokes hooks", async () => {
  let calls = 0;
  const handler = createSharedEntryHandler(
    undefined,
    dependencies(() => calls++),
  );
  for (
    const path of [
      "/auth/bootstrap",
      "/auth/login",
      "/mcp",
      metadataPath,
      "/workspace",
      "/api/auth/status",
    ]
  ) {
    const response = await handler(
      request(path, { headers: { authorization: "Bearer synthetic" } }),
    );
    assertEquals(response.status, 503);
  }
  assertEquals(calls, 0);
});

Deno.test("entry invalid operator input fails with fixed configuration error", () => {
  assertThrows(
    () => createSharedEntryHandler({ secret: "DO_NOT_ECHO" }),
    Error,
    "Shared service configuration rejected",
  );
});
