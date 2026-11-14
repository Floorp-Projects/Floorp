// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import { calculatePKCECodeChallenge } from "openid-client";
import { createLocalJWKSet, exportJWK, generateKeyPair, SignJWT } from "jose";
import {
  WorkspaceOAuthClient,
  WorkspaceOAuthError,
} from "./fdh_workspace_oauth.ts";
import type {
  WorkspaceOAuthOptions,
  WorkspaceOAuthTestRequest,
} from "./fdh_workspace_oauth_types.ts";

const REDIRECT = "http://127.0.0.1:19281/oauth/callback";
const TOKEN = "https://oauth2.googleapis.com/token";
const REVOKE = "https://oauth2.googleapis.com/revoke";
const SECRET = "synthetic-provider-secret-not-a-real-token";

async function fixture(options: Partial<WorkspaceOAuthOptions> = {}) {
  const keys = await generateKeyPair("RS256", { extractable: true });
  const jwk = {
    ...await exportJWK(keys.publicKey),
    kid: "offline-google",
    alg: "RS256",
  };
  let now = Date.now();
  const claims: Record<string, unknown> = {};
  const responsePatch: Record<string, unknown> = {};
  const flags = { tokenError: false, revokeError: false, redirectToken: false };
  const requests: WorkspaceOAuthTestRequest[] = [];
  const flows = new Map<string, URL>();
  let hold: Promise<void> | undefined;
  let heldGrant: string | undefined;
  let enteredHold: (() => void) | undefined;
  const config: WorkspaceOAuthOptions = {
    clientId: "offline-client",
    redirectUri: REDIRECT,
    authorizationTtlMs: 300000,
    maxPending: 2,
    maxSessions: 4,
    ...options,
  };
  const oauth = new WorkspaceOAuthClient(config, {
    now: () => now,
    oidc: { resolveKey: createLocalJWKSet({ keys: [jwk] }) },
    fetch: async (url, init) => {
      const params = new URLSearchParams(String(init?.body));
      requests.push({ url, params, redirect: init?.redirect });
      if (url === REVOKE) {
        if (flags.revokeError) throw new Error(SECRET);
        return new Response("", { status: 200 });
      }
      assertEquals(url, TOKEN);
      if (
        hold !== undefined &&
        (heldGrant === undefined || params.get("grant_type") === heldGrant)
      ) {
        enteredHold?.();
        await hold;
      }
      if (flags.tokenError) throw new Error(SECRET);
      if (flags.redirectToken) {
        return new Response(null, {
          status: 302,
          headers: { location: "https://evil.app/token" },
        });
      }
      const isCode = params.get("grant_type") === "authorization_code";
      const flow = isCode ? flows.get(params.get("code") ?? "") : undefined;
      if (isCode) {
        assert(flow !== undefined);
        assertEquals(params.get("redirect_uri"), config.redirectUri);
        assertEquals(
          await calculatePKCECodeChallenge(params.get("code_verifier") ?? ""),
          flow.searchParams.get("code_challenge"),
        );
      }
      const seconds = Math.floor(Date.now() / 1000);
      const payload = {
        iss: "https://accounts.google.com",
        aud: config.clientId,
        sub: "workspace-subject",
        email: "tester@floorp.app",
        hd: "floorp.app",
        email_verified: true,
        iat: seconds - 1,
        exp: seconds + 3600,
        ...(isCode ? { nonce: flow?.searchParams.get("nonce") } : {}),
        ...claims,
      };
      const idToken = await new SignJWT(payload).setProtectedHeader({
        alg: "RS256",
        kid: jwk.kid,
      }).sign(keys.privateKey);
      return Response.json({
        access_token: SECRET + "-access",
        refresh_token: SECRET + (isCode ? "-refresh" : "-rotated"),
        expires_in: 3600,
        token_type: "Bearer",
        id_token: idToken,
        scope: "openid email profile",
        ...responsePatch,
      });
    },
  });
  async function begin() {
    const result = await oauth.beginAuthorization();
    const url = new URL(result.authorizationUrl);
    flows.set(url.searchParams.get("state") ?? "", url);
    return {
      ...result,
      url,
      callback: REDIRECT + "?state=" + url.searchParams.get("state") +
        "&code=" + url.searchParams.get("state"),
    };
  }
  return {
    oauth,
    config,
    claims,
    flags,
    requests,
    responsePatch,
    begin,
    advance(ms: number) {
      now += ms;
    },
    block(grant?: string) {
      let release = () => {};
      heldGrant = grant;
      const entered = new Promise<void>((resolve) => {
        enteredHold = resolve;
      });
      hold = new Promise<void>((resolve) => {
        release = resolve;
      });
      return Object.assign(() => {
        hold = undefined;
        heldGrant = undefined;
        enteredHold = undefined;
        release();
      }, { entered });
    },
  };
}

