// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import {
  base64url,
  createLocalJWKSet,
  exportJWK,
  generateKeyPair,
  type JWK,
  type JWSHeaderParameters,
  type JWTHeaderParameters,
  type JWTPayload,
  type JWTVerifyGetKey,
  SignJWT,
} from "jose";
import {
  ALLOWED_ALGORITHMS,
  createGoogleKeyResolver,
  ERROR_MESSAGES,
  GOOGLE_ID_TOKEN_ISSUERS,
  GOOGLE_ID_TOKEN_JWKS_URL,
  GOOGLE_JWKS_OPTIONS,
  IDENTITY_PATTERN,
  MAX_CLIENT_ID_CHARS,
  MAX_ID_TOKEN_BYTES,
  MAX_NONCE_CHARS,
  verifyWorkspaceIdToken,
  WORKSPACE_EMAIL_PATTERN,
  WORKSPACE_HOSTED_DOMAIN,
  WorkspaceOidcError,
} from "./fdh_workspace_oidc.ts";
import type {
  VerifiedWorkspaceIdentity,
  WorkspaceIdTokenExpectation,
  WorkspaceOidcDependencies,
  WorkspaceOidcTestOutcome as Outcome,
  WorkspaceOidcTestSigningKey as TestSigningKey,
} from "./fdh_workspace_oidc_types.ts";

// Deterministic clock: every test supplies this single capture instead of the
// machine clock, so timing verdicts are stable in CI.
const NOW = new Date("2026-11-14T00:00:00.000Z");
const NOW_SECONDS = Math.floor(NOW.getTime() / 1000);
const CLIENT_ID = "701234567890-abc123def456.apps.google.com";
const OTHER_CLIENT_ID = "999999999999-other.apps.google.com";
const NONCE = "nonce-0123456789abcdef0123";
const SUB = "118401337000000000001";

async function createTestSigningKey(kid: string): Promise<TestSigningKey> {
  const { publicKey, privateKey } = await generateKeyPair("RS256", {
    extractable: true,
  });
  const exported = await exportJWK(publicKey);
  const jwk: JWK = { ...exported, kid, alg: "RS256", use: "sig" };
  return { kid, privateKey, jwk };
}

// Ephemeral keys only: generated in process, never persisted anywhere.
const primary = await createTestSigningKey("kid-primary");
const secondary = await createTestSigningKey("kid-secondary");

function localResolver(...keys: JWK[]): JWTVerifyGetKey {
  const local = createLocalJWKSet({ keys });
  return (header, token) => local(header, token);
}

const allKeys = localResolver(primary.jwk, secondary.jwk);
const primaryOnly = localResolver(primary.jwk);

const EXPECTATION: WorkspaceIdTokenExpectation = {
  clientId: CLIENT_ID,
  expectedNonce: NONCE,
};

function clocked(resolver: JWTVerifyGetKey): WorkspaceOidcDependencies {
  return { resolveKey: resolver, now: () => NOW };
}

// Rejections that must happen before crypto never reach key resolution; a
// plain Error from this resolver fails the test loudly instead of passing.
const MUST_NOT_RUN: WorkspaceOidcDependencies = {
  resolveKey: () => {
    throw new Error("key resolution must not run for this input");
  },
  now: () => NOW,
};

async function attempt(operation: () => Promise<unknown>): Promise<Outcome> {
  try {
    await operation();
    return "fulfilled";
  } catch (error) {
    if (error instanceof WorkspaceOidcError) return error.code;
    throw error;
  }
}

async function withoutNetwork<T>(operation: () => T | Promise<T>): Promise<T> {
  const original = globalThis.fetch;
  Reflect.set(globalThis, "fetch", () => {
    throw new Error("unexpected network access");
  });
  try {
    return await operation();
  } finally {
    Reflect.set(globalThis, "fetch", original);
  }
}

function workspaceClaims(overrides: JWTPayload = {}): JWTPayload {
  return {
    iss: GOOGLE_ID_TOKEN_ISSUERS[0],
    aud: CLIENT_ID,
    sub: SUB,
    nonce: NONCE,
    email: "dev@floorp.app",
    email_verified: true,
    hd: WORKSPACE_HOSTED_DOMAIN,
    iat: NOW_SECONDS - 60,
    exp: NOW_SECONDS + 300,
    ...overrides,
  };
}

