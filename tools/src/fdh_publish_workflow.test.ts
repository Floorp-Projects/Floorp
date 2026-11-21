// SPDX-License-Identifier: MPL-2.0
//
// Static workflow contract checks for the FDH publish wiring.
//
// These tests read workflow YAML as plain text and assert indentation-scoped
// substrings. They are deliberately NOT a YAML parser or GitHub expression
// evaluator: they cannot prove runtime semantics (environment approval gates,
// workflow_dispatch behavior, job conclusions, continue-on-error handling).
// Runtime behavior must be observed on GitHub Actions.

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

Deno.test("alpha publication requires QA and writes only from the protected job", async () => {
  const alpha = await readWorkflow("publish_alpha.yml");
  const qa = jobBlock(alpha, "final-package-qa");
  const main = jobBlock(alpha, "main");
  assertEquals(alpha.includes("workflow_dispatch:"), false);
  assertStringIncludes(alpha, "workflow_call:");
  assertStringIncludes(
    qa,
    "uses: ./.github/workflows/validate-final-package-qa.yml",
  );
  assertStringIncludes(qa, "alpha: true");
  assertStringIncludes(qa, "bundle_artifact: ${{ inputs.bundle_artifact }}");
  assertStringIncludes(main, "needs: final-package-qa");
  assertStringIncludes(main, "contents: write");
  assertEquals(alpha.match(/contents: write/g)?.length, 1);
  assertStringIncludes(main, "validate-release-bundle --bundle-dir");
  assertStringIncludes(
    main,
    "generateUpdateXml(Deno.args[0], Deno.args[1], Deno.args[2])",
  );
  assertStringIncludes(main, '"$BUNDLE_DIR/win-meta.json"');
  assertStringIncludes(main, `tag_name: "alpha"`);
  assertStringIncludes(main, "prerelease: true");
  assertStringIncludes(main, "name: noraneko-publish");
  const publishIndex = main.indexOf("- name: Deploy to GitHub Releases");
  const bindTagIndex = main.indexOf("- name: Bind the rolling Alpha tag");
  assertEquals(publishIndex >= 0 && bindTagIndex > publishIndex, true);
  const bindTag = main.slice(bindTagIndex);
  assertStringIncludes(bindTag, "ref: 'tags/alpha'");
  assertStringIncludes(bindTag, "github.rest.git.updateRef");
  assertStringIncludes(bindTag, "sha: context.sha, force: true");
});

Deno.test("alpha caller supplies Runtime identity to the shared package workflow", async () => {
  const caller = await readWorkflow("package_and_publish_alpha.yml");
  assertEquals(caller.includes("environment:"), false);
  const packageJob = jobBlock(caller, "package-and-publish");
  assertStringIncludes(
    packageJob,
    "uses: ./.github/workflows/package_and_publish_release.yml",
  );
  assertStringIncludes(
    packageJob,
    "runtime_manifest_artifact_id: ${{ inputs.runtime_manifest_artifact_id }}",
  );
  assertStringIncludes(packageJob, "alpha: true");
});

Deno.test("final package QA keeps manual dispatch and stable Windows shadow", async () => {
  const qa = await readWorkflow("validate-final-package-qa.yml");
  assertStringIncludes(qa, "workflow_dispatch:");
  assertStringIncludes(qa, "workflow_call:");
  assertStringIncludes(qa, "package_run_id:");
  assertStringIncludes(qa, "floorp_head_sha:");
  assertStringIncludes(qa, "validation_only:");
  assertStringIncludes(qa, "pre_publication:");
  assertStringIncludes(qa, "default: true");
  assertStringIncludes(qa, "default: false");
  assertStringIncludes(
    qa,
    "continue-on-error: ${{ matrix.platform == 'windows' && !inputs.alpha }}",
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
  const qa = jobBlock(publish, "final-package-qa");
  const prepare = jobBlock(publish, "prepare_release");
  const releaseCheck = jobBlock(publish, "release_check");
  assertStringIncludes(
    qa,
    "uses: ./.github/workflows/validate-final-package-qa.yml",
  );
  assertStringIncludes(
    qa,
    "installer_artifact_ids: ${{ inputs.installer_artifact_ids }}",
  );
  assertStringIncludes(prepare, "needs: final-package-qa");
  assertStringIncludes(prepare, "draft: true");
  assertStringIncludes(
    releaseCheck,
    "needs: [final-package-qa, prepare_release]",
  );
  assertStringIncludes(
    releaseCheck,
    "environment: Deploy-to-installer-release",
  );
  assertStringIncludes(releaseCheck, "await github.rest.repos.updateRelease(");
  assertStringIncludes(
    jobBlock(publish, "publish_updates"),
    "needs: [prepare_release, release_check]",
  );
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

Deno.test("shared packaging selects the channel and isolates validation-only runs", async () => {
  const entry = await readWorkflow("package_and_publish_release.yml");
  const callInputs =
    entry.split("  workflow_call:")[1].split("\npermissions:")[0];
  assertStringIncludes(
    callInputs,
    "validation_only:\n        type: boolean\n        default: false",
  );
  const publish = jobBlock(entry, "publish");
  const alpha = jobBlock(entry, "publish-alpha");
  assertStringIncludes(
    publish,
    "uses: ./.github/workflows/publish_release.yml",
  );
  assertStringIncludes(
    publish,
    "if: ${{ inputs.validation_only != true && inputs.alpha != true }}",
  );
  assertStringIncludes(publish, "source_workflow_run_id: ${{ github.run_id }}");
  assertStringIncludes(publish, "publish_ppa: true");
  assertStringIncludes(
    alpha,
    "if: ${{ inputs.validation_only != true && inputs.alpha == true }}",
  );
  assertStringIncludes(alpha, "uses: ./.github/workflows/publish_alpha.yml");
  for (const job of [publish, alpha]) {
    assertStringIncludes(job, "needs: assemble-release-bundle");
    assertStringIncludes(
      job,
      "needs.assemble-release-bundle.outputs.artifact_id",
    );
    assertStringIncludes(
      job,
      "needs.assemble-release-bundle.outputs.artifact_digest",
    );
    assertStringIncludes(
      job,
      "needs.assemble-release-bundle.outputs.installer_artifact_ids",
    );
  }
});