Deno.test("OAuth begin uses fixed Google endpoints and mandatory independent S256/state/nonce without I/O", async () => {
  const f = await fixture();
  const a = await f.begin();
  const b = await f.begin();
  assertEquals(f.requests.length, 0);
  assertEquals(
    a.url.origin + a.url.pathname,
    "https://accounts.google.com/o/oauth2/v2/auth",
  );
  for (const key of ["state", "nonce", "code_challenge"]) {
    assert((a.url.searchParams.get(key)?.length ?? 0) >= 43);
    assert(a.url.searchParams.get(key) !== b.url.searchParams.get(key));
  }
  assertEquals(a.url.searchParams.get("code_challenge_method"), "S256");
  assertEquals(a.url.searchParams.get("scope"), "openid email profile");
  assertEquals(a.url.searchParams.get("access_type"), "offline");
  assertEquals(a.url.searchParams.get("prompt"), "consent");
  assertEquals(a.url.searchParams.get("hd"), "floorp.app");
  assert(!a.url.searchParams.has("code_verifier"));
  await assertRejects(() => f.oauth.beginAuthorization(), WorkspaceOAuthError);
});

Deno.test("OAuth callback exchanges code with PKCE and nonce and consumes state once", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const session = await f.oauth.completeAuthorization(flow.callback);
  assertEquals(session.identity.email, "tester@floorp.app");
  assertEquals(session.mcpAuthorizationGranted, false);
  assert(!JSON.stringify(session).includes(SECRET));
  assertEquals(JSON.stringify(f.oauth), "{}");
  assertEquals(f.requests[0].params.get("client_id"), "offline-client");
  assertEquals(f.requests[0].redirect, "manual");
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assertEquals(f.requests.filter((r) => r.url === TOKEN).length, 1);
});

Deno.test("OAuth parallel duplicate callback admits one grant only", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const release = f.block();
  const first = f.oauth.completeAuthorization(flow.callback);
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  release();
  await first;
  assertEquals(f.requests.filter((r) => r.url === TOKEN).length, 1);
});

for (
  const mutate of [
    (url: string) => url.replace("127.0.0.1", "localhost"),
    (url: string) => url.replace("/oauth/callback", "/other"),
    (url: string) => url + "#fragment",
    (url: string) => url + "&state=duplicate",
    (url: string) => url + "&code=duplicate",
    (url: string) => url + "&unexpected=true",
    (url: string) => url.replace("state=", "state=wrong"),
    (url: string) => url.replace("http://", "http://user:password@"),
  ]
) {
  Deno.test(
    "OAuth rejects mismatched or ambiguous callback " +
      mutate.toString().slice(-55),
    async () => {
      const f = await fixture();
      const flow = await f.begin();
      await assertRejects(
        () => f.oauth.completeAuthorization(mutate(flow.callback)),
        WorkspaceOAuthError,
      );
      assertEquals(f.requests.length, 0);
    },
  );
}

Deno.test("OAuth denied callback consumes the flow and never sends provider error details", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const url = new URL(flow.callback);
  url.searchParams.delete("code");
  url.searchParams.set("error", "access_denied");
  url.searchParams.set("error_description", SECRET);
  const e = await assertRejects(
    () => f.oauth.completeAuthorization(url.href),
    WorkspaceOAuthError,
  );
  assert(!String(e).includes(SECRET));
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assertEquals(f.requests.length, 0);
});

Deno.test("OAuth expiry and clock regression cannot resurrect pending state", async () => {
  const f = await fixture({ maxPending: 1, authorizationTtlMs: 10 });
  const flow = await f.begin();
  f.advance(10);
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  const next = await f.begin();
  f.advance(-1);
  await assertRejects(
    () => f.oauth.completeAuthorization(next.callback),
    WorkspaceOAuthError,
  );
  assertEquals(f.requests.length, 0);
});

Deno.test("OAuth parallel pending capacity is reserved before asynchronous PKCE", async () => {
  const f = await fixture({ maxPending: 1 });
  const first = f.begin();
  await assertRejects(() => f.begin(), WorkspaceOAuthError);
  await first;
  f.oauth.clearPending();
  await f.begin();
});

