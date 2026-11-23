// SPDX-License-Identifier: MPL-2.0
import { assertEquals } from "@std/assert";
import {
  parseWorkspaceSpikeArgs,
  runWorkspaceSpikeCli,
} from "../integration/fdh_workspace_mcp_spike.ts";
const valid = [
  "--execute-google-workspace",
  "--task-id",
  "task-probe",
  "--client-id",
  "123-example.apps.googleusercontent.com",
  "--client-secret-env",
  "FDH_GOOGLE_CLIENT_SECRET",
];
Deno.test("workspace CLI accepts named secret environment and bounded timeout", () => {
  assertEquals(parseWorkspaceSpikeArgs(valid), {
    taskId: "task-probe",
    clientId: "123-example.apps.googleusercontent.com",
    clientSecretEnv: "FDH_GOOGLE_CLIENT_SECRET",
    timeoutMs: 900000,
  });
  assertEquals(
    parseWorkspaceSpikeArgs([...valid, "--timeout-ms", "1000"])?.timeoutMs,
    1000,
  );
});
for (
  const args of [
    [],
    ["--help"],
    ["--execute-protocol-only"],
    valid.slice(0, -1),
    [...valid, "--client-secret", "literal-secret"],
    [...valid, "--timeout-ms", "900001"],
    [...valid, "--timeout-ms", "0"],
    [...valid, "--timeout-ms", "999"],
    [...valid, "--timeout-ms", "1e5"],
    [...valid, "--timeout-ms", " 1000"],
    [...valid, "--timeout-ms=1000"],
    [...valid, "--task-id", "other"],
    valid.map((x) => x === "task-probe" ? "../other" : x),
    valid.map((x) => x === "FDH_GOOGLE_CLIENT_SECRET" ? "OTHER_ENV" : x),
    valid.map((x) =>
      x === "123-example.apps.googleusercontent.com"
        ? "https://evil.example"
        : x
    ),
    [...valid, "--unknown", "x"],
  ]
) {
  Deno.test(
    "workspace CLI rejects invalid raw argument case " +
      JSON.stringify(args.map((x) => x === "literal-secret" ? "REDACTED" : x)),
    async () => {
      let executions = 0, reports = 0, errors = 0;
      const code = await runWorkspaceSpikeCli(args, {
        execute: () => {
          executions++;
          throw Error("must not execute");
        },
        report: () => {
          reports++;
        },
        error: () => {
          errors++;
        },
      });
      assertEquals([code, executions, reports, errors], [2, 0, 0, 1]);
    },
  );
}
Deno.test("workspace CLI catches secret-bearing exceptions without exposing them", async () => {
  let errors = 0, reports = 0;
  const code = await runWorkspaceSpikeCli(valid, {
    execute: () => {
      throw Error("synthetic-secret-not-for-output");
    },
    report: () => {
      reports++;
    },
    error: () => {
      errors++;
    },
  });
  assertEquals([code, reports, errors], [1, 0, 1]);
});
