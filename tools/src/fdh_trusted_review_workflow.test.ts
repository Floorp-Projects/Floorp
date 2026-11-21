// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";

const WORKFLOW = new URL(
  "../../.github/workflows/fdh-trusted-review-gate.yml",
  import.meta.url,
);
const CANDIDATE_WORKFLOW = new URL(
  "../../.github/workflows/fdh-review-gate.yml",
  import.meta.url,
);
const RUNNER = new URL("./fdh_trusted_review_runner.ts", import.meta.url);

Deno.test("FDH trusted receipt workflow is base-trusted and does not execute candidate code", async () => {
  const workflow = await Deno.readTextFile(WORKFLOW);
  assertStringIncludes(workflow, "pull_request_target:");
  assertStringIncludes(workflow, "issue_comment:");
  assertStringIncludes(
    workflow,
    "ref: ${{ github.workflow_sha }}",
  );
  assertStringIncludes(workflow, "branches: [main]");
  assertStringIncludes(workflow, "persist-credentials: false");
  assertStringIncludes(workflow, "checks: write");
  assertStringIncludes(
    workflow,
    "actions/github-script@f28e40c7f34bde8b3046d885e986cb6290c5673b",
  );
  assertStringIncludes(workflow, 'name: "fdh-trusted-receipts"');
  assertStringIncludes(workflow, "head_sha: headSha");
  assertStringIncludes(workflow, "check_run_id: pending.id");
  assertStringIncludes(workflow, "name: trusted-receipt-publication");
  assertStringIncludes(
    workflow,
    'core.setFailed("Trusted receipt validation failed")',
  );
  assertStringIncludes(
    workflow,
    "const { data: pending } = await github.rest.checks.create",
  );
  assertStringIncludes(workflow, "types: [created, edited, deleted]");
  assertStringIncludes(
    workflow,
    "contains(github.event.changes.body.from, 'fdh-review-receipt:v1')",
  );
  assertStringIncludes(workflow, "event=pull_request_target&head_sha=");
  assertStringIncludes(
    workflow,
    ".display_title == $title and .head_sha == $sha",
  );
  assertStringIncludes(workflow, 'gh run rerun "$run_id"');
  assertStringIncludes(workflow, "github.rest.pulls.get");
  assertStringIncludes(
    workflow,
    "github.rest.repos.getCollaboratorPermissionLevel",
  );
  assertStringIncludes(workflow, "currentHeadSha: headSha");
  assertStringIncludes(
    workflow,
    "denoland/setup-deno@667a34cdef165d8d2b2e98dde39547c9daac7282",
  );
  assertStringIncludes(workflow, "env: childEnv");
  assertStringIncludes(
    workflow,
    'const childEnv = { PATH: process.env.PATH, DENO_NO_UPDATE_CHECK: "1" }',
  );
  assertEquals(workflow.includes("GITHUB_TOKEN: \${{ github.token }}"), false);
  assertEquals(
    workflow.split("  trusted-receipts:")[1].includes("GH_TOKEN"),
    false,
  );
  assertEquals(workflow.includes("secrets."), false);
  assertStringIncludes(workflow, '"run", "--no-config"');
  assertStringIncludes(workflow, "latest.head?.sha !== headSha");
  assertStringIncludes(workflow, 'spawn("deno"');
  assertStringIncludes(workflow, "shell: false");
  assertEquals(workflow.includes("deno install"), false);
  assertEquals(workflow.includes("deno task test"), false);
  assertEquals(workflow.includes("feles-build"), false);
  assertEquals(workflow.includes("\n  pull_request:\n"), false);
  assertStringIncludes(
    workflow,
    "EVENT_HEAD_SHA: ${{ github.event.pull_request.head.sha }}",
  );
});

Deno.test("FDH candidate workflow retains the required host test and build job", async () => {
  const workflow = await Deno.readTextFile(CANDIDATE_WORKFLOW);
  assertStringIncludes(workflow, "\n  pull_request:\n");
  assertStringIncludes(workflow, "\n  review-gate:\n");
  assertStringIncludes(workflow, "deno task test:host");
  assertStringIncludes(
    workflow,
    "deno task feles-build build --phase before-mach",
  );
  assertEquals(workflow.includes("fdh_review.ts"), false);
  assertEquals(workflow.includes("pull_request_target:"), false);
});

Deno.test({
  name: "trusted receipt runner subprocess integration",
  ignore:
    (await Deno.permissions.query({ name: "run", command: Deno.execPath() }))
      .state !== "granted",
  fn: async () => {
    const head = "9955ae2e96646d1e323c241fa738b06ea33cf553";
    const receipt = (reviewer: string) => ({
      schemaVersion: 1,
      reviewer,
      model: "fixture-model",
      commit: head,
      verdict: "approve",
      blocking: false,
      scope: "workflow contract",
      tier: "contract_spot",
    });
    const input = {
      prNumber: 16,
      expectedHeadSha: head,
      currentHeadSha: head,
      comments: [
        {
          body: `<!-- fdh-review-receipt:v1 ${
            JSON.stringify(receipt("reviewer-a"))
          } -->`,
          author: "member-a",
          authorAssociation: "MEMBER",
          authorPermission: "write",
        },
        {
          body: `<!-- fdh-review-receipt:v1 ${
            JSON.stringify(receipt("reviewer-b"))
          } -->`,
          author: "member-b",
          authorAssociation: "COLLABORATOR",
          authorPermission: "admin",
        },
      ],
    };
    const child = new Deno.Command(Deno.execPath(), {
      args: ["run", "--no-config", "--no-lock", "--quiet", RUNNER.href],
      clearEnv: true,
      env: { DENO_NO_UPDATE_CHECK: "1" },
      stdin: "piped",
      stdout: "piped",
      stderr: "piped",
    }).spawn();
    const writer = child.stdin.getWriter();
    await writer.write(new TextEncoder().encode(JSON.stringify(input)));
    await writer.close();
    const result = await child.output();
    assertEquals(result.code, 0, new TextDecoder().decode(result.stderr));
    const output = JSON.parse(new TextDecoder().decode(result.stdout));
    assertEquals(output.name, "fdh-trusted-receipts");
    assertEquals(output.headSha, head);
    assertEquals(output.conclusion, "success");
  },
});
