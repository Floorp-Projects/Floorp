// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { base64url, generateKeyPair, SignJWT } from "jose";
import type { JWTPayload, JWTVerifyGetKey } from "jose";
import {
  SharedAccessError,
  verifySharedAccessToken,
} from "./fdh_shared_access.ts";
import type {
  SharedAccessPolicy,
  SharedAccessTrustedDependencies,
} from "./fdh_shared_access_types.ts";

const NOW = new Date("2026-11-16T00:00:00.000Z");
const SECONDS = NOW.getTime() / 1000;
const POLICY: SharedAccessPolicy = {
  issuer: "https://auth.example/realms/fdh",
  resourceAudience: "https://mcp.example/fdh",
  requiredScopes: ["mcp:read", "mcp:execute"],
};
const key = await generateKeyPair("RS256");
const foreign = await generateKeyPair("RS256");
const deps: SharedAccessTrustedDependencies = {
  resolveKey: () => key.publicKey,
  now: () => NOW,
};
const claims: JWTPayload = {
  iss: POLICY.issuer,
  aud: POLICY.resourceAudience,
  sub: "broker-opaque-sub",
  exp: SECONDS + 60,
  typ: "Bearer",
  scope: "openid mcp:read mcp:execute",
};
async function sign(overrides: JWTPayload = {}): Promise<string> {
  return await new SignJWT({ ...claims, ...overrides }).setProtectedHeader({
    alg: "RS256",
    typ: "JWT",
  }).sign(key.privateKey);
}
async function rejects(
  token: unknown,
  policy: unknown = POLICY,
  dependencies: unknown = deps,
  code?: string,
): Promise<void> {
  const error = await assertRejects(
    () =>
      verifySharedAccessToken(
        token as string,
        policy as SharedAccessPolicy,
        dependencies as SharedAccessTrustedDependencies,
      ),
    SharedAccessError,
  );
  if (code) assertEquals(error.code, code);
  assert(!error.message.includes("broker-opaque-sub"));
  assertEquals(error.cause, undefined);
}

