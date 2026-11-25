// SPDX-License-Identifier: MPL-2.0

Deno.test("shared MCP policy and dependency references are pinned at construction", async () => {
  const requiredScopes = ["fdh:probe"];
  const mutablePolicy = { ...policy, requiredScopes };
  const mutableDependencies = dependencies();
  const handler = createSharedMcpHandler(mutablePolicy, mutableDependencies);
  mutablePolicy.issuer = "https://evil.example";
  requiredScopes[0] = "fdh:release";
  const response = await handler(
    new Request(
      "https://fdh.example.test/.well-known/oauth-protected-resource/mcp",
    ),
  );
  const metadata = await response.json();
  assertEquals(metadata.authorization_servers, [policy.issuer]);
  assertEquals(metadata.scopes_supported, ["fdh:probe"]);
});
Deno.test("shared MCP never uses URL query or cookie tokens", async () => {
  let calls = 0;
  const handler = createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: () => {
        calls++;
        throw Error();
      },
    }),
  );
  assertEquals(
    (await handler(new Request(RESOURCE + "?access_token=UNIT_SECRET"))).status,
    404,
  );
  assertEquals(
    (await handler(request({ cookie: "access_token=UNIT_SECRET" }))).status,
    401,
  );
  assertEquals(calls, 0);
});
Deno.test("shared MCP checks literal Host header as well as request URL", async () => {
  const handler = createSharedMcpHandler(policy, dependencies());
  assertEquals((await handler(request({ host: "evil.example" }))).status, 403);
});
Deno.test("shared MCP refuses missing online deployment authorizer", () => {
  const malformed = { verifyAccessToken: dependencies().verifyAccessToken };
  assertThrows(() =>
    createSharedMcpHandler(policy, malformed as SharedMcpTrustedDependencies)
  );
});
Deno.test("shared MCP rejects scope claims inconsistent with pinned deployment", async () => {
  const base = dependencies();
  const handler = createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: async (token) => ({
        ...await base.verifyAccessToken(token),
        scopes: ["fdh:other"],
      }),
    }),
  );
  assertEquals(
    (await handler(request({ authorization: "Bearer unit.jwt.signature" })))
      .status,
    403,
  );
});
Deno.test("shared MCP rechecks expiration after online binding", async () => {
  const base = dependencies();
  const handler = createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: async (token) => ({
        ...await base.verifyAccessToken(token),
        expiresAt: Math.floor(Date.now() / 1000) - 1,
      }),
    }),
  );
  assertEquals(
    (await handler(request({ authorization: "Bearer unit.jwt.signature" })))
      .status,
    401,
  );
});
Deno.test("shared MCP SDK enforces bounded request body", async () => {
  const handler = createSharedMcpHandler(policy, dependencies());
  const body = JSON.stringify({
    jsonrpc: "2.0",
    id: 1,
    method: "initialize",
    params: { padding: "x".repeat(70000) },
  });
  const response = await handler(
    request({
      authorization: "Bearer unit.jwt.signature",
      "content-type": "application/json",
      accept: "application/json, text/event-stream",
    }, body),
  );
  assertEquals(response.status, 413);
});
Deno.test("shared MCP malformed configuration diagnostics are constant", () => {
  try {
    createSharedMcpHandler(
      { ...policy, issuer: "UNIT_CONFIG_SECRET" },
      dependencies(),
    );
  } catch (error) {
    assertEquals(String(error).includes("UNIT_CONFIG_SECRET"), false);
    return;
  }
  throw Error("Expected configuration refusal");
});

import { assertEquals, assertStringIncludes, assertThrows } from "@std/assert";
import { Client } from "@modelcontextprotocol/sdk/client";
// SDK wildcard types export appends .d.ts; bind its extensionless declaration.
// @deno-types="npm:@modelcontextprotocol/sdk@1.32.2/client/streamableHttp"
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { createSharedMcpHandler } from "./fdh_shared_mcp.ts";
import { SharedAccessError } from "./fdh_shared_access.ts";
import type {
  SharedMcpPolicy,
  SharedMcpTrustedDependencies,
  SharedMcpWorkspaceBinding,
} from "./fdh_shared_mcp_types.ts";

const RESOURCE = "https://fdh.example.test/mcp";
const policy: SharedMcpPolicy = {
  issuer: "https://issuer.example.test/realms/fdh",
  resourceAudience: RESOURCE,
  requiredScopes: ["fdh:probe"],
};
const binding: SharedMcpWorkspaceBinding = {
  kind: "verified-online-google-binding",
  active: true,
  sessionProvider: "google",
  authorizationSubject: "broker-subject",
  identityKey: "google-subject",
  identityIssuer: "https://accounts.google.com",
  email: "operator@floorp.app",
  emailVerified: true,
  hostedDomain: "floorp.app",
};
function dependencies(
  overrides: Partial<SharedMcpTrustedDependencies> = {},
): SharedMcpTrustedDependencies {
  return {
    verifyAccessToken: () =>
      Promise.resolve(Object.freeze({
        kind: "verified-shared-access-token",
        issuer: policy.issuer,
        subject: "broker-subject",
        resourceAudience: RESOURCE,
        expiresAt: 2000000000,
        scopes: Object.freeze(["fdh:probe"]),
      })),
    resolveOnlineWorkspaceBinding: () => Promise.resolve(binding),
    ...overrides,
  };
}
function request(headers: HeadersInit = {}, body?: string) {
  return new Request(RESOURCE, {
    method: body === undefined ? "GET" : "POST",
    headers,
    body,
  });
}