async function signClaims(
  overrides: JWTPayload = {},
  header: JWTHeaderParameters = {
    alg: "RS256",
    typ: "JWT",
    kid: primary.kid,
  },
  key: TestSigningKey["privateKey"] | Uint8Array = primary.privateKey,
): Promise<string> {
  return await new SignJWT(workspaceClaims(overrides))
    .setProtectedHeader(header)
    .sign(key);
}

async function verify(
  token: string,
  expectation: WorkspaceIdTokenExpectation = EXPECTATION,
  dependencies: WorkspaceOidcDependencies = clocked(allKeys),
): Promise<VerifiedWorkspaceIdentity> {
  return await verifyWorkspaceIdToken(token, expectation, dependencies);
}

Deno.test("accepts a well-formed floorp workspace ID token", async () => {
  const identity = await verify(await signClaims());
  assertEquals(identity.identityKey, SUB);
  assertEquals(identity.email, "dev@floorp.app");
  assertEquals(identity.hostedDomain, WORKSPACE_HOSTED_DOMAIN);
  assertEquals(identity.issuer, GOOGLE_ID_TOKEN_ISSUERS[0]);
  assertEquals(identity.audience, CLIENT_ID);
  assertEquals(identity.authorizedParty, undefined);
  assertEquals(identity.issuedAt, NOW_SECONDS - 60);
  assertEquals(identity.expiresAt, NOW_SECONDS + 300);
  assertEquals(identity.notBefore, undefined);
});

Deno.test("result is verification-only with no authorization claims", async () => {
  const identity = await verify(await signClaims());
  assertEquals(identity.scope, "workspace-id-token-verification-only");
  assertEquals(identity.liveOAuthVerified, false);
  assertEquals(identity.mcpAuthorizationGranted, false);
});

Deno.test("accepts the second official Google issuer", async () => {
  const identity = await verify(
    await signClaims({ iss: GOOGLE_ID_TOKEN_ISSUERS[1] }),
  );
  assertEquals(identity.issuer, "accounts.google.com");
});

Deno.test("accepts issued-at exactly at the captured clock", async () => {
  const identity = await verify(
    await signClaims({ iat: NOW_SECONDS, exp: NOW_SECONDS + 60 }),
  );
  assertEquals(identity.issuedAt, NOW_SECONDS);
  assertEquals(identity.expiresAt, NOW_SECONDS + 60);
});

Deno.test("accepts a case-insensitive domain and preserves the raw email", async () => {
  const identity = await verify(await signClaims({ email: "Dev@Floorp.APP" }));
  assertEquals(identity.email, "Dev@Floorp.APP");
  assertEquals(identity.hostedDomain, WORKSPACE_HOSTED_DOMAIN);
});

Deno.test("accepts a 255 character printable identity", async () => {
  const identity = await verify(await signClaims({ sub: "s".repeat(255) }));
  assertEquals(identity.identityKey, "s".repeat(255));
});

Deno.test("accepts a multi-audience token with a matching authorized party", async () => {
  const identity = await verify(
    await signClaims({ aud: [OTHER_CLIENT_ID, CLIENT_ID], azp: CLIENT_ID }),
  );
  assertEquals(identity.audience, [OTHER_CLIENT_ID, CLIENT_ID]);
  assertEquals(identity.authorizedParty, CLIENT_ID);
});

Deno.test("accepts a single-audience token with a matching authorized party", async () => {
  const identity = await verify(await signClaims({ azp: CLIENT_ID }));
  assertEquals(identity.authorizedParty, CLIENT_ID);
});

Deno.test("rejects an unexpected issuer", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ iss: "https://accounts.example.invalid" }))
    ),
    "verification-failed",
  );
});

Deno.test("rejects a missing issuer claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ iss: undefined }))),
    "verification-failed",
  );
});

Deno.test("rejects a wrong audience", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ aud: OTHER_CLIENT_ID }))
    ),
    "verification-failed",
  );
});

Deno.test("rejects an audience list without the expected client", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ aud: [OTHER_CLIENT_ID, "someone-else"] }))
    ),
    "verification-failed",
  );
});

Deno.test("rejects a missing audience claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ aud: undefined }))),
    "verification-failed",
  );
});

Deno.test("rejects an expired token", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ iat: NOW_SECONDS - 61, exp: NOW_SECONDS - 1 }))
    ),
    "verification-failed",
  );
});