Deno.test("valid RS256 access token returns only frozen broker admission facts", async () => {
  const result = await verifySharedAccessToken(await sign(), POLICY, deps);
  assertEquals(result, {
    kind: "verified-shared-access-token",
    issuer: POLICY.issuer,
    subject: "broker-opaque-sub",
    resourceAudience: POLICY.resourceAudience,
    expiresAt: SECONDS + 60,
    scopes: ["openid", "mcp:read", "mcp:execute"],
  });
  assert(Object.isFrozen(result));
  assert(Object.isFrozen(result.scopes));
  for (
    const absent of [
      "identityKey",
      "identity_key",
      "mcpAuthorizationGranted",
      "ac05qualified",
      "email",
      "activeIntrospection",
    ]
  ) assert(!(absent in result));
});
Deno.test("resource audience accepts an exact array member, not client azp", async () => {
  await verifySharedAccessToken(
    await sign({
      aud: ["account", POLICY.resourceAudience],
      azp: "client-app",
    }),
    POLICY,
    deps,
  );
  await rejects(
    await sign({ aud: "client-app", azp: POLICY.resourceAudience }),
  );
  await rejects(await sign({ aud: POLICY.resourceAudience + "/" }));
});
for (
  const [name, override] of Object.entries({
    issuer: { iss: "https://evil.example/realms/fdh" },
    expired: { exp: SECONDS - 1 },
    boundary: { exp: SECONDS },
    futureNbf: { nbf: SECONDS + 1 },
    missingExp: { exp: undefined },
    badExp: { exp: "tomorrow" },
    fractionalExp: { exp: SECONDS + 0.5 },
    missingSub: { sub: undefined },
    blankSub: { sub: "" },
    objectSub: { sub: {} },
    missingIssuer: { iss: undefined },
    missingAudience: { aud: undefined },
    malformedAudience: { aud: [POLICY.resourceAudience, 3] },
    idToken: { typ: "ID" },
    noPayloadTyp: { typ: undefined },
    caseTyp: { typ: "bearer" },
    noScope: { scope: undefined },
    arrayScope: { scope: ["mcp:read", "mcp:execute"] },
    partialScope: { scope: "mcp:read mcp:executed" },
    delimiter: { scope: "mcp:read\tmcp:execute" },
  })
) {
  Deno.test("reject signed invalid claims: " + name, async () => {
    await rejects(await sign(override as JWTPayload));
  });
}
Deno.test("does not invent an iat or maximum token age requirement", async () => {
  await verifySharedAccessToken(
    await sign({ iat: 1, exp: SECONDS + 86400 * 365 }),
    POLICY,
    deps,
  );
});
Deno.test("signature, algorithm and unsigned tokens fail closed", async () => {
  await rejects(
    await new SignJWT(claims).setProtectedHeader({ alg: "RS256" }).sign(
      foreign.privateKey,
    ),
  );
  await rejects(
    await new SignJWT(claims).setProtectedHeader({ alg: "HS256" }).sign(
      new Uint8Array(32),
    ),
  );
  await rejects(
    base64url.encode(JSON.stringify({ alg: "none" })) + "." +
      base64url.encode(JSON.stringify(claims)) + ".",
  );
  const signed = await sign();
  const parts = signed.split(".");
  parts[1] = base64url.encode(JSON.stringify({ ...claims, sub: "tampered" }));
  await rejects(parts.join("."));
});
Deno.test("bounded token inputs reject before key resolution", async () => {
  let calls = 0;
  const blocked = {
    resolveKey: () => {
      calls++;
      throw Error("secret");
    },
  };
  for (
    const input of [null, 123, {}, "", "x".repeat(16385), "not-a-jwt", "a.b.c"]
  ) await rejects(input, POLICY, blocked);
  assertEquals(calls, 0);
});
Deno.test("issuer transport pinning: HTTPS or explicit literal loopback HTTP only", async () => {
  for (
    const issuer of [
      "http://127.0.0.1:8080/realms/fdh",
      "http://[::1]:8080/realms/fdh",
    ]
  ) {
    await verifySharedAccessToken(await sign({ iss: issuer }), {
      ...POLICY,
      issuer,
      localSpike: true,
    }, deps);
    await rejects(await sign(), { ...POLICY, issuer });
  }
  for (
    const issuer of [
      "http://localhost/realms/fdh",
      "http://127.1/realms/fdh",
      "http://2130706433/realms/fdh",
      "http://127.0.0.1.evil/",
      "http://10.0.0.1/",
      "https://user:pass@auth.example/",
      "https://auth.example/?q=x",
      "https://auth.example/#fragment",
      "file:///tmp/keys",
    ]
  ) {
    await rejects(
      await sign(),
      { ...POLICY, issuer, localSpike: true },
      deps,
      "invalid-policy",
    );
  }
});
Deno.test("invalid and accessor policies reject without reads or key lookup", async () => {
  let reads = 0;
  let calls = 0;
  const blocked = {
    resolveKey: () => {
      calls++;
      throw Error("secret");
    },
  };
  const accessor = Object.defineProperty({ ...POLICY }, "issuer", {
    get: () => {
      reads++;
      throw Error("secret");
    },
  });
  const scopeAccessor = Object.defineProperty(["mcp:read"], "0", {
    get: () => {
      reads++;
      return "mcp:read";
    },
  });
  for (
    const policy of [
      null,
      {},
      accessor,
      Object.create(POLICY),
      { ...POLICY, localSpike: "true" },
      { ...POLICY, issuer: 1 },
      { ...POLICY, resourceAudience: "" },
      { ...POLICY, requiredScopes: [] },
      { ...POLICY, requiredScopes: ["mcp:read", "mcp:read"] },
      { ...POLICY, requiredScopes: ["bad scope"] },
      { ...POLICY, requiredScopes: scopeAccessor },
    ]
  ) await rejects(await sign(), policy, blocked, "invalid-policy");
  assertEquals(reads, 0);
  assertEquals(calls, 0);
});
Deno.test("mutable policy, scope list, dependency and Date cannot race await", async () => {
  const mutable = { ...POLICY, requiredScopes: [...POLICY.requiredScopes] };
  const clock = new Date(NOW);
  let clockCalls = 0;
  const resolveKey: JWTVerifyGetKey = async () => {
    mutable.issuer = "https://evil.example/";
    mutable.resourceAudience = "other";
    mutable.requiredScopes[0] = "different";
    clock.setTime(0);
    dependencies.resolveKey = () => foreign.publicKey;
    await Promise.resolve();
    return key.publicKey;
  };
  const dependencies = {
    resolveKey,
    now: () => {
      clockCalls++;
      return clock;
    },
  };
  const result = await verifySharedAccessToken(
    await sign(),
    mutable,
    dependencies,
  );
  assertEquals(result.issuer, POLICY.issuer);
  assertEquals(clockCalls, 1);
});
Deno.test("clock intrinsic and error redaction", async () => {
  const clock = new Date(NOW);
  clock.getTime = () => 0;
  await verifySharedAccessToken(await sign(), POLICY, {
    ...deps,
    now: () => clock,
  });
  for (
    const dependencies of [null, {}, { ...deps, now: 2 }, {
      ...deps,
      now: () => new Date(NaN),
    }, {
      ...deps,
      now: () => {
        throw Error("secret-token");
      },
    }, { resolveKey: 1 }]
  ) await rejects(await sign(), POLICY, dependencies, "invalid-dependencies");
  const error = await assertRejects(
    () => verifySharedAccessToken("secret-token", POLICY, deps),
    SharedAccessError,
  );
  assert(!error.message.includes("secret-token"));
  const thrown = await assertRejects(
    () =>
      verifySharedAccessToken(awaitToken, POLICY, {
        ...deps,
        resolveKey: () => {
          throw Error("secret-token");
        },
      }),
    SharedAccessError,
  );
  assert(!thrown.message.includes("secret-token"));
});
const awaitToken = await sign();

