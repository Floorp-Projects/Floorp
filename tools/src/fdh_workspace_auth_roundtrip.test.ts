// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { calculatePKCECodeChallenge } from "openid-client";
import { createLocalJWKSet, exportJWK, generateKeyPair, SignJWT } from "jose";
import {
  WorkspaceOAuthClient,
  WorkspaceOAuthError,
} from "./fdh_workspace_oauth.ts";
import { runWorkspaceAuthProbe } from "./fdh_workspace_auth_cli_runtime.ts";
import { runWorkspaceAuthCli } from "./fdh_workspace_auth_cli.ts";
import type { WorkspaceOAuthTestRequest } from "./fdh_workspace_oauth_types.ts";
import type { WorkspaceAuthProbeDependencies } from "./fdh_workspace_auth_cli_types.ts";

const REDIRECT = "http://127.0.0.1:19281/oauth/callback";
const CLIENT = "offline-roundtrip-client";
const TOKEN = "https://oauth2.googleapis.com/token";
const REVOKE = "https://oauth2.googleapis.com/revoke";
const ACCESS = "synthetic-google-access-private-material";
const REFRESH = "synthetic-google-refresh-private-material";
const ROTATED = "synthetic-google-rotated-private-material";

/** Real OAuth client and mature protocol, synthetic provider and callback socket. */
async function harness() {
  const keys = await generateKeyPair("RS256", { extractable: true });
  const jwk = {
    ...await exportJWK(keys.publicKey),
    kid: "offline-roundtrip-rs256",
    alg: "RS256",
  };
  const requests: WorkspaceOAuthTestRequest[] = [];
  const callbacks: Promise<Response>[] = [];
  const flags = { wrongState: false };
  let authorization: URL | undefined;
  let handle = (_request: Request): Promise<Response> =>
    Promise.reject(new Error("Callback not bound"));
  let shutdowns = 0;
  let announced = 0;
  const client = new WorkspaceOAuthClient({
    clientId: CLIENT,
    redirectUri: REDIRECT,
    authorizationTtlMs: 3000,
    maxPending: 1,
    maxSessions: 1,
  }, {
    oidc: { resolveKey: createLocalJWKSet({ keys: [jwk] }) },
    fetch: async (url, init) => {
      const params = new URLSearchParams(String(init.body));
      requests.push({ url, params, redirect: init.redirect });
      assertEquals(init.redirect, "manual");
      if (url === REVOKE) {
        assertEquals(params.get("token"), ROTATED);
        return new Response("", { status: 200 });
      }
      assertEquals(url, TOKEN);
      assertEquals(params.get("client_id"), CLIENT);
      assert(authorization !== undefined);
      const code = params.get("grant_type") === "authorization_code";
      if (code) {
        assertEquals(params.get("code"), "synthetic-code");
        assertEquals(params.get("redirect_uri"), REDIRECT);
        assertEquals(
          await calculatePKCECodeChallenge(params.get("code_verifier") ?? ""),
          authorization.searchParams.get("code_challenge"),
        );
        assertEquals(
          authorization.searchParams.get("code_challenge_method"),
          "S256",
        );
      } else {
        assertEquals(params.get("grant_type"), "refresh_token");
        assertEquals(params.get("refresh_token"), REFRESH);
      }
      const seconds = Math.floor(Date.now() / 1000);
      const claims = {
        iss: "https://accounts.google.com",
        aud: CLIENT,
        sub: "google-subject",
        email: "roundtrip@floorp.app",
        hd: "floorp.app",
        email_verified: true,
        iat: seconds - 1,
        exp: seconds + 3600,
        ...(code ? { nonce: authorization.searchParams.get("nonce") } : {}),
      };
      if (code) assert((claims.nonce?.length ?? 0) >= 43);
      else assertEquals("nonce" in claims, false);
      const idToken = await new SignJWT(claims).setProtectedHeader({
        alg: "RS256",
        kid: jwk.kid,
      }).sign(keys.privateKey);
      return Response.json({
        access_token: ACCESS,
        refresh_token: code ? REFRESH : ROTATED,
        id_token: idToken,
        token_type: "Bearer",
        expires_in: 3600,
      });
    },
  });
  const dependencies: WorkspaceAuthProbeDependencies = {
    client,
    serve(options, handler) {
      assertEquals(options.hostname, "127.0.0.1");
      assertEquals(options.port, 19281);
      handle = handler;
      return {
        shutdown() {
          shutdowns++;
          return Promise.resolve();
        },
      };
    },
    announce(url) {
      announced++;
      authorization = new URL(url);
      assertEquals(authorization.origin, "https://accounts.google.com");
      const state = flags.wrongState
        ? "wrong-state"
        : authorization.searchParams.get("state");
      callbacks.push(
        handle(
          new Request(REDIRECT + "?state=" + state + "&code=synthetic-code"),
        ),
      );
    },
  };
  return {
    client,
    flags,
    requests,
    run: (timeoutMs = 3000) =>
      runWorkspaceAuthProbe({
        clientId: CLIENT,
        redirectUri: REDIRECT,
        timeoutMs,
      }, dependencies),
    async drain() {
      const statuses: number[] = [];
      for (const response of await Promise.all(callbacks)) {
        statuses.push(response.status);
        assertEquals(response.headers.get("cache-control"), "no-store");
        assertEquals(response.headers.get("referrer-policy"), "no-referrer");
        await response.text();
      }
      return statuses;
    },
    shutdowns: () => shutdowns,
    announced: () => announced,
  };
}

