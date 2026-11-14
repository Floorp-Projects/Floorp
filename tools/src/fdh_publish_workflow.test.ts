// SPDX-License-Identifier: MPL-2.0
//
// Static workflow contract checks for the FDH publish wiring.
//
// These tests read workflow YAML as plain text and assert indentation-scoped
// substrings. They are deliberately NOT a YAML parser or GitHub expression
// evaluator: they cannot prove runtime semantics (environment approval gates,
// workflow_dispatch behavior, job conclusions, continue-on-error handling).
// Runtime behavior must be observed on GitHub Actions. See
// docs/qa/fdh-coordinator-followup/PUBLISH-GATE-DESIGN.md for the known gap.

import { assertEquals, assertStringIncludes } from "@std/assert";

const WORKFLOW_DIR = new URL("../../.github/workflows/", import.meta.url);

async function readWorkflow(name: string): Promise<string> {
  const text = await Deno.readTextFile(new URL(name, WORKFLOW_DIR));
  // Normalize checkout line endings so the same text assertions hold on
  // Windows and Linux hosts.
  return text.replace(/\r\n/g, "\n");
}

/**
 * Return the text of one job declared at the two-space `jobs:` indent. The
 * block ends at the next two-space job key; nested mapping keys are deeper.
 */
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

/** Environment declarations are job properties at the four-space indent. */
function environmentDeclarations(jobText: string): string[] {
  return jobText.split("\n").filter((line) => /^ {4}environment:/.test(line));
}

Deno.test("alpha publish declares exactly one protected environment gate", async () => {
  const alpha = await readWorkflow("publish_alpha.yml");
  const main = jobBlock(alpha, "main");
  const declarations = environmentDeclarations(main);
  assertEquals(declarations.length, 1);
  assertEquals(
    declarations[0].trim(),
    "environment: Deploy-to-installer-alpha",
  );
  // The gate belongs to the called workflow's job; the caller cannot set it.
  assertEquals(environmentDeclarations(alpha).length, 1);
  // It must be placed after runs-on and before the first step, i.e. on the job.
  const runsOnIndex = main.indexOf("    runs-on: ubuntu-latest");
  const environmentIndex = main.indexOf("    environment:");
  const stepsIndex = main.indexOf("    steps:");
  assertEquals(runsOnIndex >= 0 && environmentIndex > runsOnIndex, true);
  assertEquals(stepsIndex > environmentIndex, true);
});

Deno.test("alpha publication contract and permissions are unchanged", async () => {
  const alpha = await readWorkflow("publish_alpha.yml");
  assertStringIncludes(alpha, "permissions:\n  contents: write");
  assertStringIncludes(alpha, "workflow_dispatch:");
  assertStringIncludes(alpha, "workflow_call:");
  assertStringIncludes(alpha, "win_runtime_artifact_workflow_call:");
  assertStringIncludes(alpha, "linux_runtime_artifact_workflow_call:");
  assertStringIncludes(alpha, "mac_runtime_artifact_workflow_call:");
  assertStringIncludes(alpha, `tag_name: "alpha"`);
  assertStringIncludes(alpha, "draft: false");
  assertStringIncludes(alpha, "prerelease: true");
  assertStringIncludes(alpha, "name: noraneko-publish");
  // No new credentials or secrets may be introduced by the gate wiring.
  assertEquals(alpha.match(/secrets\.[A-Za-z0-9_]+/g), [
    "secrets.GITHUB_TOKEN",
  ]);
});

Deno.test("alpha caller does not duplicate the environment gate", async () => {
  const caller = await readWorkflow("package_and_publish_alpha.yml");
  // GitHub ignores or rejects a caller job environment for reusable workflows;
  // the single gate must live in publish_alpha.yml only.
  assertEquals(caller.includes("environment:"), false);
  const publishs = jobBlock(caller, "publishs");
  assertStringIncludes(
    publishs,
    "uses: ./.github/workflows/publish_alpha.yml",
  );
  assertStringIncludes(
    publishs,
    "needs: [package-windows, package-linux, package-macos]",
  );
});

Deno.test("final package QA keeps dispatch scope and Windows shadow", async () => {
  const qa = await readWorkflow("validate-final-package-qa.yml");
  assertStringIncludes(qa, "workflow_dispatch:");
  assertStringIncludes(qa, "package_run_id:");
  assertStringIncludes(qa, "floorp_head_sha:");
  assertStringIncludes(qa, "validation_only:");
  assertStringIncludes(qa, "pre_publication:");
  assertStringIncludes(qa, "default: true");
  assertStringIncludes(qa, "default: false");
  // Windows remains a recorded shadow result, not a mandatory leg. Removing
  // this fiat would misrepresent the qualification scope.
  assertStringIncludes(
    qa,
    "continue-on-error: ${{ matrix.platform == 'windows' }}",
  );
  assertStringIncludes(
    qa,
    "Upload compact final package regression evidence",
  );
  assertStringIncludes(qa, "permissions:\n  actions: read\n  contents: read");
  assertEquals(qa.includes("contents: write"), false);
});

Deno.test("stable publisher keeps its protected release boundaries", async () => {
  const publish = await readWorkflow("publish_release.yml");
  for (
    const expected of [
      "environment: Deploy-to-installer-release",
      "environment: Deploy-to-updater-release",
      "group: publish-release-v2-${{ github.repository }}",
      "cancel-in-progress: false",
      "workflow_call:",
      "SOURCE_WORKFLOW_RUN_ID",
      "bundle_artifact",
      "UPDATES_ACTIONS_TOKEN:",
      "PORTABLE_ACTIONS_TOKEN:",
    ]
  ) {
    assertStringIncludes(publish, expected);
  }
  // The exact 16-asset allowlist and public-asset expectation must stay in
  // sync; a later gate patch must not silently shrink either list.
  const expectedAssets = [
    "floorp-linux-aarch64-full.mar",
    "floorp-linux-aarch64.tar.xz",
    "floorp-linux-x86_64-full.mar",
    "floorp-linux-x86_64.tar.xz",
    "floorp-mac-universal-full.mar",
    "floorp-macOS-universal.dmg",
    "floorp-stub.installer.exe",
    "floorp-windows-x86_64-full.mar",
    "floorp-windows-x86_64.installer.exe",
    "hashes.txt",
    "linux-aarch64-meta.json",
    "linux-meta.json",
    "mac-meta.json",
    "release-manifest-set-v2.json",
    "win-meta.json",
  ];
  for (const asset of expectedAssets) {
    const count = publish.split(asset).length - 1;
    assertEquals(count >= 2, true, `${asset} must appear in both asset lists`);
  }
  assertStringIncludes(
    publish,
    "floorp-${process.env.FLOORP_VERSION}.deb",
  );
});

Deno.test("stable entrypoint still calls the publisher only for production", async () => {
  const entry = await readWorkflow("package_and_publish_release.yml");
  const publish = jobBlock(entry, "publish");
  assertStringIncludes(
    publish,
    "uses: ./.github/workflows/publish_release.yml",
  );
  assertStringIncludes(publish, "if: ${{ inputs.validation_only != true }}");
  assertStringIncludes(publish, "source_workflow_run_id: ${{ github.run_id }}");
  assertStringIncludes(publish, "publish_ppa: true");
  // No final package QA dependency exists yet; this is the documented KAN-133
  // release-gate gap, not a claimed completed gate.
  assertEquals(entry.includes("validate-final-package-qa.yml"), false);
});
