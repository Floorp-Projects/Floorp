// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertFalse } from "@std/assert";
import {
  buildActArguments,
  buildEventPayload,
  formatActCommand,
  resolvePlatforms,
} from "./run-package-workflow.ts";

Deno.test("act secret is passed by environment name, never by value", () => {
  const sentinel = "NOT_A_REAL_TOKEN_ARG_TEST";
  const args = buildActArguments({
    workflowPath: ".github/workflows/package.yml",
    payloadPath: "event.json",
    jobName: "main",
    platformImage: "ubuntu-22.04=image",
    extraArgs: ["--secret", `UNRELATED=${sentinel}`],
  });

  assertEquals(args.slice(8, 10), ["-s", "GITHUB_TOKEN"]);
  assertFalse(args.some((argument) => argument.startsWith("GITHUB_TOKEN=")));

  const displayed = formatActCommand(args, 2);
  assertFalse(displayed.includes(sentinel));
  assert(displayed.includes("[2 additional argument(s) omitted]"));
});

Deno.test("default platform and help implementation stay aligned", () => {
  assertEquals(resolvePlatforms(), ["Linux-x64"]);
  assertEquals(resolvePlatforms("Linux-x64,Linux-aarch64,Linux-x64"), [
    "Linux-x64",
    "Linux-aarch64",
  ]);
});

Deno.test("legacy act event contains no credentials", () => {
  const payload = buildEventPayload({
    ref: "main",
    platform: "Linux-x64",
    beta: false,
    skipSigning: true,
    runtimeRunId: "1234",
  });
  assertEquals(payload, {
    ref: "main",
    inputs: {
      platform: "Linux-x64",
      beta: "false",
      runtime_artifact_workflow_run_id: "1234",
      skip_signing: "true",
    },
  });
  assertFalse(JSON.stringify(payload).toLowerCase().includes("token"));
});
