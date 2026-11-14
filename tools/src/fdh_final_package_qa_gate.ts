// SPDX-License-Identifier: MPL-2.0

import type {
  FinalPackageQaConsistency,
  FinalPackageQaExpectation,
  FinalQaArtifactIdentity,
  FinalQaRunIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";

const TARGETS: readonly FinalQaTarget[] = [
  "windows/x86_64",
  "linux/x86_64",
  "mac/x86_64",
  "mac/aarch64",
];
const LIMITATIONS = [
  "Caller API snapshot authenticity, completeness and trusted-policy origin are not verified.",
  "Post-QA ordering uses only the declared precision interval of API artifact/job upper timestamps, not arbitrary clock skew or extra freshness tolerance.",
  "Caller ZIP download, archive byte hashing and extraction authenticity are unproved; digest agreement is consistency only.",
  "Source eligibility is a signed-mode workflow attestation comparison, not fresh pending-deployment or approval verification.",
  "No QA3 verdict is evaluated or replaced; no release authorization, live proof, publication lock or publisher dependency is established.",
  "Windows shadow still requires complete pre/post attestations; failure with missing post-QA evidence is rejected, and synthetic complete failure shapes are not live reachability proof.",
] as const;

function requireCondition(
  condition: unknown,
  message: string,
): asserts condition {
  if (!condition) throw new Error(`Final package QA consistency: ${message}`);
}
function object(value: unknown): Record<string, unknown> {
  requireCondition(
    typeof value === "object" && value !== null && !Array.isArray(value),
    "expected object",
  );
  return value as Record<string, unknown>;
}
function array(value: unknown): unknown[] {
  requireCondition(Array.isArray(value), "expected complete array");
  return value;
}
function id(value: unknown): number {
  requireCondition(
    typeof value === "number" && Number.isSafeInteger(value) && value > 0,
    "invalid positive id/attempt",
  );
  return value;
}
function time(value: unknown): number {
  requireCondition(
    typeof value === "string" &&
      /^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{1,3})?Z$/.test(value),
    "invalid UTC timestamp",
  );
  const result = Date.parse(value);
  requireCondition(
    Number.isFinite(result) &&
      new Date(result).toISOString().slice(0, 19) === value.slice(0, 19),
    "invalid timestamp",
  );
  return result;
}
// REST timestamps can omit fractional seconds while checkedAt is ISO millis.
// An upper ordering bound covers ONLY its declared precision interval, not clock
// skew: whole seconds end at .999; .1 at .199; .12 at .129; .123 stays exact.
// Trusted now/freshness and attempt lower bounds receive no such tolerance.
function precisionUpper(value: unknown): number {
  const start = time(value);
  requireCondition(typeof value === "string", "invalid timestamp precision");
  const digits = /\.(\d+)Z$/.exec(value)?.[1].length ?? 0;
  return start + 10 ** (3 - digits) - 1;
}
function digest(value: unknown): void {
  requireCondition(
    typeof value === "string" && /^sha256:[0-9a-f]{64}$/.test(value),
    "invalid digest",
  );
}
function uniqueIds(values: unknown[]): void {
  const ids = values.map((value) => id(object(value).id));
  requireCondition(new Set(ids).size === ids.length, "duplicate API ids");
}
function reference(value: unknown, expected: FinalQaArtifactIdentity): void {
  const actual = object(value);
  id(expected.id);
  digest(expected.digest);
  requireCondition(
    typeof expected.name === "string" && expected.name.length > 0,
    "invalid expected name",
  );
  requireCondition(
    actual.id === expected.id && actual.name === expected.name &&
      actual.digest === expected.digest,
    "artifact reference mismatch",
  );
}
function run(
  value: unknown,
  expected: FinalQaRunIdentity,
  policy: FinalPackageQaExpectation,
  path: string,
  qa: boolean,
): Record<string, unknown> {
  const actual = object(value);
  const repo = object(actual.repository);
  id(expected.id);
  id(expected.workflowId);
  id(expected.attempt);
  requireCondition(
    /^[0-9a-f]{40}$/.test(expected.head),
    "invalid expected head",
  );
  requireCondition(
    actual.id === expected.id && actual.workflow_id === expected.workflowId &&
      actual.run_attempt === expected.attempt &&
      actual.head_sha === expected.head &&
      actual.path === path && actual.event === "workflow_dispatch" &&
      actual.head_branch === "main" && repo.id === policy.repository.id &&
      repo.full_name === policy.repository.fullName,
    "run repository/workflow/attempt/head mismatch",
  );
  requireCondition(
    qa
      ? actual.status === "completed" && actual.conclusion === "success"
      : ["waiting", "in_progress"].includes(String(actual.status)) &&
        actual.conclusion === null,
    "run is not in expected completed-QA/signed-prepublication state",
  );
  return actual;
}
function artifact(
  artifacts: unknown[],
  expected: FinalQaArtifactIdentity,
  runIdentity: FinalQaRunIdentity,
  started: number,
  ended: number,
  now: number,
  repositoryId: number,
): Record<string, unknown> {
  // Count all matches, including expired ones: a fresh duplicate cannot hide ambiguity.
  const matches = artifacts.filter((value) =>
    object(value).name === expected.name
  );
  requireCondition(matches.length === 1, "missing/duplicate artifact name");
  const actual = object(matches[0]);
  reference(actual, expected);
  const source = object(actual.workflow_run);
  requireCondition(
    source.id === runIdentity.id && source.head_sha === runIdentity.head &&
      source.repository_id === repositoryId &&
      source.head_repository_id === repositoryId &&
      source.head_branch === "main",
    "artifact source run/head mismatch",
  );
  requireCondition(
    actual.expired === false && time(actual.expires_at) > now,
    "expired artifact",
  );
  const created = time(actual.created_at);
  requireCondition(
    created >= started && created <= ended && created <= now,
    "artifact outside current attempt time window",
  );
  return actual;
}
function eligibility(value: unknown, expected: FinalQaArtifactIdentity): void {
  const actual = object(value);
  requireCondition(
    actual.phase === "awaiting-publication-approval" &&
      actual.publicationApprovalPerformed === false,
    "not signed prepublication attestation",
  );
  reference(actual.bundle, expected);
}

