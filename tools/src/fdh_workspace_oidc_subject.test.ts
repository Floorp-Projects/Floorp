// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertRejects } from "@std/assert";
import { createLocalJWKSet, exportJWK, generateKeyPair, SignJWT } from "jose";
import {
  verifyWorkspaceIdToken,
  verifyWorkspaceSubjectIdToken,
  WorkspaceOidcError,
} from "./fdh_workspace_oidc.ts";
const NOW = 1800000000;
const keys = await generateKeyPair("RS256", { extractable: true });
const jwk = { ...await exportJWK(keys.publicKey), kid: "subject-fixture" };
const resolveKey = createLocalJWKSet({ keys: [jwk] });
const dependencies = { resolveKey, now: () => new Date(NOW * 1000) };
async function token(patch: Record<string, unknown> = {}) {
  return await new SignJWT({
    iss: "https://accounts.google.com",
    sub: "opaque-subject",
    aud: "client",
    iat: NOW - 1,
    exp: NOW + 100,
    email: "test@floorp.app",
    email_verified: true,
    hd: "floorp.app",
    ...patch,
  }).setProtectedHeader({ alg: "RS256", kid: jwk.kid }).sign(keys.privateKey);
}
Deno.test("subject-bound fresh ID token needs existing subject but not initial-login nonce", async () => {
  const jwt = await token();
  const identity = await verifyWorkspaceSubjectIdToken(jwt, {
    clientId: "client",
    expectedSubject: "opaque-subject",
  }, dependencies);
  assertEquals(identity.identityKey, "opaque-subject");
  assertEquals(identity.mcpAuthorizationGranted, false);
  await assertRejects(
    () =>
      verifyWorkspaceIdToken(jwt, {
        clientId: "client",
        expectedNonce: "required-initial-nonce",
      }, dependencies),
    WorkspaceOidcError,
  );
});
for (
  const patch of [
    { sub: "other-subject" },
    { aud: "other-client" },
    { hd: "other.app" },
    { email_verified: false },
    { email: "test@other.app" },
    { exp: NOW - 1 },
    { iss: "https://other.app" },
    { azp: "other-client" },
    { sub: "" },
    { iat: NOW + 1 },
  ]
) {
  Deno.test(
    "subject-bound verifier preserves strict domain/time/binding " +
      JSON.stringify(patch),
    async () => {
      const jwt = await token(patch);
      await assertRejects(() =>
        verifyWorkspaceSubjectIdToken(jwt, {
          clientId: "client",
          expectedSubject: "opaque-subject",
        }, dependencies), WorkspaceOidcError);
    },
  );
}
Deno.test("subject-bound verifier rejects accessors without invoking them", async () => {
  let reads = 0;
  const jwt = await token();
  const expectation = {
    clientId: "client",
    get expectedSubject() {
      reads++;
      return "opaque-subject";
    },
  };
  await assertRejects(
    () => verifyWorkspaceSubjectIdToken(jwt, expectation, dependencies),
    WorkspaceOidcError,
  );
  assertEquals(reads, 0);
});
Deno.test("subject-bound verifier snapshots binding before asynchronous key work", async () => {
  const expectation = { clientId: "client", expectedSubject: "opaque-subject" };
  const jwt = await token();
  const identity = await verifyWorkspaceSubjectIdToken(jwt, expectation, {
    now: dependencies.now,
    resolveKey: async (header, t) => {
      expectation.clientId = "other";
      expectation.expectedSubject = "other";
      return await resolveKey(header, t);
    },
  });
  assertEquals(identity.identityKey, "opaque-subject");
});
Deno.test("subject-bound verifier never propagates secret-bearing key errors", async () => {
  const jwt = await token();
  const error = await assertRejects(
    () =>
      verifyWorkspaceSubjectIdToken(jwt, {
        clientId: "client",
        expectedSubject: "opaque-subject",
      }, {
        now: dependencies.now,
        resolveKey() {
          throw new Error("synthetic-private-value");
        },
      }),
    WorkspaceOidcError,
  );
  assertEquals(error.message, "Workspace OIDC ID token verification failed");
});
