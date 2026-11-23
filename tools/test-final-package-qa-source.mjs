// SPDX-License-Identifier: MPL-2.0
import assert from "node:assert/strict";
import test from "node:test";
import { validateFinalPackageSource } from "./final-package-qa-source.mjs";
/** @returns {import('./final-package-qa-source.mjs').FinalSourceInput} */
function fixture() {
  const headSha = "a".repeat(40);
  /** @param {string} name @returns {import('./final-package-qa-source.mjs').PackageJob} */
  const success = (name) => ({
    name,
    status: "completed",
    conclusion: "success",
  });
  return {
    headSha,
    prePublication: true,
    unsigned: false,
    run: {
      id: 123,
      repository: { full_name: "Floorp-Projects/Floorp" },
      path: ".github/workflows/package_and_publish_release.yml",
      head_sha: headSha,
      event: "workflow_dispatch",
      head_branch: "main",
      status: "waiting",
      conclusion: null,
    },
    jobs: [
      "windows/x86_64",
      "linux/x86_64",
      "linuxAarch64/aarch64",
      "mac/x86_64",
      "mac/aarch64",
    ]
      .map((target) => success(`Verify package / Verify ${target} package`))
      .concat([
        success(
          "Assemble / Validate provenance and assemble the flat release bundle",
        ),
        success("Publish / Validate immutable bundle and create draft release"),
        {
          name: "Publish / Manual approval and public release gate",
          status: "waiting",
          conclusion: null,
        },
      ]),
    pendingDeployments: [{
      environment: { id: 456, name: "Deploy-to-installer-release" },
      reviewers: [{ type: "User", reviewer: { id: 123 } }],
    }],
    release: { draft: true, prerelease: false, target_commitish: headSha },
    bundleArtifacts: [{
      id: 789,
      name: "floorp-release-bundle-v2",
      expired: false,
      digest: `sha256:${"b".repeat(64)}`,
      workflow_run: { id: 123, head_sha: headSha },
    }],
  };
}
test("accepts finished signed packages awaiting protected publication approval", () => {
  assert.equal(
    validateFinalPackageSource(fixture()).phase,
    "awaiting-publication-approval",
  );
});
test("default intake still requires the whole run to succeed", () => {
  const x = fixture();
  x.prePublication = false;
  assert.throws(() => validateFinalPackageSource(x), /completed successful/);
  x.run.status = "completed";
  x.run.conclusion = "success";
  assert.equal(validateFinalPackageSource(x).phase, "completed");
});
test("draft invisibility never substitutes for a pending protected gate", () => {
  const x = fixture();
  x.release = null;
  assert.equal(
    validateFinalPackageSource(x).phase,
    "awaiting-publication-approval",
  );
  x.pendingDeployments = [];
  assert.throws(
    () => validateFinalPackageSource(x),
    /protected installer approval/,
  );
});
/** @type {Array<[string,(input:import('./final-package-qa-source.mjs').FinalSourceInput)=>void]>} */
const rejected = [
  ["foreign source", (x) => {
    x.run.head_sha = "c".repeat(40);
  }],
  ["noncanonical workflow", (x) => {
    x.run.path = ".github/workflows/package.yml";
  }],
  ["foreign repository", (x) => {
    x.run.repository.full_name = "other/Floorp";
  }],
  ["non-dispatch source", (x) => {
    x.run.event = "pull_request";
  }],
  ["unsigned package", (x) => {
    x.unsigned = true;
  }],
  ["non-main source", (x) => {
    x.run.head_branch = "other";
  }],
  ["failed source", (x) => {
    x.run.conclusion = "failure";
  }],
  ["skipped native verification", (x) => {
    x.jobs[0].conclusion = "skipped";
  }],
  ["missing native architecture", (x) => {
    x.jobs.splice(1, 1);
  }],
  ["ambiguous native evidence", (x) => {
    x.jobs.push({ ...x.jobs[0] });
  }],
  ["unfinished assembly", (x) => {
    x.jobs[5].status = "in_progress";
  }],
  ["failed draft preparation", (x) => {
    x.jobs[6].conclusion = "failure";
  }],
  ["unprotected environment", (x) => {
    x.pendingDeployments[0].reviewers = [];
  }],
  ["wrong environment", (x) => {
    x.pendingDeployments[0].environment.name = "Deploy-to-updater-release";
  }],
  ["multiple pending gates", (x) => {
    x.pendingDeployments.push(x.pendingDeployments[0]);
  }],
  ["executing publication", (x) => {
    x.jobs[7].status = "in_progress";
  }],
  ["completed publication", (x) => {
    x.jobs[7].conclusion = "success";
  }],
  ["executing PPA child", (x) => {
    x.jobs.push({
      name: "Publish the exact source-run DEB to PPA / Publish PPA",
      status: "in_progress",
      conclusion: null,
    });
  }],
  ["public release", (x) => {
    assert.ok(x.release);
    x.release.draft = false;
  }],
  ["unrelated draft", (x) => {
    assert.ok(x.release);
    x.release.target_commitish = "c".repeat(40);
  }],
  ["missing bundle", (x) => {
    x.bundleArtifacts = [];
  }],
  ["expired bundle", (x) => {
    x.bundleArtifacts[0].expired = true;
  }],
  ["foreign bundle run", (x) => {
    x.bundleArtifacts[0].workflow_run.id = 999;
  }],
  ["foreign bundle head", (x) => {
    x.bundleArtifacts[0].workflow_run.head_sha = "c".repeat(40);
  }],
  ["missing bundle digest", (x) => {
    x.bundleArtifacts[0].digest = null;
  }],
];
for (const [name, mutate] of rejected) {
  test(`rejects ${name}`, () => {
    const x = fixture();
    mutate(x);
    assert.throws(() => validateFinalPackageSource(x));
  });
}
