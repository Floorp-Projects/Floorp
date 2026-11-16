// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { runWorkspaceAuthCli } from "./fdh_workspace_auth_cli.ts";
import type { WorkspaceAuthProbeResult } from "./fdh_workspace_auth_cli_types.ts";
const SECRET = "synthetic-secret-not-a-real-credential";
const args = [
  "auth",
  "probe",
  "--client-id",
  "offline-client",
  "--redirect-uri",
  "http://127.0.0.1:19281/oauth/callback",
];
const result: WorkspaceAuthProbeResult = {
  schemaVersion: 1,
  scope: "workspace-oauth-probe",
  codeExchangeVerified: true,
  refreshVerified: true,
  localLogout: true,
  providerRevocationConfirmed: true,
  identity: {
    subject: "offline-sub",
    email: "test@floorp.app",
    hostedDomain: "floorp.app",
    issuer: "https://accounts.google.com",
  },
  mcpAuthorizationGranted: false,
  ac05Qualified: false,
};
for (
  const input of [["auth"], ["auth", "--help"], ["auth", "probe", "--help"]]
) {
  Deno.test("auth help is effect-free " + input.join(" "), async () => {
    const output = await runWorkspaceAuthCli(input, {
      readEnv() {
        throw new Error(SECRET);
      },
      probe() {
        throw new Error(SECRET);
      },
    });
    assertEquals(output.exitCode, 0);
    assert(output.stdout.includes("probe"));
    assert(!output.stdout.includes(SECRET));
  });
}
for (
  const input of [
    [],
    ["other"],
    ["auth", "login"],
    ["auth", "probe"],
    [...args, "--unknown", "x"],
    [...args, "--client-id", "other"],
    [...args, "--client-secret", SECRET],
    [...args, "--client-secret-env", "lowercase"],
    [...args, "--timeout-ms", ""],
    [...args, "--timeout-ms", "0"],
    [...args, "--timeout-ms", "900001"],
    [...args, "--timeout-ms", "01"],
    [...args, "--timeout-ms", "--help"],
    [...args, "--help"],
    [...args, "positional"],
    [...args, "--client-secret-env", "ENV", "--unexpected", "x"],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "http://localhost:19281/oauth/callback",
    ],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "http://127.0.0.1:80/oauth/callback",
    ],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "http://127.0.0.1:19281/other",
    ],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "http://127.0.0.1:19281/oauth/callback?x=1",
    ],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "http://user:password@127.0.0.1:19281/oauth/callback",
    ],
    [
      "auth",
      "probe",
      "--client-id",
      "id",
      "--redirect-uri",
      "https://evil.app/oauth/callback",
    ],
  ]
) {
  Deno.test(
    "auth malformed argv rejects before any I/O " +
      JSON.stringify(input).replaceAll(SECRET, "redacted"),
    async () => {
      let calls = 0;
      const output = await runWorkspaceAuthCli(input, {
        readEnv() {
          calls++;
          throw new Error(SECRET);
        },
        probe() {
          calls++;
          throw new Error(SECRET);
        },
      });
      assertEquals(output.exitCode, 1);
      assertEquals(calls, 0);
      assert(!output.stdout.includes(SECRET));
    },
  );
}
Deno.test("auth valid probe sends only validated configuration and reports no provider secrets", async () => {
  let called = 0;
  const output = await runWorkspaceAuthCli(args, {
    readEnv() {
      throw new Error();
    },
    probe(options) {
      called++;
      assert(Object.isFrozen(options));
      assertEquals(options.timeoutMs, 300000);
      assertEquals(options.clientId, "offline-client");
      return Promise.resolve({
        ...result,
        access_token: SECRET,
        refresh_token: SECRET,
        id: SECRET,
      });
    },
  });
  assertEquals(called, 1);
  assertEquals(output.exitCode, 0);
  assert(!output.stdout.includes(SECRET));
  assertEquals(JSON.parse(output.stdout).ac05Qualified, false);
  assertEquals(JSON.parse(output.stdout).mcpAuthorizationGranted, false);
});
Deno.test("auth reads exactly the explicitly selected secret env and never echoes it", async () => {
  const read: string[] = [];
  const output = await runWorkspaceAuthCli([
    ...args,
    "--client-secret-env",
    "FDH_GOOGLE_CLIENT_SECRET",
  ], {
    readEnv(name) {
      read.push(name);
      return SECRET;
    },
    probe(options) {
      assertEquals(options.clientSecret, SECRET);
      return Promise.resolve(result);
    },
  });
  assertEquals(read, ["FDH_GOOGLE_CLIENT_SECRET"]);
  assertEquals(output.exitCode, 0);
  assert(!output.stdout.includes(SECRET));
});
for (const secret of [undefined, "", "bad\nvalue"]) {
  Deno.test(
    "auth rejects missing or malformed selected secret " + String(secret),
    async () => {
      let called = false;
      const output = await runWorkspaceAuthCli([
        ...args,
        "--client-secret-env",
        "ENV",
      ], {
        readEnv() {
          return secret;
        },
        probe() {
          called = true;
          return Promise.resolve(result);
        },
      });
      assertEquals(output.exitCode, 1);
      assertEquals(called, false);
    },
  );
}
Deno.test("auth transport/environment errors cannot leak credentials or cause", async () => {
  for (
    const dependencies of [{
      readEnv() {
        return SECRET;
      },
      probe() {
        return Promise.reject(new Error(SECRET));
      },
    }, {
      readEnv() {
        throw new Error(SECRET);
      },
    }]
  ) {
    const output = await runWorkspaceAuthCli([
      ...args,
      "--client-secret-env",
      "ENV",
    ], dependencies);
    assertEquals(output.exitCode, 1);
    assert(!output.stdout.includes(SECRET));
  }
});
Deno.test("auth probe revocation uncertainty returns a failing exit without leaking provider state", async () => {
  const output = await runWorkspaceAuthCli(args, {
    probe() {
      return Promise.resolve({ ...result, providerRevocationConfirmed: false });
    },
  });
  assertEquals(output.exitCode, 1);
  assertEquals(JSON.parse(output.stdout).providerRevocationConfirmed, false);
});