Deno.test("scope sets are exact, case-sensitive, deduplicated and bounded", async () => {
  await rejects(
    await sign({ scope: "mcp:read MCP:execute" }),
    POLICY,
    deps,
    "insufficient-scope",
  );
  await rejects(
    await sign({ scope: "mcp:read mcp:execute-extra" }),
    POLICY,
    deps,
    "insufficient-scope",
  );
  const result = await verifySharedAccessToken(
    await sign({ scope: "mcp:execute mcp:read mcp:read" }),
    POLICY,
    deps,
  );
  assertEquals(result.scopes, ["mcp:execute", "mcp:read"]);
  for (
    const scope of [
      "",
      " mcp:read mcp:execute",
      "mcp:read  mcp:execute",
      "mcp:read mcp:execute ",
      "mcp:read mcp:execute " + "x".repeat(257),
      "mcp:read mcp:execute " + Array(129).fill("x").join(" "),
    ]
  ) await rejects(await sign({ scope }), POLICY, deps, "invalid-claims");
});
Deno.test("policy/dependency traps and bounded configurations redact before key lookup", async () => {
  let calls = 0;
  let reads = 0;
  const blocked = {
    resolveKey: () => {
      calls++;
      throw Error("secret");
    },
  };
  const trap = new Proxy({}, {
    getOwnPropertyDescriptor: () => {
      throw Error("secret");
    },
  });
  for (
    const policy of [
      trap,
      { ...POLICY, issuer: "x".repeat(2049) },
      { ...POLICY, resourceAudience: "x".repeat(2049) },
      { ...POLICY, requiredScopes: Array(129).fill("read") },
      { ...POLICY, requiredScopes: ["x".repeat(257)] },
      { ...POLICY, requiredScopes: ["a" + String.fromCharCode(92) + "b"] },
    ]
  ) await rejects(await sign(), policy, blocked, "invalid-policy");
  const getter = Object.defineProperty({}, "resolveKey", {
    get: () => {
      reads++;
      throw Error("secret");
    },
  });
  for (
    const dependencies of [trap, getter, {
      ...deps,
      now: () => ({ getTime: () => NOW.getTime() }),
    }]
  ) await rejects(await sign(), POLICY, dependencies, "invalid-dependencies");
  assertEquals(calls, 0);
  assertEquals(reads, 0);
});
Deno.test("issuer is never normalized to match a foreign realm or trailing slash", async () => {
  await rejects(await sign({ iss: POLICY.issuer + "/" }));
  await rejects(await sign({ iss: POLICY.issuer.replace("/fdh", "/other") }));
});