Deno.test("real-client probe roundtrip executes code/PKCE/nonce then nonce-free fresh refresh and rotated revoke", async () => {
  const f = await harness();
  const result = await f.run();
  assertEquals(await f.drain(), [200]);
  assertEquals(f.requests.map((r) => r.url), [TOKEN, TOKEN, REVOKE]);
  assertEquals(f.requests.slice(0, 2).map((r) => r.params.get("grant_type")), [
    "authorization_code",
    "refresh_token",
  ]);
  assertEquals(result.identity.subject, "google-subject");
  assertEquals(result.identity.hostedDomain, "floorp.app");
  assertEquals(result.providerRevocationConfirmed, true);
  assertEquals(result.codeExchangeVerified, true);
  assertEquals(result.refreshVerified, true);
  assertEquals(result.mcpAuthorizationGranted, false);
  assertEquals(result.ac05Qualified, false);
  const json = JSON.stringify(result);
  for (const secret of [ACCESS, REFRESH, ROTATED, "synthetic-code"]) {
    assert(!json.includes(secret));
  }
  assertEquals("id" in result, false);
  assertEquals(JSON.stringify(f.client), "{}");
  assertEquals(f.announced(), 1);
  assertEquals(f.shutdowns(), 1);
});

Deno.test("real-client probe wrong state never exchanges a code and closes on bounded timeout", async () => {
  const f = await harness();
  f.flags.wrongState = true;
  const error = await assertRejects(() => f.run(30), WorkspaceOAuthError);
  assert(!String(error).includes(ACCESS));
  assertEquals(await f.drain(), [400]);
  assertEquals(f.requests.length, 0);
  assertEquals(f.shutdowns(), 1);
});

Deno.test("fdh raw auth dispatch is lazy and precedes generic argv parsing", async () => {
  const source = await Deno.readTextFile(new URL("../fdh.ts", import.meta.url));
  const main = source.indexOf("async function main(");
  assert(main >= 0);
  const auth = source.indexOf('if (argv[0] === "auth")', main);
  const generic = source.indexOf("parseArgv(argv)", main);
  assert(auth > main && generic > auth);
  const intercept = source.slice(auth, generic);
  assert(intercept.includes("await import("));
  assert(intercept.includes('"./src/fdh_workspace_auth_cli.ts"'));
  assertEquals(
    source.slice(0, main).includes('from "./src/fdh_workspace_auth_cli.ts"'),
    false,
  );
});

Deno.test("auth CLI help and rejected argv do not import runtime or call provider/env adapters", async () => {
  const source = await Deno.readTextFile(
    new URL("./fdh_workspace_auth_cli.ts", import.meta.url),
  );
  const help = source.indexOf("return { exitCode: 0, stdout: HELP }");
  const runtime = source.indexOf(
    'import("./fdh_workspace_auth_cli_runtime.ts")',
  );
  assert(help >= 0 && runtime > help);
  let effects = 0;
  for (
    const argv of [["auth", "--help"], [
      "auth",
      "probe",
      "--client-secret-env",
      "ENV",
      "--unknown",
      "x",
    ]]
  ) {
    const result = await runWorkspaceAuthCli(argv, {
      readEnv() {
        effects++;
        throw new Error(ACCESS);
      },
      probe() {
        effects++;
        return Promise.reject(new Error(ACCESS));
      },
    });
    assertEquals(result.exitCode, argv[1] === "--help" ? 0 : 1);
    assert(!result.stdout.includes(ACCESS));
  }
  assertEquals(effects, 0);
});