Deno.test("rejects an expiration not later than issuance in the past window", async () => {
  assertEquals(
    await attempt(async () =>
      verify(
        await signClaims({ iat: NOW_SECONDS - 120, exp: NOW_SECONDS - 60 }),
      )
    ),
    "verification-failed",
  );
});

Deno.test("rejects issuance in the future", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ iat: NOW_SECONDS + 30, exp: NOW_SECONDS + 60 }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a non-integer issued-at", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ iat: NOW_SECONDS - 60.5 }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a non-integer future expiration", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ exp: NOW_SECONDS + 300.5 }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a missing expiration claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ exp: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a missing issued-at claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ iat: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a not-before in the future", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ nbf: NOW_SECONDS + 300 }))
    ),
    "verification-failed",
  );
});

Deno.test("rejects a non-integer not-before", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ nbf: NOW_SECONDS - 0.5 }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects an empty required nonce", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims(), { clientId: CLIENT_ID, expectedNonce: "" })
    ),
    "invalid-expectation",
  );
});

Deno.test("rejects an over-long required nonce", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims(), {
        clientId: CLIENT_ID,
        expectedNonce: "n".repeat(MAX_NONCE_CHARS + 1),
      })
    ),
    "invalid-expectation",
  );
});

Deno.test("rejects an empty client id", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims(), { clientId: "", expectedNonce: NONCE })
    ),
    "invalid-expectation",
  );
});

Deno.test("rejects an over-long client id", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims(), {
        clientId: "c".repeat(MAX_CLIENT_ID_CHARS + 1),
        expectedNonce: NONCE,
      })
    ),
    "invalid-expectation",
  );
});

Deno.test("rejects a non-object expectation", async () => {
  assertEquals(
    await attempt(async () =>
      verify(
        await signClaims(),
        null as unknown as WorkspaceIdTokenExpectation,
      )
    ),
    "invalid-expectation",
  );
});

Deno.test("rejects a token over the byte ceiling before crypto", async () => {
  assertEquals(
    await attempt(() =>
      verify("a".repeat(MAX_ID_TOKEN_BYTES + 1), EXPECTATION, MUST_NOT_RUN)
    ),
    "invalid-token",
  );
});

Deno.test("rejects a multi-byte token over the byte ceiling before crypto", async () => {
  assertEquals(
    await attempt(() => verify("あ".repeat(6000), EXPECTATION, MUST_NOT_RUN)),
    "invalid-token",
  );
});

Deno.test("rejects a non-string token before crypto", async () => {
  assertEquals(
    await attempt(() =>
      verify(null as unknown as string, EXPECTATION, MUST_NOT_RUN)
    ),
    "invalid-token",
  );
});

Deno.test("rejects an empty token before crypto", async () => {
  assertEquals(
    await attempt(() => verify("", EXPECTATION, MUST_NOT_RUN)),
    "invalid-token",
  );
});

Deno.test("rejects an email outside the workspace domain", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email: "dev@example.com" }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a look-alike email suffix", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email: "dev@floorp.app.evil.example" }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects an email containing whitespace in the local part", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email: "a b@floorp.app" }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a missing hosted domain", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ hd: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a non-exact hosted domain", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ hd: "Floorp.app" }))),
    "invalid-claims",
  );
});

Deno.test("rejects a string email_verified claim", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email_verified: "true" }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects email_verified false", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email_verified: false }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a missing email_verified claim", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ email_verified: undefined }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a missing email claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ email: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a missing identity claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ sub: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a non-string identity claim", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ sub: 42 as unknown as string }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects an identity containing a control character", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ sub: "bad\nsub" }))),
    "invalid-claims",
  );
});

Deno.test("rejects a non-ASCII identity", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ sub: "ユーザー" }))),
    "invalid-claims",
  );
});

Deno.test("rejects an identity longer than 255 characters", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ sub: "s".repeat(256) }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a missing required nonce claim", async () => {
  assertEquals(
    await attempt(async () => verify(await signClaims({ nonce: undefined }))),
    "invalid-claims",
  );
});

