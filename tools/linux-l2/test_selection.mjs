// SPDX-License-Identifier: MPL-2.0
// Execute the exact hosted admission script with a synthetic, read-only provider.
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import path from "node:path";
import test from "node:test";

/** @typedef {import('./selection-types.ts').AdmissionFixture} AdmissionFixture */
/** @typedef {import('./selection-types.ts').AdmissionOptions} AdmissionOptions */
/** @typedef {import('./selection-types.ts').RawArtifactFixture} RawArtifactFixture */

const root = fileURLToPath(new URL("../../", import.meta.url));
const workflow = readFileSync(
  new URL(
    "../../.github/workflows/validate-final-package-qa.yml",
    import.meta.url,
  ),
  "utf8",
);
const selection =
  workflow.split("  l2-selection:\n")[1].split("\n  final-package:")[0];
const script = selection.split("          script: |\n")[1].split("\n").map((
  line,
) => line.slice(12)).join("\n");
const AsyncFunction = Object.getPrototypeOf(async function () {}).constructor;
const evaluate = new AsyncFunction("github", "context", script);
const observerScript = workflow.split("  linux-l2-observation:\n")[1]
  .split("          script: |\n")[1].split("\n      - name:")[0]
  .split("\n").map((line) => line.slice(12)).join("\n");
const observe = new AsyncFunction(
  "github",
  "context",
  "require",
  observerScript,
);
const source = "a".repeat(40);

/** @returns {AdmissionFixture} */
function fixture(prePublication = false) {
  const run = {
    id: 11,
    repository: { full_name: "Floorp-Projects/Floorp" },
    path: ".github/workflows/package_and_publish_release.yml",
    head_sha: source,
    head_branch: "main",
    event: "workflow_dispatch",
    run_attempt: 1,
    status: prePublication ? "waiting" : "completed",
    conclusion: prePublication ? null : "success",
  };
  const jobs = [
    "windows/x86_64",
    "linux/x86_64",
    "linuxAarch64/aarch64",
    "mac/x86_64",
    "mac/aarch64",
  ]
    .map((target) => ({
      name: `Verify ${target} package`,
      status: "completed",
      conclusion: "success",
    }));
  jobs.push(
    ...[
      "Validate provenance and assemble the flat release bundle",
      "Validate immutable bundle and create draft release",
    ]
      .map((name) => ({ name, status: "completed", conclusion: "success" })),
  );
  const artifacts = [
    "noraneko-linux-x86_64-installer",
    "floorp-native-verification-linux-x86_64-v2",
    "floorp-release-bundle-v2",
  ]
    .map((name, index) => ({
      id: index + 31,
      name,
      digest: "sha256:" + "b".repeat(64),
      expired: false,
      workflow_run: { id: 11, head_sha: source },
    }));
  return {
    run,
    jobs,
    artifacts,
    pending: [{
      environment: { id: 1, name: "Deploy-to-installer-release" },
      reviewers: [{ type: "User", reviewer: { id: 2 } }],
    }],
    release: { draft: true, prerelease: false, target_commitish: source },
  };
}

/** @param {AdmissionFixture} data @param {AdmissionOptions} [options] */
async function admit(
  data,
  { unsigned = false, attempt = "1", prePublication = false } = {},
) {
  const saved = { ...process.env };
  Object.assign(process.env, {
    GITHUB_WORKSPACE: root,
    UNSIGNED: String(unsigned),
    GITHUB_RUN_ATTEMPT: attempt,
    PACKAGE_RUN: "11",
    FLOORP_HEAD: source,
    PRE_PUBLICATION: String(prePublication),
  });
  const actions = {
    getWorkflowRun: () => Promise.resolve({ data: data.run }),
    listJobsForWorkflowRunAttempt: () => {},
    listWorkflowRunArtifacts: () => {},
    getPendingDeploymentsForRun: () => Promise.resolve({ data: data.pending }),
  };
  const github = {
    rest: {
      actions,
      repos: {
        getContent: () =>
          Promise.resolve({
            data: {
              content: Buffer.from('{"version":"12.20.2"}').toString("base64"),
            },
          }),
        getReleaseByTag: () => Promise.resolve({ data: data.release }),
      },
    },
    paginate: (/** @type {unknown} */ method) =>
      Promise.resolve(
        method === actions.listJobsForWorkflowRunAttempt
          ? data.jobs
          : data.artifacts,
      ),
  };
  try {
    await evaluate(github, {
      repo: { owner: "Floorp-Projects", repo: "Floorp" },
    });
  } finally {
    for (const key of Object.keys(process.env)) {
      if (!(key in saved)) delete process.env[key];
    }
    Object.assign(process.env, saved);
  }
}

test("original completed production input is admitted", async () =>
  await admit(fixture()));