Deno.test("shared MCP publishes pinned RFC9728 metadata without authorizing requests", async () => {
  let calls = 0;
  const handler = createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: () => {
        calls++;
        throw Error("never");
      },
    }),
  );
  const response = await handler(
    new Request(
      "https://fdh.example.test/.well-known/oauth-protected-resource/mcp",
    ),
  );
  assertEquals(response.status, 200);
  const value = await response.json();
  assertEquals(value.resource, RESOURCE);
  assertEquals(value.authorization_servers, [policy.issuer]);
  assertEquals(value.bearer_methods_supported, ["header"]);
  assertEquals(calls, 0);
});
Deno.test("shared MCP missing bearer is401 with metadata challenge", async () => {
  const response = await createSharedMcpHandler(policy, dependencies())(
    request(),
  );
  assertEquals(response.status, 401);
  assertStringIncludes(
    response.headers.get("www-authenticate") ?? "",
    'resource_metadata="https://fdh.example.test/.well-known/oauth-protected-resource/mcp"',
  );
});
Deno.test("shared MCP wrongorigin is403 before verifier/broker IO", async () => {
  let calls = 0;
  const handler = createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: () => {
        calls++;
        throw Error();
      },
    }),
  );
  assertEquals(
    (await handler(
      request({
        origin: "https://evil.example",
        authorization: "Bearer unit.jwt.signature",
      }),
    )).status,
    403,
  );
  assertEquals(calls, 0);
});
Deno.test("shared MCP never accepts forwardedhost as resource authority", async () => {
  const handler = createSharedMcpHandler(policy, dependencies());
  const response = await handler(
    new Request("https://evil.example/mcp", {
      headers: { "x-forwarded-host": "fdh.example.test" },
    }),
  );
  assertEquals(response.status, 403);
});
Deno.test("shared MCP scope refusal is403 and errors do not echo credentials", async () => {
  const response = await createSharedMcpHandler(
    policy,
    dependencies({
      verifyAccessToken: () => {
        throw new SharedAccessError("insufficient-scope");
      },
    }),
  )(request({ authorization: "Bearer UNIT_SECRET" }));
  assertEquals(response.status, 403);
  assertEquals((await response.text()).includes("UNIT_SECRET"), false);
  assertStringIncludes(
    response.headers.get("www-authenticate") ?? "",
    "insufficient_scope",
  );
});
Deno.test("shared MCP signed token alone cannot bypass online broker binding", async () => {
  for (
    const denied of [
      null,
      { ...binding, active: false },
      { ...binding, sessionProvider: "password" },
      { ...binding, authorizationSubject: "someoneelse" },
      { ...binding, hostedDomain: "evilfloorp.app" },
      { ...binding, email: "operator@evilfloorp.app" },
      { ...binding, emailVerified: false },
      { ...binding, identityKey: "" },
    ]
  ) {
    const deps = dependencies({
      resolveOnlineWorkspaceBinding: () =>
        Promise.resolve(denied as SharedMcpWorkspaceBinding | null),
    });
    const response = await createSharedMcpHandler(policy, deps)(
      request({ authorization: "Bearer unit.jwt.signature" }),
    );
    assertEquals(response.status, 403);
  }
});
Deno.test("shared MCP provider outage failsclosed without reflecting provider error", async () => {
  const response = await createSharedMcpHandler(
    policy,
    dependencies({
      resolveOnlineWorkspaceBinding: () => {
        throw Error("UNIT_PRIVATE_PROVIDER_DETAIL");
      },
    }),
  )(request({ authorization: "Bearer unit.jwt.signature" }));
  assertEquals(response.status, 503);
  assertEquals(
    (await response.text()).includes("UNIT_PRIVATE_PROVIDER_DETAIL"),
    false,
  );
});
Deno.test("shared MCP SDK client initializes lists and calls only readonly probe (offlinefixture)", async () => {
  const handler = createSharedMcpHandler(policy, dependencies());
  const client = new Client({ name: "offline-fixture", version: "1.0.0" });
  const transport = new StreamableHTTPClientTransport(new URL(RESOURCE), {
    fetch: (input, init) => handler(new Request(input, init)),
    requestInit: { headers: { authorization: "Bearer unit.jwt.signature" } },
  });
  try {
    await client.connect(transport);
    const tools = await client.listTools();
    assertEquals(tools.tools.map((t) => t.name), ["fdh.authz.probe"]);
    assertEquals(tools.tools[0].annotations?.readOnlyHint, true);
    const result = await client.callTool({
      name: "fdh.authz.probe",
      arguments: {},
    });
    assertEquals(result.isError, undefined);
    assertEquals(JSON.stringify(result).includes("google-subject"), false);
    assertEquals(JSON.stringify(result).includes("unit.jwt.signature"), false);
    const forbidden = await client.callTool({
      name: "fdh.release.publish",
      arguments: {},
    });
    assertEquals(forbidden.isError, true);
  } finally {
    await client.close();
  }
});
Deno.test("shared MCP rejects malformed deployment and unpinned HTTP", () => {
  for (
    const changed of [
      { issuer: "http://issuer.example.test" },
      { resourceAudience: "http://fdh.example.test/mcp" },
      { resourceAudience: RESOURCE + "?query=x" },
      { requiredScopes: ['bad"scope'] },
    ]
  ) {
    assertThrows(() =>
      createSharedMcpHandler({ ...policy, ...changed }, dependencies())
    );
  }
});
Deno.test("shared MCP accepts literal loopback only for explicit local Spike", () => {
  assertThrows(() =>
    createSharedMcpHandler({
      ...policy,
      issuer: "http://127.0.0.1:18880/realms/fdh",
      resourceAudience: "http://127.0.0.1:18881/mcp",
    }, dependencies())
  );
  createSharedMcpHandler({
    ...policy,
    issuer: "http://127.0.0.1:18880/realms/fdh",
    resourceAudience: "http://127.0.0.1:18881/mcp",
    localSpike: true,
  }, dependencies());
});