Deno.test("rejects a nonce mismatch", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ nonce: "nonce-foreign-value" }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a multi-audience token without an authorized party", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ aud: [OTHER_CLIENT_ID, CLIENT_ID] }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a multi-audience token with a foreign authorized party", async () => {
  assertEquals(
    await attempt(async () =>
      verify(
        await signClaims({
          aud: [OTHER_CLIENT_ID, CLIENT_ID],
          azp: OTHER_CLIENT_ID,
        }),
      )
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a single-audience token with a foreign authorized party", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ azp: OTHER_CLIENT_ID }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects a non-string authorized party", async () => {
  assertEquals(
    await attempt(async () =>
      verify(await signClaims({ azp: 42 as unknown as string }))
    ),
    "invalid-claims",
  );
});

Deno.test("rejects an unsigned alg-none token", async () => {
  const header = base64url.encode(
    new TextEncoder().encode(JSON.stringify({ alg: "none", typ: "JWT" })),
  );
  const body = base64url.encode(
    new TextEncoder().encode(JSON.stringify(workspaceClaims())),
  );
  assertEquals(
    await attempt(() => verify(header + "." + body + ".")),
    "verification-failed",
  );
});

Deno.test("rejects an HS256 signature", async () => {
  const token = await new SignJWT(workspaceClaims())
    .setProtectedHeader({ alg: "HS256", typ: "JWT" })
    .sign(new TextEncoder().encode("0123456789abcdef0123456789abcdef"));
  assertEquals(await attempt(() => verify(token)), "verification-failed");
});

Deno.test("rejects a tampered signature", async () => {
  const token = await signClaims();
  const parts = token.split(".");
  const head = parts[0] ?? "";
  const body = parts[1] ?? "";
  const signature = parts[2] ?? "";
  const flipped = (signature.startsWith("A") ? "B" : "A") +
    signature.slice(1);
  assertEquals(
    await attempt(() => verify(head + "." + body + "." + flipped)),
    "verification-failed",
  );
});

Deno.test("rejects a key id absent from the selected key set", async () => {
  const token = await signClaims(
    {},
    { alg: "RS256", typ: "JWT", kid: secondary.kid },
    secondary.privateKey,
  );
  assertEquals(
    await attempt(() =>
      verifyWorkspaceIdToken(
        token,
        EXPECTATION,
        clocked(primaryOnly),
      )
    ),
    "verification-failed",
  );
});

Deno.test("ignores token-supplied key-source URLs when selecting keys", async () => {
  let seenHeader: JWSHeaderParameters | undefined;
  const capturing: JWTVerifyGetKey = (header, token) => {
    seenHeader = header;
    return allKeys(header, token);
  };
  const token = await signClaims({}, {
    alg: "RS256",
    typ: "JWT",
    kid: primary.kid,
    jku: "https://keys.attacker.invalid/jwks",
    x5u: "https://keys.attacker.invalid/cert",
  });
  const identity = await verifyWorkspaceIdToken(
    token,
    EXPECTATION,
    clocked(capturing),
  );
  assertEquals(identity.identityKey, SUB);
  assertEquals(seenHeader?.["jku"], "https://keys.attacker.invalid/jwks");
  assertEquals(seenHeader?.["x5u"], "https://keys.attacker.invalid/cert");
});

Deno.test("default key resolver construction performs no network access", async () => {
  const resolver = await withoutNetwork(() => createGoogleKeyResolver());
  assertEquals(typeof resolver, "function");
});

Deno.test("verification through the injected seam performs no network access", async () => {
  const identity = await withoutNetwork(async () => verify(await signClaims()));
  assertEquals(identity.identityKey, SUB);
});

Deno.test("failures carry only constant redacted messages", async () => {
  const mismatched = await signClaims({ nonce: "nonce-foreign-value" });
  try {
    await verify(mismatched);
    assert(false, "verification must fail");
  } catch (error) {
    assert(error instanceof WorkspaceOidcError, "expected workspace error");
    assertEquals(error.code, "invalid-claims");
    assertEquals(error.message, ERROR_MESSAGES["invalid-claims"]);
    assert(!error.message.includes(mismatched));
    assert(!error.message.includes("nonce-foreign-value"));
  }
  const wrongAudience = await signClaims({ aud: OTHER_CLIENT_ID });
  try {
    await verify(wrongAudience);
    assert(false, "verification must fail");
  } catch (error) {
    assert(error instanceof WorkspaceOidcError, "expected workspace error");
    assertEquals(error.code, "verification-failed");
    assertEquals(error.message, ERROR_MESSAGES["verification-failed"]);
    assert(!error.message.includes(wrongAudience));
    assert(!error.message.includes("@floorp"));
  }
});

Deno.test("runtime module never uses key-source URL headers, direct network calls, credential storage, or runtime file APIs", async () => {
  const source = await Deno.readTextFile(
    new URL("./fdh_workspace_oidc.ts", import.meta.url),
  );
  const forbidden = [
    "jku",
    "x5u",
    "fetch(",
    "http://",
    "client_secret",
    "refresh_token",
    "redirect_uri",
    "Deno.",
    "localStorage",
  ];
  for (const token of forbidden) {
    assert(
      !source.includes(token),
      "forbidden token in runtime source: " + token,
    );
  }
});

Deno.test("types module declares types only", async () => {
  const source = await Deno.readTextFile(
    new URL("./fdh_workspace_oidc_types.ts", import.meta.url),
  );
  assert(source.includes("import type"));
  const runtimeExport = /\bexport\s+(const|let|var|function|class|enum)\b/;
  assert(!runtimeExport.test(source), "runtime export in types module");
});

Deno.test("fixed constants and bounds match the approved spike contract", () => {
  assertEquals([...GOOGLE_ID_TOKEN_ISSUERS], [
    "https://accounts.google.com",
    "accounts.google.com",
  ]);
  assertEquals(
    GOOGLE_ID_TOKEN_JWKS_URL,
    "https://www.googleapis.com/oauth2/v3/certs",
  );
  assertEquals(GOOGLE_JWKS_OPTIONS.timeoutDuration, 5_000);
  assertEquals(GOOGLE_JWKS_OPTIONS.cooldownDuration, 30_000);
  assertEquals(GOOGLE_JWKS_OPTIONS.cacheMaxAge, 300_000);
  assertEquals([...ALLOWED_ALGORITHMS], ["RS256"]);
  assertEquals(WORKSPACE_HOSTED_DOMAIN, "floorp.app");
  assertEquals(MAX_ID_TOKEN_BYTES, 16 * 1024);
  assertEquals(MAX_CLIENT_ID_CHARS, 256);
  assertEquals(MAX_NONCE_CHARS, 256);
  assertEquals(WORKSPACE_EMAIL_PATTERN.test("dev@floorp.app"), true);
  assertEquals(WORKSPACE_EMAIL_PATTERN.test("dev@floorp.app.evil"), false);
  assertEquals(IDENTITY_PATTERN.test("118401337000000000001"), true);
  assertEquals(IDENTITY_PATTERN.test(""), false);
  assertEquals(IDENTITY_PATTERN.test("line\nbreak"), false);
});

function deferred(): { promise: Promise<void>; release: () => void } {
  let release = () => {};
  const promise = new Promise<void>((resolve) => {
    release = resolve;
  });
  return { promise, release };
}

Deno.test("expectation is snapshotted before asynchronous key resolution", async () => {
  const expectation: { clientId: string; expectedNonce: string } = {
    clientId: CLIENT_ID,
    expectedNonce: NONCE,
  };
  const gate = deferred();
  const resolver: JWTVerifyGetKey = async (header, token) => {
    expectation.clientId = OTHER_CLIENT_ID;
    expectation.expectedNonce = "mutated-nonce";
    gate.release();
    await gate.promise;
    return allKeys(header, token);
  };
  const pending = verifyWorkspaceIdToken(
    await signClaims(),
    expectation,
    clocked(resolver),
  );
  gate.release();
  const identity = await pending;
  assertEquals(identity.identityKey, SUB);
  assertEquals(identity.audience, CLIENT_ID);
});

Deno.test("original nonce stays required when expectation mutates during key resolution", async () => {
  const expectation: { clientId: string; expectedNonce: string } = {
    clientId: CLIENT_ID,
    expectedNonce: NONCE,
  };
  const gate = deferred();
  const resolver: JWTVerifyGetKey = async (header, token) => {
    expectation.clientId = OTHER_CLIENT_ID;
    expectation.expectedNonce = "mutated-nonce";
    gate.release();
    await gate.promise;
    return allKeys(header, token);
  };
  const token = await signClaims({ nonce: "mutated-nonce" });
  const pending = attempt(() =>
    verifyWorkspaceIdToken(token, expectation, clocked(resolver))
  );
  gate.release();
  assertEquals(await pending, "invalid-claims");
});

Deno.test("expectation accessors are rejected before any key work", async () => {
  let getterCalls = 0;
  let keyCalls = 0;
  const expectation = {
    get clientId(): string {
      getterCalls += 1;
      return CLIENT_ID;
    },
    get expectedNonce(): string {
      getterCalls += 1;
      return NONCE;
    },
  };
  const resolver: JWTVerifyGetKey = (header, token) => {
    keyCalls += 1;
    return allKeys(header, token);
  };
  const token = await signClaims();
  assertEquals(
    await attempt(() =>
      verifyWorkspaceIdToken(token, expectation, clocked(resolver))
    ),
    "invalid-expectation",
  );
  assertEquals(getterCalls, 0);
  assertEquals(keyCalls, 0);
});

Deno.test("throwing expectation property traps are redacted to a constant message", async () => {
  let keyCalls = 0;
  const trapped = new Proxy(
    { clientId: CLIENT_ID, expectedNonce: NONCE },
    {
      getOwnPropertyDescriptor(): never {
        throw new Error("sensitive-property-detail");
      },
    },
  );
  const resolver: JWTVerifyGetKey = (header, token) => {
    keyCalls += 1;
    return allKeys(header, token);
  };
  try {
    await verifyWorkspaceIdToken(
      await signClaims(),
      trapped,
      clocked(resolver),
    );
    assert(false, "expectation must be rejected");
  } catch (error) {
    assert(error instanceof WorkspaceOidcError, "expected workspace error");
    assertEquals(error.code, "invalid-expectation");
    assertEquals(error.message, ERROR_MESSAGES["invalid-expectation"]);
    assert(!error.message.includes("sensitive-property-detail"));
  }
  assertEquals(keyCalls, 0);
});

Deno.test("clock source failure is redacted to a constant message", async () => {
  const deps: WorkspaceOidcDependencies = {
    resolveKey: allKeys,
    now: () => {
      throw new Error("sensitive-clock-detail");
    },
  };
  try {
    await verifyWorkspaceIdToken(await signClaims(), EXPECTATION, deps);
    assert(false, "clock failure must be reported");
  } catch (error) {
    assert(error instanceof WorkspaceOidcError, "expected workspace error");
    assertEquals(error.code, "invalid-expectation");
    assertEquals(error.message, ERROR_MESSAGES["invalid-expectation"]);
    assert(!error.message.includes("sensitive-clock-detail"));
  }
});

Deno.test("non-date clock result is rejected with a constant message", async () => {
  const deps: WorkspaceOidcDependencies = {
    resolveKey: allKeys,
    now: () => "not-a-date" as unknown as Date,
  };
  const token = await signClaims();
  assertEquals(
    await attempt(() => verifyWorkspaceIdToken(token, EXPECTATION, deps)),
    "invalid-expectation",
  );
});

Deno.test("caller clock mutation during key resolution cannot expire a valid token", async () => {
  const mutable = new Date(NOW.getTime());
  const gate = deferred();
  const resolver: JWTVerifyGetKey = async (header, token) => {
    mutable.setTime(Date.UTC(2010, 0, 1));
    gate.release();
    await gate.promise;
    return allKeys(header, token);
  };
  const deps: WorkspaceOidcDependencies = {
    resolveKey: resolver,
    now: () => mutable,
  };
  const pending = verifyWorkspaceIdToken(
    await signClaims(),
    EXPECTATION,
    deps,
  );
  gate.release();
  const identity = await pending;
  assertEquals(identity.identityKey, SUB);
});

Deno.test("caller clock mutation during key resolution cannot validate an expired token", async () => {
  const mutable = new Date(NOW.getTime());
  const gate = deferred();
  const resolver: JWTVerifyGetKey = async (header, token) => {
    mutable.setTime(Date.UTC(2006, 5, 1));
    gate.release();
    await gate.promise;
    return allKeys(header, token);
  };
  const deps: WorkspaceOidcDependencies = {
    resolveKey: resolver,
    now: () => mutable,
  };
  const expired = await signClaims({
    iat: Math.floor(Date.UTC(2005, 0, 1) / 1000),
    exp: Math.floor(Date.UTC(2007, 0, 1) / 1000),
  });
  const pending = attempt(() =>
    verifyWorkspaceIdToken(expired, EXPECTATION, deps)
  );
  gate.release();
  assertEquals(await pending, "verification-failed");
});

Deno.test("timing claims outside the safe integer range are rejected", async () => {
  const token = await signClaims({ exp: Number.MAX_SAFE_INTEGER + 2 });
  assertEquals(await attempt(() => verify(token)), "invalid-claims");
});

Deno.test("exported constant tables are frozen", () => {
  assertEquals(Object.isFrozen(GOOGLE_ID_TOKEN_ISSUERS), true);
  assertEquals(Object.isFrozen(ALLOWED_ALGORITHMS), true);
  assertEquals(Object.isFrozen(ERROR_MESSAGES), true);
  assertEquals(Object.isFrozen(GOOGLE_JWKS_OPTIONS), true);
});

Deno.test("email with a trailing line terminator is rejected by the exact pattern", async () => {
  for (
    const email of [
      "dev@floorp.app\n",
      "dev@floorp.app\r\n",
      "dev@floorp.app\u2028",
    ]
  ) {
    const token = await signClaims({ email });
    assertEquals(await attempt(() => verify(token)), "invalid-claims", email);
  }
});

Deno.test("email with trailing or leading whitespace is rejected", async () => {
  for (
    const email of [
      "dev@floorp.app ",
      "dev@floorp.app\t",
      " dev@floorp.app",
    ]
  ) {
    const token = await signClaims({ email });
    assertEquals(await attempt(() => verify(token)), "invalid-claims", email);
  }
});

Deno.test("identity with a trailing line terminator is rejected by the exact pattern", async () => {
  for (const sub of ["identity\n", "identity\r\n"]) {
    const token = await signClaims({ sub });
    assertEquals(await attempt(() => verify(token)), "invalid-claims", sub);
  }
});

Deno.test("identity with a line separator or embedded carriage return is rejected", async () => {
  for (const sub of ["identity\u2028", "id\rentity"]) {
    const token = await signClaims({ sub });
    assertEquals(await attempt(() => verify(token)), "invalid-claims", sub);
  }
});

Deno.test("audience array elements must all be non-empty strings", async () => {
  const token = await signClaims({
    aud: [CLIENT_ID, 42] as unknown as string[],
    azp: CLIENT_ID,
  });
  assertEquals(await attempt(() => verify(token)), "invalid-claims");
});

Deno.test("one-shot intrinsic clock rejects an expired token despite a stateful getTime", async () => {
  let reads = 0;
  const date = new Date(NOW.getTime());
  Object.defineProperty(date, "getTime", {
    value: () => ++reads === 1 ? NOW.getTime() : NaN,
  });
  const token = await signClaims({
    iat: NOW_SECONDS - 2000,
    exp: NOW_SECONDS - 1000,
  });
  assertEquals(
    await attempt(() =>
      verifyWorkspaceIdToken(token, EXPECTATION, {
        resolveKey: allKeys,
        now: () => date,
      })
    ),
    "verification-failed",
  );
  assertEquals(reads, 0);
});

for (const edge of ["clock", "resolver", "expectation"]) {
  Deno.test(
    "forged workspace error message is redacted at " + edge,
    async () => {
      const fail = (): never => {
        const error = new WorkspaceOidcError("invalid-expectation");
        error.message = "sensitive-forged-error-detail";
        throw error;
      };
      let expectation = EXPECTATION;
      let dependencies = clocked(allKeys);
      if (edge === "clock") dependencies = { resolveKey: allKeys, now: fail };
      if (edge === "resolver") {
        dependencies = {
          get resolveKey(): JWTVerifyGetKey {
            return fail();
          },
          now: () => NOW,
        };
      }
      if (edge === "expectation") {
        expectation = new Proxy(EXPECTATION, {
          getOwnPropertyDescriptor: fail,
        });
      }
      try {
        await verifyWorkspaceIdToken(
          await signClaims(),
          expectation,
          dependencies,
        );
        assert(false, "injected failure must be rejected");
      } catch (error) {
        assert(error instanceof WorkspaceOidcError);
        assertEquals(error.code, "invalid-expectation");
        assertEquals(error.message, ERROR_MESSAGES["invalid-expectation"]);
      }
    },
  );
}
