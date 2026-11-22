// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertThrows } from "@std/assert";
import { validateFinalPackageQaConsistency } from "./fdh_final_package_qa_gate.ts";
import type {
  FinalPackageQaExpectation,
  FinalQaArtifactIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";

// Faithful emitted fields: workflow lines 182-187 and 425-426, NOT an invented
// post-QA run/head schema. Synthetic REST subsets include actual job head_sha and
// run_attempt, while artifact.workflow_run deliberately has no attempt field.
function fixture() {
  const head = "a".repeat(40);
  const probe = "b".repeat(40);
  const stamp = (minute: number) =>
    "2026-11-13T04:" + String(minute).padStart(2, "0") + ":00Z";
  const ref = (id: number, name: string): FinalQaArtifactIdentity => ({
    id,
    name,
    digest: "sha256:" + id.toString(16).padStart(64, "0"),
  });
  const targets: FinalQaTarget[] = [
    "windows/x86_64",
    "linux/x86_64",
    "mac/x86_64",
    "mac/aarch64",
  ];
  const selections = targets.map((target, index) => {
    const [platform, arch] = target.split("/");
    return [target, {
      package: platform === "mac"
        ? ref(103, "noraneko-mac-universal-installer")
        : ref(101 + index, "noraneko-" + platform + "-" + arch + "-installer"),
      record: ref(
        201 + index,
        "floorp-native-verification-" + platform + "-" + arch + "-v2",
      ),
      evidence: ref(
        301 + index,
        "final-package-qa-" + platform + "-" + arch + "-20",
      ),
      floorpVersion: "12.3.4",
      firefoxVersion: "140.0",
      runtimeBuildId: "20261113000000",
    }];
  });
  const expected: FinalPackageQaExpectation = {
    repository: { id: 1, fullName: "Floorp-Projects/Floorp" },
    packageRun: { id: 10, workflowId: 11, attempt: 1, head },
    qaRun: { id: 20, workflowId: 21, attempt: 1, head: probe },
    bundle: ref(401, "floorp-release-bundle-v2"),
    targets: Object.fromEntries(
      selections,
    ) as FinalPackageQaExpectation["targets"],
    now: stamp(15),
    maxAgeMs: 10 * 60_000,
    windowsPolicy: "require-success",
  };
  const run = (qa: boolean) => ({
    id: qa ? 20 : 10,
    workflow_id: qa ? 21 : 11,
    run_attempt: 1,
    repository: { id: 1, full_name: "Floorp-Projects/Floorp" },
    path: qa
      ? ".github/workflows/validate-final-package-qa.yml"
      : ".github/workflows/package_and_publish_release.yml",
    head_sha: qa ? probe : head,
    head_branch: "main",
    event: "workflow_dispatch",
    status: qa ? "completed" : "waiting",
    conclusion: qa ? "success" : null,
    run_started_at: stamp(qa ? 6 : 0),
    updated_at: stamp(qa ? 12 : 14),
  });
  const apiArtifact = (identity: FinalQaArtifactIdentity, qa: boolean) => ({
    ...identity,
    expired: false,
    created_at: stamp(qa ? 10 : 4),
    expires_at: "2026-12-13T04:00:00Z",
    workflow_run: {
      id: qa ? 20 : 10,
      head_sha: qa ? probe : head,
      head_branch: "main",
      repository_id: 1,
      head_repository_id: 1,
    },
  });
  const packageRefs = [
    expected.bundle,
    ...targets.flatMap((target) => [
      expected.targets[target].package,
      expected.targets[target].record,
    ]),
  ];
  const observed = {
    packageRun: run(false),
    qaRun: run(true),
    qaJobs: targets.map((target, index) => ({
      id: 501 + index,
      name: "Final package QA / " + target,
      run_id: 20,
      run_attempt: 1,
      head_sha: probe,
      head_branch: "main",
      status: "completed",
      conclusion: "success",
      started_at: stamp(7),
      completed_at: stamp(11),
    })),
    packageArtifacts: [
      ...new Map(packageRefs.map((identity) => [identity.id, identity]))
        .values(),
    ]
      .map((identity) => apiArtifact(identity, false)),
    qaArtifacts: targets.map((target) =>
      apiArtifact(expected.targets[target].evidence, true)
    ),
    evidence: targets.map((target) => {
      const [platform, arch] = target.split("/");
      const selected = expected.targets[target];
      const sourceEligibility = {
        phase: "awaiting-publication-approval",
        bundle: { ...expected.bundle },
        publicationApprovalPerformed: false,
      };
      return {
        artifactId: selected.evidence.id,
        observedArchiveDigest: selected.evidence.digest,
        entries: [
          {
            name: "input.json",
            value: {
              runId: 10,
              floorpHead: head,
              probeHead: probe,
              platform,
              arch,
              unsigned: false,
              floorpVersion: selected.floorpVersion,
              firefoxVersion: selected.firefoxVersion,
              runtimeBuildId: selected.runtimeBuildId,
              sourceEligibility: structuredClone(sourceEligibility),
              package: { ...selected.package },
              record: { ...selected.record },
            },
          },
          {
            name: "source-after-qa.json",
            value: {
              sourceEligibility: structuredClone(sourceEligibility),
              checkedAt: stamp(9),
            },
          },
        ],
      };
    }),
  };
  return { expected, observed };
}

// Unknown-path mutation keeps deliberately malformed fixtures out of production
// types without weakening the validator boundary or using an unchecked cast.
function poison(
  root: unknown,
  path: readonly (string | number)[],
  value: unknown,
): void {
  assert(path.length > 0);
  const [key, ...rest] = path;
  assert(typeof root === "object" && root !== null);
  const record = root as Record<string | number, unknown>;
  if (rest.length === 0) record[key] = value;
  else poison(record[key], rest, value);
}

Deno.test("faithful signed pre/post fields match only nonauthorizing consistency", () => {
  const { expected, observed } = fixture();
  const before = JSON.stringify({ expected, observed });
  const result = validateFinalPackageQaConsistency(expected, observed);
  assertEquals(result.attestationConsistencyMatched, true);
  assertEquals(result.archiveBytesVerified, false);
  assertEquals(result.publicationAuthorized, false);
  assertEquals(result.scope, "source-attestation-consistency-only");
  assertEquals(result.windows.scope, "required-success");
  assertEquals(result.targets.length, 4);
  assert(
    result.limitations.some((text) =>
      text.includes("ZIP") && text.includes("unproved")
    ),
  );
  assert(result.limitations.some((text) => text.includes("not fresh")));
  assert(!("publishAuthorized" in result));
  assert(!("qa3Verdict" in result));
  assertEquals(JSON.stringify({ expected, observed }), before);
});

Deno.test("pending package updated_at is not an artifact completion boundary", () => {
  const { expected, observed } = fixture();
  observed.packageRun.updated_at = "2026-11-13T04:01:00Z";
  assertEquals(
    validateFinalPackageQaConsistency(expected, observed)
      .attestationConsistencyMatched,
    true,
  );
});

Deno.test("post-QA ISO millis respect declared artifact timestamp precision without clock skew", () => {
  for (
    const [created, accepted, rejected] of [
      ["00Z", "00.500Z", "01.000Z"],
      ["00.1Z", "00.199Z", "00.200Z"],
      ["00.12Z", "00.129Z", "00.130Z"],
      ["00.123Z", "00.123Z", "00.124Z"],
    ]
  ) {
    const { expected, observed } = fixture();
    observed.qaArtifacts[0].created_at = "2026-11-13T04:10:" + created;
    poison(
      observed,
      ["evidence", 0, "entries", 1, "value", "checkedAt"],
      "2026-11-13T04:10:" + accepted,
    );
    assertEquals(
      validateFinalPackageQaConsistency(expected, observed)
        .attestationConsistencyMatched,
      true,
    );
    poison(
      observed,
      ["evidence", 0, "entries", 1, "value", "checkedAt"],
      "2026-11-13T04:10:" + rejected,
    );
    assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
  }
});

Deno.test("coarse job and completed-run upper bounds retain API precision intervals", () => {
  const { expected, observed } = fixture();
  observed.qaJobs[0].completed_at = "2026-11-13T04:12:00.500Z";
  observed.qaArtifacts[0].created_at = "2026-11-13T04:12:00.400Z";
  poison(
    observed,
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-11-13T04:12:00.300Z",
  );
  assertEquals(
    validateFinalPackageQaConsistency(expected, observed)
      .attestationConsistencyMatched,
    true,
  );
  observed.qaJobs[0].completed_at = "2026-11-13T04:12:01.000Z";
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});

Deno.test("precision intervals never relax trusted now or freshness bounds", () => {
  const { expected, observed } = fixture();
  expected.now = "2026-11-13T04:12:00.000Z";
  observed.qaArtifacts[0].created_at = "2026-11-13T04:12:00Z";
  observed.qaJobs[0].completed_at = "2026-11-13T04:12:00Z";
  poison(
    observed,
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-11-13T04:12:00.500Z",
  );
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});

Deno.test("explicit Windows shadow is always limited scope even on success", () => {
  const { expected, observed } = fixture();
  expected.windowsPolicy = "shadow-observation";
  assertEquals(
    validateFinalPackageQaConsistency(expected, observed).windows.scope,
    "shadow-observation-only",
  );
  observed.qaJobs[0].conclusion = "failure";
  const result = validateFinalPackageQaConsistency(expected, observed);
  assertEquals(result.windows.conclusion, "failure");
  assertEquals(result.publicationAuthorized, false);
  assertEquals(result.archiveBytesVerified, false);
});

Deno.test("Windows failure in explicit shadow cannot synthesize missing post-QA evidence", () => {
  const { expected, observed } = fixture();
  expected.windowsPolicy = "shadow-observation";
  observed.qaJobs[0].conclusion = "failure";
  observed.evidence[0].entries.pop();
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});

Deno.test("self-declared archiveVerified cannot prove bytes or repair poisoned evidence", () => {
  const { expected, observed } = fixture();
  const claimed = { ...observed, archiveVerified: true };
  assertEquals(
    validateFinalPackageQaConsistency(expected, claimed).archiveBytesVerified,
    false,
  );
  poison(claimed, ["evidence", 1, "entries", 0, "value", "runId"], 999);
  assertThrows(() => validateFinalPackageQaConsistency(expected, claimed));
});

Deno.test("current attempt timestamps permit a complete later attempt, not stale prior artifacts", () => {
  const { expected, observed } = fixture();
  expected.qaRun.attempt = 2;
  observed.qaRun.run_attempt = 2;
  for (const job of observed.qaJobs) job.run_attempt = 2;
  assertEquals(
    validateFinalPackageQaConsistency(expected, observed).qaAttempt,
    2,
  );
  observed.qaArtifacts[0].created_at = "2026-11-13T04:05:59Z";
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});

const badFields: [string, (string | number)[], unknown][] = [
  ["artifact foreign repository", [
    "qaArtifacts",
    0,
    "workflow_run",
    "repository_id",
  ], 999],
  ["artifact fork head repository", [
    "qaArtifacts",
    0,
    "workflow_run",
    "head_repository_id",
  ], 999],
  ["artifact non-main branch", [
    "qaArtifacts",
    0,
    "workflow_run",
    "head_branch",
  ], "feature"],
  ["bundle foreign repository", [
    "packageArtifacts",
    0,
    "workflow_run",
    "repository_id",
  ], 999],
  ["foreign repository", ["qaRun", "repository", "full_name"], "evil/Floorp"],
  ["foreign repository id", ["qaRun", "repository", "id"], 9],
  ["other QA run", ["qaRun", "id"], 99],
  ["other package run", ["packageRun", "id"], 99],
  ["other QA workflow id", ["qaRun", "workflow_id"], 99],
  [
    "other package workflow",
    ["packageRun", "path"],
    ".github/workflows/package.yml",
  ],
  [
    "other QA workflow path",
    ["qaRun", "path"],
    ".github/workflows/impostor.yml",
  ],
  ["wrong event", ["qaRun", "event"], "pull_request"],
  ["wrong package event", ["packageRun", "event"], "push"],
  ["wrong branch", ["packageRun", "head_branch"], "alpha"],
  ["wrong QA branch", ["qaRun", "head_branch"], "feature"],
  ["wrong QA head", ["qaRun", "head_sha"], "c".repeat(40)],
  ["wrong package head", ["packageRun", "head_sha"], "c".repeat(40)],
  ["wrong run attempt", ["qaRun", "run_attempt"], 2],
  [
    "package finished not prepublication",
    ["packageRun", "status"],
    "completed",
  ],
  ["QA failed", ["qaRun", "conclusion"], "failure"],
  ["QA incomplete", ["qaRun", "status"], "in_progress"],
  ["skipped Windows never shadow pass", ["qaJobs", 0, "conclusion"], "skipped"],
  ["failed Windows without shadow", ["qaJobs", 0, "conclusion"], "failure"],
  ["failed Linux", ["qaJobs", 1, "conclusion"], "failure"],
  ["failed mac", ["qaJobs", 2, "conclusion"], "failure"],
  ["cancelled mac", ["qaJobs", 3, "conclusion"], "cancelled"],
  ["incomplete job", ["qaJobs", 2, "status"], "in_progress"],
  ["wrong job run", ["qaJobs", 1, "run_id"], 99],
  ["wrong job head", ["qaJobs", 1, "head_sha"], "c".repeat(40)],
  ["wrong job branch", ["qaJobs", 0, "head_branch"], "feature"],
  ["missing job branch", ["qaJobs", 0, "head_branch"], undefined],
  ["wrong job attempt", ["qaJobs", 1, "run_attempt"], 2],
  [
    "lookalike job name",
    ["qaJobs", 1, "name"],
    "Prefix / Final package QA / linux/x86_64",
  ],
  ["missing jobs", ["qaJobs"], []],
  ["missing artifacts", ["qaArtifacts"], []],
  ["missing payloads", ["evidence"], []],
  ["expired QA artifact", ["qaArtifacts", 0, "expired"], true],
  [
    "past QA expiration",
    ["qaArtifacts", 0, "expires_at"],
    "2026-11-13T04:14:59Z",
  ],
  ["wrong evidence id", ["qaArtifacts", 0, "id"], 999],
  [
    "wrong evidence digest",
    ["qaArtifacts", 0, "digest"],
    "sha256:" + "f".repeat(64),
  ],
  ["wrong artifact run", ["qaArtifacts", 0, "workflow_run", "id"], 99],
  [
    "wrong artifact head",
    ["qaArtifacts", 0, "workflow_run", "head_sha"],
    "c".repeat(40),
  ],
  [
    "wrong package artifact digest",
    ["packageArtifacts", 1, "digest"],
    "sha256:" + "f".repeat(64),
  ],
  ["expired bundle", ["packageArtifacts", 0, "expired"], true],
  ["wrong bundle run", ["packageArtifacts", 0, "workflow_run", "id"], 99],
  [
    "stale evidence attempt",
    ["qaArtifacts", 0, "created_at"],
    "2026-11-13T04:05:59Z",
  ],
  ["future evidence", ["qaArtifacts", 0, "created_at"], "2026-11-13T04:16:00Z"],
  ["stale job", ["qaJobs", 0, "started_at"], "2026-11-13T04:05:00Z"],
  ["job after run", ["qaJobs", 0, "completed_at"], "2026-11-13T04:13:00Z"],
  ["future run", ["qaRun", "updated_at"], "2026-11-13T04:16:00Z"],
  [
    "observed digest mismatch",
    ["evidence", 0, "observedArchiveDigest"],
    "sha256:" + "f".repeat(64),
  ],
  ["missing input", ["evidence", 0, "entries", 0, "name"], "other.json"],
  ["missing post", ["evidence", 0, "entries", 1, "name"], "other.json"],
  ["poisoned run despite valid API", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "runId",
  ], 999],
  [
    "poisoned source head",
    ["evidence", 0, "entries", 0, "value", "floorpHead"],
    "c".repeat(40),
  ],
  [
    "poisoned probe head",
    ["evidence", 0, "entries", 0, "value", "probeHead"],
    "c".repeat(40),
  ],
  [
    "unsigned attestation",
    ["evidence", 0, "entries", 0, "value", "unsigned"],
    true,
  ],
  ["wrong target", ["evidence", 0, "entries", 0, "value", "platform"], "linux"],
  ["wrong Floorp version", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "floorpVersion",
  ], "99.0.0"],
  ["wrong Firefox version", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "firefoxVersion",
  ], "99.0"],
  [
    "wrong BuildID",
    ["evidence", 0, "entries", 0, "value", "runtimeBuildId"],
    "20260101000000",
  ],
  ["wrong selected package", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "package",
    "id",
  ], 999],
  ["wrong selected record", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "record",
    "digest",
  ], "sha256:" + "f".repeat(64)],
  ["wrong pre phase", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "sourceEligibility",
    "phase",
  ], "completed"],
  ["pre approved", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "sourceEligibility",
    "publicationApprovalPerformed",
  ], true],
  ["wrong pre bundle", [
    "evidence",
    0,
    "entries",
    0,
    "value",
    "sourceEligibility",
    "bundle",
    "id",
  ], 999],
  ["post approved", [
    "evidence",
    0,
    "entries",
    1,
    "value",
    "sourceEligibility",
    "publicationApprovalPerformed",
  ], true],
  ["wrong post phase", [
    "evidence",
    0,
    "entries",
    1,
    "value",
    "sourceEligibility",
    "phase",
  ], "completed"],
  ["wrong post bundle", [
    "evidence",
    0,
    "entries",
    1,
    "value",
    "sourceEligibility",
    "bundle",
    "digest",
  ], "sha256:" + "f".repeat(64)],
  [
    "future post",
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-11-13T04:16:00Z",
  ],
  [
    "post after upload",
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-11-13T04:10:01Z",
  ],
  [
    "post before job",
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-11-13T04:06:00Z",
  ],
  [
    "invalid timestamp",
    ["evidence", 0, "entries", 1, "value", "checkedAt"],
    "2026-02-30T04:09:00Z",
  ],
  [
    "path alias",
    ["evidence", 0, "entries", 1, "name"],
    "./source-after-qa.json",
  ],
];
for (const [name, path, value] of badFields) {
  Deno.test("rejects " + name, () => {
    const { expected, observed } = fixture();
    poison(observed, path, value);
    assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
  });
}

Deno.test("rejects duplicate jobs/artifacts/files even with good alternatives", () => {
  for (
    const path of [
      "qaJobs",
      "qaArtifacts",
      "packageArtifacts",
      "evidence",
    ] as const
  ) {
    const { expected, observed } = fixture();
    const list = observed[path];
    poison(observed, [path, list.length], structuredClone(list[0]));
    assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
  }
  const { expected, observed } = fixture();
  observed.evidence[0].entries.push(
    structuredClone(observed.evidence[0].entries[0]),
  );
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});

Deno.test("rejects absent policy, stale scope and malformed unknown data", () => {
  const { expected, observed } = fixture();
  for (const value of [undefined, "implicit-shadow", true]) {
    const policy = structuredClone(expected);
    poison(policy, ["windowsPolicy"], value);
    assertThrows(() => validateFinalPackageQaConsistency(policy, observed));
  }
  for (const value of [null, [], false, "passed", { archiveVerified: true }]) {
    assertThrows(() => validateFinalPackageQaConsistency(expected, value));
  }
  expected.maxAgeMs = 60_000;
  assertThrows(() => validateFinalPackageQaConsistency(expected, observed));
});