test("protected production input is admitted before approval", async () =>
  await admit(fixture(true), { prePublication: true }));
/** @type {Array<[string, (data:AdmissionFixture)=>void, AdmissionOptions?]>} */
const invalidAdmissions = [
  ["unsigned mode", () => {}, { unsigned: true }],
  ["consumer retry", () => {}, { attempt: "2" }],
  ["producer retry", (data) => data.run.run_attempt = 2],
  ["failed producer", (data) => data.run.conclusion = "failure"],
  ["missing native target", (data) => data.jobs.shift()],
  ["unfinished completed-mode producer", (data) => {
    data.run.status = "waiting";
    data.run.conclusion = null;
  }],
  ["pre-publication mismatch", () => {}, { prePublication: true }],
  ["missing production package", (data) => data.artifacts.shift()],
  [
    "foreign artifact source",
    (data) => data.artifacts[0].workflow_run.head_sha = "c".repeat(40),
  ],
  ["missing artifact digest", (data) => delete data.artifacts[0].digest],
];
for (const [name, mutate, options] of invalidAdmissions) {
  test(`${name} is rejected by hosted admission`, async () => {
    const data = fixture();
    mutate(data);
    await assert.rejects(() => admit(data, options));
  });
}
/** @type {Array<[string, (data:AdmissionFixture)=>void]>} */
const invalidProtectedInputs = [
  ["missing protected approval", (data) => data.pending = []],
  ["already public release", (data) => data.release.draft = false],
  ["missing bundle", (data) => data.artifacts.pop()],
];
for (const [name, mutate] of invalidProtectedInputs) {
  test(`${name} is rejected before Blacksmith allocation`, async () => {
    const data = fixture(true);
    mutate(data);
    await assert.rejects(() => admit(data, { prePublication: true }));
  });
}

/** @type {Array<[string, (artifact:RawArtifactFixture)=>void, number]>} */
const rawAcquisitions = [
  ["exact raw metadata", () => {}, 1],
  ["foreign raw run", (artifact) => artifact.workflow_run.id = 99, 0],
  [
    "wrong raw attempt name",
    (artifact) => artifact.name = "final-package-qa-linux-x86_64-22-2",
    0,
  ],
  ["expired raw artifact", (artifact) => artifact.expired = true, 0],
  [
    "oversize raw artifact",
    (artifact) => artifact.size_in_bytes = 64 * 1024 * 1024 + 1,
    0,
  ],
];
for (const [name, mutate, expectedDownloads] of rawAcquisitions) {
  test(`${name}: observer checks metadata before byte acquisition`, async () => {
    const saved = { ...process.env };
    Object.assign(process.env, {
      RUNNER_TEMP: "/synthetic",
      RAW_ARTIFACT: "55",
      PACKAGE_RUN: "11",
      FLOORP_HEAD: source,
      GITHUB_RUN_ATTEMPT: "1",
    });
    const artifact = {
      id: 55,
      size_in_bytes: 9,
      name: "final-package-qa-linux-x86_64-22-1",
      expired: false,
      digest: "sha256:" + "b".repeat(64),
      workflow_run: { id: 22, head_sha: source },
    };
    mutate(artifact);
    let downloads = 0;
    /** @type {Map<string,string|Uint8Array>} */
    const files = new Map();
    /** @type {{writeFileSync:(name:string,data:string|Uint8Array)=>void}} */
    const fakeFs = { writeFileSync: (name, data) => files.set(name, data) };
    /** @param {string} name */
    function fakeRequire(name) {
      return name === "node:fs" ? fakeFs : path;
    }
    const actions = {
      getWorkflowRun: () => Promise.resolve({ data: {} }),
      listJobsForWorkflowRunAttempt: () => {},
      listWorkflowRunArtifacts: () => {},
      getArtifact: () => Promise.resolve({ data: artifact }),
      downloadArtifact: () => {
        downloads++;
        return Promise.resolve({ data: Buffer.from("synthetic") });
      },
    };
    const github = { rest: { actions }, paginate: () => Promise.resolve([]) };
    try {
      await observe(
        github,
        {
          runId: 22,
          sha: source,
          repo: { owner: "Floorp-Projects", repo: "Floorp" },
        },
        fakeRequire,
      );
      assert.equal(downloads, expectedDownloads);
      const serialized = files.get("/synthetic/l2-context.json");
      assert.ok(typeof serialized === "string");
      const retained = JSON.parse(serialized);
      assert.equal(
        retained.collectionErrors.length,
        expectedDownloads === 1 ? 0 : 1,
      );
    } finally {
      for (const key of Object.keys(process.env)) {
        if (!(key in saved)) delete process.env[key];
      }
      Object.assign(process.env, saved);
    }
  });
}
