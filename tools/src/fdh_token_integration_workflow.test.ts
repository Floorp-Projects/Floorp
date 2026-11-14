// SPDX-License-Identifier: MPL-2.0
//
// Static contract checks for the read-only token-adapter CI job in
// colocated_runner_test.yml. These are text assertions, not a YAML parser:
// they cannot prove GitHub runtime behavior. The job must be observed on
// GitHub Actions.

import { assertEquals, assertStringIncludes } from "@std/assert";

const WORKFLOW = new URL(
  "../../.github/workflows/colocated_runner_test.yml",
  import.meta.url,
);

async function readWorkflow(): Promise<string> {
  const text = await Deno.readTextFile(WORKFLOW);
  return text.replace(/\r\n/g, "\n");
}

/** Return the text of one job declared at the two-space jobs indent. */
function jobBlock(workflow: string, jobId: string): string {
  const lines = workflow.split("\n");
  const start = lines.indexOf(`  ${jobId}:`);
  if (start < 0) throw new Error(`missing job: ${jobId}`);
  let end = lines.length;
  for (let index = start + 1; index < lines.length; index += 1) {
    if (/^ {2}[A-Za-z0-9_-]+:\s*$/.test(lines[index])) {
      end = index;
      break;
    }
  }
  return lines.slice(start, end).join("\n");
}

Deno.test("token-adapter-contract job is read-only with no secrets", async () => {
  const wf = await readWorkflow();
  const job = jobBlock(wf, "token-adapter-contract");
  assertStringIncludes(job, "runs-on: ubuntu-24.04");
  assertStringIncludes(wf, "permissions:\n  contents: read");
  assertEquals(job.includes("contents: write"), false);
  assertEquals(job.includes("id-token"), false);
  assertEquals(job.includes("secrets."), false);
  assertEquals(job.includes("GH_TOKEN"), false);
  assertEquals(job.includes("pull_request_target"), false);
  assertStringIncludes(job, "persist-credentials: false");
});

Deno.test("token-adapter-contract builds the adapter before the test runs", async () => {
  const wf = await readWorkflow();
  const job = jobBlock(wf, "token-adapter-contract");
  const buildIndex = job.indexOf("deno task build");
  const testIndex = job.indexOf("tools/integration/os-server-token.test.ts");
  assertEquals(buildIndex >= 0 && testIndex > buildIndex, true);
  assertStringIncludes(job, "working-directory: bridge/loader-modules");
  assertStringIncludes(job, "deno install --frozen");
});

Deno.test("token-adapter-contract runs both built-adapter and host token suites", async () => {
  const wf = await readWorkflow();
  const job = jobBlock(wf, "token-adapter-contract");
  assertStringIncludes(job, "deno test --allow-read --allow-write");
  assertStringIncludes(job, "tools/integration/os-server-token.test.ts");
  assertStringIncludes(job, "tools/src/os_server_token.test.ts");
  for (
    const forbidden of [
      "package",
      "sign",
      "publish",
      "release",
      "feles-build",
      "firefox",
    ]
  ) {
    assertEquals(job.toLowerCase().includes(forbidden), false, forbidden);
  }
});

Deno.test("existing required jobs remain declared unchanged", async () => {
  const wf = await readWorkflow();
  for (
    const id of [
      "workspace-development",
      "workspace-extended-development",
      "workspace-restart-development",
      "smoke",
      "native-runtime",
      "token-adapter-contract",
    ]
  ) {
    assertStringIncludes(wf, `  ${id}:`);
  }
  assertStringIncludes(
    jobBlock(wf, "native-runtime"),
    "name: Native Runtime / ${{ matrix.runner }}",
  );
  assertStringIncludes(jobBlock(wf, "native-runtime"), "needs: smoke");
});

Deno.test("pull_request trigger keeps no path filter", async () => {
  const wf = await readWorkflow();
  // Unfiltered so adapter, shared, test, and type changes always reach the
  // job; a path filter here would also gate the required jobs.
  assertStringIncludes(wf, "  pull_request:\n");
  assertEquals(wf.includes("paths:"), false);
});