/**
 * Pure consistency check only. This does not authenticate API responses or archive
 * bytes, run QA, implement source eligibility policy, or authorize publication.
 * The trusted caller must retain complete current-attempt API job/artifact lists,
 * download/hash the named ZIPs and safely extract entries; no boolean can replace
 * those external operations here. Parsed entry names must be archive-root paths.
 */
export function validateFinalPackageQaConsistency(
  expected: FinalPackageQaExpectation,
  observation: unknown,
): FinalPackageQaConsistency {
  const observed = object(observation);
  id(expected.repository.id);
  requireCondition(
    expected.repository.fullName === "Floorp-Projects/Floorp",
    "noncanonical expected repository",
  );
  requireCondition(
    expected.windowsPolicy === "require-success" ||
      expected.windowsPolicy === "shadow-observation",
    "explicit trusted Windows policy required",
  );
  requireCondition(
    Number.isSafeInteger(expected.maxAgeMs) && expected.maxAgeMs > 0,
    "invalid freshness policy",
  );
  requireCondition(
    expected.packageRun.id !== expected.qaRun.id,
    "package and QA runs must differ",
  );
  const now = time(expected.now);
  const packageRun = run(
    observed.packageRun,
    expected.packageRun,
    expected,
    ".github/workflows/package_and_publish_release.yml",
    false,
  );
  const qaRun = run(
    observed.qaRun,
    expected.qaRun,
    expected,
    ".github/workflows/validate-final-package-qa.yml",
    true,
  );
  const packageStart = time(packageRun.run_started_at);
  const packageEnd = time(packageRun.updated_at);
  const qaStart = time(qaRun.run_started_at);
  const qaEnd = time(qaRun.updated_at);
  const qaStartUpper = precisionUpper(qaRun.run_started_at);
  const qaEndUpper = precisionUpper(qaRun.updated_at);
  requireCondition(
    packageStart <= precisionUpper(packageRun.updated_at) &&
      packageEnd <= now &&
      packageStart <= qaStartUpper &&
      qaStart <= qaEndUpper && qaEnd <= now &&
      now - qaStart <= expected.maxAgeMs,
    "stale/future/invalid run time window",
  );
  const jobs = array(observed.qaJobs);
  const packageArtifacts = array(observed.packageArtifacts);
  const qaArtifacts = array(observed.qaArtifacts);
  const evidence = array(observed.evidence);
  uniqueIds(jobs);
  uniqueIds(packageArtifacts);
  uniqueIds(qaArtifacts);
  requireCondition(
    jobs.length === TARGETS.length && evidence.length === TARGETS.length,
    "exact four-job/evidence scope required",
  );
  requireCondition(
    expected.bundle.name === "floorp-release-bundle-v2",
    "noncanonical bundle name",
  );
  artifact(
    packageArtifacts,
    expected.bundle,
    expected.packageRun,
    packageStart,
    qaStartUpper,
    now,
    expected.repository.id,
  );
  const evidenceIds = evidence.map((value) => id(object(value).artifactId));
  requireCondition(
    new Set(evidenceIds).size === TARGETS.length,
    "duplicate evidence payload",
  );
  const selectedQaIds: number[] = [];
  let windowsConclusion: "success" | "failure" = "success";
  for (const target of TARGETS) {
    const [platform, arch] = target.split("/");
    const selection = expected.targets[target];
    const jobName = `Final package QA / ${target}`;
    const matchingJobs = jobs.filter((value) => object(value).name === jobName);
    requireCondition(
      matchingJobs.length === 1,
      "missing/duplicate canonical QA job",
    );
    const job = object(matchingJobs[0]);
    requireCondition(
      job.run_id === expected.qaRun.id &&
        job.run_attempt === expected.qaRun.attempt &&
        job.head_sha === expected.qaRun.head && job.head_branch === "main" &&
        job.status === "completed",
      "job run/attempt/completion mismatch",
    );
    const shadow = target === "windows/x86_64" &&
      expected.windowsPolicy === "shadow-observation";
    requireCondition(
      job.conclusion === "success" || (shadow && job.conclusion === "failure"),
      "failed/skipped/cancelled required job",
    );
    if (target === "windows/x86_64" && job.conclusion === "failure") {
      windowsConclusion = "failure";
    }
    const jobStart = time(job.started_at);
    const jobEnd = time(job.completed_at);
    requireCondition(
      jobStart >= qaStart && jobStart <= precisionUpper(job.completed_at) &&
        jobEnd <= qaEndUpper && jobEnd <= now,
      "job outside QA attempt",
    );
    const packageName = platform === "mac"
      ? "noraneko-mac-universal-installer"
      : `noraneko-${platform}-${arch}-installer`;
    requireCondition(
      selection.package.name === packageName &&
        selection.record.name ===
          `floorp-native-verification-${platform}-${arch}-v2` &&
        selection.evidence.name ===
          `final-package-qa-${platform}-${arch}-${expected.qaRun.id}`,
      "noncanonical target artifact names",
    );
    artifact(
      packageArtifacts,
      selection.package,
      expected.packageRun,
      packageStart,
      qaStartUpper,
      now,
      expected.repository.id,
    );
    artifact(
      packageArtifacts,
      selection.record,
      expected.packageRun,
      packageStart,
      qaStartUpper,
      now,
      expected.repository.id,
    );
    const qaArtifact = artifact(
      qaArtifacts,
      selection.evidence,
      expected.qaRun,
      qaStart,
      qaEndUpper,
      now,
      expected.repository.id,
    );
    requireCondition(
      time(qaArtifact.created_at) >= jobStart &&
        time(qaArtifact.created_at) <= precisionUpper(job.completed_at),
      "evidence artifact outside target job",
    );
    selectedQaIds.push(selection.evidence.id);
    const payloads = evidence.filter((value) =>
      object(value).artifactId === selection.evidence.id
    );
    requireCondition(
      payloads.length === 1,
      "missing/duplicate target evidence payload",
    );
    const payload = object(payloads[0]);
    // This is an untrusted observed digest string, NOT a byte verification receipt.
    digest(payload.observedArchiveDigest);
    requireCondition(
      payload.observedArchiveDigest === selection.evidence.digest,
      "observed archive digest disagreement",
    );
    const entries = array(payload.entries);
    const names = entries.map((value) => object(value).name);
    requireCondition(
      names.every((name) =>
        typeof name === "string" && name.length > 0 &&
        !name.includes("\\") && !name.startsWith("/") &&
        !name.split("/").some((part) =>
          part === ".." || part === "." || part === ""
        )
      ) && new Set(names).size === names.length,
      "ambiguous/duplicate archive entry paths",
    );
    const entry = (name: string): unknown => {
      const matches = entries.filter((value) => object(value).name === name);
      requireCondition(
        matches.length === 1,
        "missing/duplicate required evidence file",
      );
      return object(matches[0]).value;
    };
    const input = object(entry("input.json"));
    requireCondition(
      input.runId === expected.packageRun.id &&
        input.floorpHead === expected.packageRun.head &&
        input.probeHead === expected.qaRun.head &&
        input.platform === platform && input.arch === arch &&
        input.unsigned === false,
      "poisoned package/head/probe/target/signed attestation",
    );
    requireCondition(
      typeof input.floorpVersion === "string" &&
        /^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$/.test(input.floorpVersion) &&
        typeof input.firefoxVersion === "string" &&
        /^\d+(?:\.\d+){1,3}(?:[ab]\d+|esr)?$/.test(input.firefoxVersion) &&
        typeof input.runtimeBuildId === "string" &&
        /^\d{14}$/.test(input.runtimeBuildId),
      "invalid emitted version metadata",
    );
    requireCondition(
      input.floorpVersion === selection.floorpVersion &&
        input.firefoxVersion === selection.firefoxVersion &&
        input.runtimeBuildId === selection.runtimeBuildId,
      "version metadata disagrees with trusted target expectation",
    );
    reference(input.package, selection.package);
    reference(input.record, selection.record);
    eligibility(input.sourceEligibility, expected.bundle);
    const after = object(entry("source-after-qa.json"));
    eligibility(after.sourceEligibility, expected.bundle);
    const checked = time(after.checkedAt);
    requireCondition(
      checked >= jobStart && checked <= precisionUpper(qaArtifact.created_at) &&
        checked <= precisionUpper(job.completed_at) &&
        checked <= now && now - checked <= expected.maxAgeMs,
      "stale/future/out-of-job post-QA check",
    );
  }
  requireCondition(
    new Set(selectedQaIds).size === TARGETS.length &&
      qaArtifacts.length === TARGETS.length,
    "ambiguous QA artifact scope",
  );
  return {
    attestationConsistencyMatched: true,
    archiveBytesVerified: false,
    publicationAuthorized: false,
    scope: "source-attestation-consistency-only",
    packageRunId: expected.packageRun.id,
    qaRunId: expected.qaRun.id,
    qaAttempt: expected.qaRun.attempt,
    targets: [...TARGETS],
    windows: {
      policy: expected.windowsPolicy,
      conclusion: windowsConclusion,
      scope: expected.windowsPolicy === "shadow-observation"
        ? "shadow-observation-only"
        : "required-success",
    },
    limitations: [...LIMITATIONS],
  };
}
