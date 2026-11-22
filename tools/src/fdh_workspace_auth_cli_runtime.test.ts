// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import { runWorkspaceAuthProbe } from "./fdh_workspace_auth_cli_runtime.ts";
import { WorkspaceOAuthError } from "./fdh_workspace_oauth.ts";
import type { WorkspaceAuthProbeDependencies } from "./fdh_workspace_auth_cli_types.ts";
const REDIRECT = "http://127.0.0.1:19281/oauth/callback";
const SECRET = "synthetic-raw-provider-credential";
function snapshot() {
  return {
    id: "opaque-session",
    identity: {
      scope: "workspace-id-token-verification-only",
      liveOAuthVerified: false,
      mcpAuthorizationGranted: false,
      identityKey: "subject",
      email: "test@floorp.app",
      hostedDomain: "floorp.app",
      issuer: "https://accounts.google.com",
      audience: "client",
      issuedAt: 1800000000,
      expiresAt: 1800003600,
    },
    expiresAt: Date.now() + 3600000,
    version: 1,
    scope: "workspace-oauth-session",
    liveOAuthVerified: false,
    mcpAuthorizationGranted: false,
  } as const;
}
function harness() {
  const calls: string[] = [];
  const responses: Response[] = [];
  const flags = {
    bindError: false,
    callbackError: false,
    refreshError: false,
    announceError: false,
  };
  let handle = (_request: Request) =>
    Promise.resolve(new Response("not-bound", { status: 503 }));
  let signal: AbortSignal | undefined;
  let delivered: Promise<void> = Promise.resolve();
  let announceAction = async () => {
    responses.push(
      await handle(new Request(REDIRECT + "?state=state&code=code")),
    );
  };
  const client: NonNullable<WorkspaceAuthProbeDependencies["client"]> = {
    beginAuthorization() {
      calls.push("begin");
      return Promise.resolve({
        authorizationUrl:
          "https://accounts.google.com/o/oauth2/v2/auth?state=state",
        expiresAt: Date.now() + 1000,
      });
    },
    completeAuthorization() {
      calls.push("callback");
      return flags.callbackError
        ? Promise.reject(new Error(SECRET))
        : Promise.resolve(snapshot());
    },
    refreshSession() {
      calls.push("refresh");
      return flags.refreshError
        ? Promise.reject(new Error(SECRET))
        : Promise.resolve(snapshot());
    },
    logout() {
      calls.push("logout");
      return Promise.resolve({
        localLogout: true as const,
        providerRevocationConfirmed: true,
      });
    },
    clearPending() {
      calls.push("clear");
    },
  };
  const dependencies: WorkspaceAuthProbeDependencies = {
    client,
    serve(options, handler) {
      calls.push("serve");
      assertEquals(options.hostname, "127.0.0.1");
      assertEquals(options.port, 19281);
      if (flags.bindError) throw new Error(SECRET);
      signal = options.signal;
      handle = handler;
      return {
        shutdown() {
          calls.push("shutdown");
          return Promise.resolve();
        },
      };
    },
    announce(url) {
      calls.push("announce");
      assert(url.startsWith("https://accounts.google.com/"));
      if (flags.announceError) throw new Error(SECRET);
      delivered = announceAction();
    },
  };
  const run = (timeoutMs = 1000) =>
    runWorkspaceAuthProbe({
      clientId: "client",
      redirectUri: REDIRECT,
      timeoutMs,
    }, dependencies);
  return {
    calls,
    flags,
    responses,
    run,
    client,
    async drain() {
      await delivered;
    },
    setAction(
      action: (send: (request: Request) => Promise<Response>) => Promise<void>,
    ) {
      announceAction = () => action((request) => handle(request));
    },
    aborted() {
      return signal?.aborted;
    },
  };
}
Deno.test("probe runtime binds loopback only and runs callback/refresh/logout then tears down", async () => {
  const f = harness();
  const result = await f.run();
  await f.drain();
  assertEquals(f.calls, [
    "begin",
    "serve",
    "announce",
    "callback",
    "refresh",
    "logout",
    "clear",
    "shutdown",
  ]);
  assertEquals(result.refreshVerified, true);
  assertEquals(result.ac05Qualified, false);
  assertEquals(result.mcpAuthorizationGranted, false);
  assert(!JSON.stringify(result).includes("opaque-session"));
  assert(!JSON.stringify(result).includes(SECRET));
  assertEquals(f.responses[0].headers.get("cache-control"), "no-store");
  assertEquals(f.responses[0].headers.get("referrer-policy"), "no-referrer");
  assert(!(await f.responses[0].text()).includes("test@floorp.app"));
});
Deno.test("probe runtime rejects wrong origin/path/method/state without consuming valid callback", async () => {
  const f = harness();
  const statuses: number[] = [];
  f.setAction(async (send) => {
    for (
      const req of [
        new Request("http://evil.app/oauth/callback?state=state"),
        new Request(REDIRECT.replace("callback", "other") + "?state=state"),
        new Request(REDIRECT + "?state=state", { method: "POST" }),
        new Request(REDIRECT + "?state=other"),
        new Request(REDIRECT + "?state=state&state=state"),
      ]
    ) {
      const res = await send(req);
      statuses.push(res.status);
      await res.text();
    }
    const res = await send(new Request(REDIRECT + "?state=state&code=code"));
    await res.text();
    const duplicate = await send(
      new Request(REDIRECT + "?state=state&code=code"),
    );
    statuses.push(duplicate.status);
    await duplicate.text();
  });
  await f.run();
  await f.drain();
  assertEquals(statuses, [404, 404, 405, 400, 400, 400]);
  assertEquals(f.calls.filter((c) => c === "callback").length, 1);
});
Deno.test("probe runtime timeout clears pending state, aborts callback server and leaves no session", async () => {
  const f = harness();
  f.setAction(async () => {});
  await assertRejects(() => f.run(5), WorkspaceOAuthError);
  await f.drain();
  assertEquals(f.aborted(), true);
  assertEquals(f.calls.filter((c) => c === "shutdown").length, 1);
  assertEquals(f.calls.includes("refresh"), false);
  assertEquals(f.calls.includes("logout"), false);
  assert(f.calls.includes("clear"));
});
for (
  const key of [
    "bindError",
    "callbackError",
    "refreshError",
    "announceError",
  ] as const
) {
  Deno.test("probe runtime cleans up and redacts failure " + key, async () => {
    const f = harness();
    f.flags[key] = true;
    const error = await assertRejects(() => f.run(), WorkspaceOAuthError);
    await f.drain();
    assert(!String(error).includes(SECRET));
    assert(f.calls.includes("clear"));
    assertEquals(
      f.calls.filter((c) => c === "shutdown").length,
      key === "bindError" ? 0 : 1,
    );
    if (key === "refreshError") assert(f.calls.includes("logout"));
    for (const res of f.responses) assert(!(await res.text()).includes(SECRET));
  });
}
for (
  const redirectUri of [
    "http://localhost:19281/oauth/callback",
    "http://127.0.0.1:80/oauth/callback",
    "https://evil.app/oauth/callback",
    "http://127.0.0.1:19281/other",
  ]
) {
  Deno.test(
    "probe runtime independently enforces its loopback policy " + redirectUri,
    async () => {
      let effects = 0;
      await assertRejects(() =>
        runWorkspaceAuthProbe(
          { clientId: "client", redirectUri, timeoutMs: 1 },
          {
            serve() {
              effects++;
              throw new Error();
            },
            announce() {
              effects++;
            },
          },
        ), WorkspaceOAuthError);
      assertEquals(effects, 0);
    },
  );
}