Deno.test("OAuth cancellation during PKCE prevents late begin result", async () => {
  const f = await fixture();
  const begin = f.oauth.beginAuthorization();
  f.oauth.clearPending();
  await assertRejects(() => begin, WorkspaceOAuthError);
});

for (
  const [name, patch] of Object.entries({
    nonce: { nonce: "wrong" },
    subject: { sub: "" },
    email: { email: "tester@evil.app" },
    hd: { hd: "evil.app" },
    verified: { email_verified: false },
    audience: { aud: "other-client" },
    issuer: { iss: "https://evil.app" },
    expired: { exp: 1 },
    azp: { azp: "other-client" },
  })
) {
  Deno.test("OAuth rejects provider ID-token " + name, async () => {
    const f = await fixture();
    Object.assign(f.claims, patch);
    const flow = await f.begin();
    await assertRejects(
      () => f.oauth.completeAuthorization(flow.callback),
      WorkspaceOAuthError,
    );
    await assertRejects(
      () => f.oauth.completeAuthorization(flow.callback),
      WorkspaceOAuthError,
    );
    assertEquals(f.requests.filter((r) => r.url === TOKEN).length, 1);
  });
}

Deno.test("OAuth failed exchange is nonreplayable and redacts raw transport errors", async () => {
  const f = await fixture();
  f.flags.tokenError = true;
  const flow = await f.begin();
  const error = await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assert(!String(error).includes(SECRET));
  assert(!("cause" in error));
  f.flags.tokenError = false;
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assertEquals(f.requests.filter((r) => r.url === TOKEN).length, 1);
});

Deno.test("OAuth refresh verifies fresh subject-bound ID-token without inventing nonce and rotates credential", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const first = await f.oauth.completeAuthorization(flow.callback);
  const refreshed = await f.oauth.refreshSession(first.id);
  assertEquals(refreshed.identity.identityKey, first.identity.identityKey);
  assert(!JSON.stringify(refreshed).includes(SECRET));
  await f.oauth.refreshSession(first.id);
  const grants = f.requests.filter((r) =>
    r.params.get("grant_type") === "refresh_token"
  );
  assertEquals(grants[0].params.get("refresh_token"), SECRET + "-refresh");
  assertEquals(grants[1].params.get("refresh_token"), SECRET + "-rotated");
  const result = await f.oauth.logout(first.id);
  assertEquals(result, {
    localLogout: true,
    providerRevocationConfirmed: true,
  });
  assertEquals(f.oauth.getSession(first.id), undefined);
  assertEquals(f.requests.at(-1)?.params.get("token"), SECRET + "-rotated");
});

Deno.test("OAuth refreshed account switch invalidates session and discards new tokens", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const first = await f.oauth.completeAuthorization(flow.callback);
  f.claims.sub = "another-subject";
  const error = await assertRejects(() => f.oauth.refreshSession(first.id));
  assert(!String(error).includes(SECRET));
  assertEquals(f.oauth.getSession(first.id), undefined);
  assert(f.requests.some((r) => r.url === REVOKE));
});

Deno.test("OAuth missing fresh refresh ID token cannot reuse cached Workspace identity", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const first = await f.oauth.completeAuthorization(flow.callback);
  f.responsePatch.id_token = undefined;
  await assertRejects(() => f.oauth.refreshSession(first.id));
  assertEquals(f.oauth.getSession(first.id), undefined);
});

Deno.test("OAuth provider expiry is required and no default one-hour lease is invented", async () => {
  const f = await fixture();
  f.responsePatch.expires_in = undefined;
  const flow = await f.begin();
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assert(f.requests.some((r) => r.url === REVOKE));
});

Deno.test("OAuth secret-bearing config is captured once and never displayed", async () => {
  const f = await fixture({ clientSecret: SECRET });
  const flow = await f.begin();
  await f.oauth.completeAuthorization(flow.callback);
  assertEquals(f.requests[0].params.get("client_secret"), SECRET);
  assert(!JSON.stringify(f.oauth).includes(SECRET));
});

for (
  const patch of [
    { clientId: "" },
    { clientSecret: "" },
    { maxPending: 0 },
    { maxSessions: -1 },
    { authorizationTtlMs: NaN },
    { redirectUri: "http://evil.app/callback" },
    { redirectUri: "https://host.app/callback?query=true" },
    { redirectUri: "https://host.app/#fragment" },
  ]
) {
  Deno.test(
    "OAuth malformed config fails before I/O " + JSON.stringify(patch),
    () => {
      assertThrows(() =>
        new WorkspaceOAuthClient({
          clientId: "id",
          redirectUri: REDIRECT,
          authorizationTtlMs: 100,
          maxPending: 1,
          maxSessions: 1,
          ...patch,
        }), WorkspaceOAuthError);
    },
  );
}

Deno.test("OAuth accessor config is never invoked", () => {
  let read = false;
  const config = {
    get clientId() {
      read = true;
      return SECRET;
    },
    redirectUri: REDIRECT,
    authorizationTtlMs: 100,
    maxPending: 1,
    maxSessions: 1,
  };
  assertThrows(() => new WorkspaceOAuthClient(config), WorkspaceOAuthError);
  assertEquals(read, false);
});
Deno.test("OAuth cancellation during code exchange rejects late session installation and revokes issued credential", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const release = f.block();
  const pending = f.oauth.completeAuthorization(flow.callback);
  f.oauth.clearPending();
  release();
  await assertRejects(() => pending, WorkspaceOAuthError);
  assert(f.requests.some((r) => r.url === REVOKE));
});
Deno.test("OAuth expiry during exchange does not extend the state deadline", async () => {
  const f = await fixture({ authorizationTtlMs: 10 });
  const flow = await f.begin();
  const release = f.block();
  const pending = f.oauth.completeAuthorization(flow.callback);
  f.advance(11);
  release();
  await assertRejects(() => pending, WorkspaceOAuthError);
  assert(f.requests.some((r) => r.url === REVOKE));
});
Deno.test("OAuth token endpoint redirect is rejected without a credential-bearing follow", async () => {
  const f = await fixture();
  f.flags.redirectToken = true;
  const flow = await f.begin();
  await assertRejects(
    () => f.oauth.completeAuthorization(flow.callback),
    WorkspaceOAuthError,
  );
  assertEquals(f.requests.length, 1);
  assertEquals(f.requests[0].url, TOKEN);
  assertEquals(f.requests[0].redirect, "manual");
});
Deno.test("OAuth absent refresh rotation preserves the existing credential", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const session = await f.oauth.completeAuthorization(flow.callback);
  f.responsePatch.refresh_token = undefined;
  await f.oauth.refreshSession(session.id);
  await f.oauth.refreshSession(session.id);
  const grants = f.requests.filter((r) =>
    r.params.get("grant_type") === "refresh_token"
  );
  assertEquals(grants.length, 2);
  assertEquals(grants[1].params.get("refresh_token"), SECRET + "-refresh");
});
Deno.test("OAuth missing refresh credential must not send access token as a refresh token", async () => {
  const f = await fixture();
  f.responsePatch.refresh_token = undefined;
  const flow = await f.begin();
  const session = await f.oauth.completeAuthorization(flow.callback);
  await assertRejects(() => f.oauth.refreshSession(session.id));
  assertEquals(
    f.requests.filter((r) => r.params.get("grant_type") === "refresh_token")
      .length,
    0,
  );
  assertEquals(f.oauth.getSession(session.id), undefined);
});
Deno.test("OAuth logged-out pending refresh still consumes bounded session capacity", async () => {
  const f = await fixture({ maxSessions: 1 });
  const first = await f.begin();
  const session = await f.oauth.completeAuthorization(first.callback);
  const release = f.block("refresh_token");
  const outcome = f.oauth.refreshSession(session.id).then(
    () => true,
    () => false,
  );
  await release.entered;
  try {
    assertEquals(
      (await f.oauth.logout(session.id)).providerRevocationConfirmed,
      false,
    );
    const second = await f.begin();
    await assertRejects(
      () => f.oauth.completeAuthorization(second.callback),
      WorkspaceOAuthError,
    );
  } finally {
    release();
    await outcome;
  }
});
Deno.test("OAuth valid fresh renamed identity rejected by session binding discards its new provider credential", async () => {
  const f = await fixture();
  const flow = await f.begin();
  const session = await f.oauth.completeAuthorization(flow.callback);
  f.claims.email = "renamed@floorp.app";
  await assertRejects(() => f.oauth.refreshSession(session.id));
  assertEquals(f.oauth.getSession(session.id), undefined);
  assert(
    f.requests.some((r) =>
      r.url === REVOKE && r.params.get("token") === SECRET + "-rotated"
    ),
  );
});
