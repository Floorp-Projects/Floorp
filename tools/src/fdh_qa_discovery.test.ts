// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { zipSync } from "fflate";
import {
  discoverAndCollectFinalPackageQa,
  QA_DISCOVERY_LIMITS,
} from "./fdh_qa_discovery.ts";
import type {
  QaDiscoveryCandidateSpec,
  QaDiscoveryOptions,
} from "./fdh_qa_discovery_types.ts";
import type {
  QaCollectionReport,
  QaCollectorDependencies,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";
import type {
  QaGhOperation,
  QaGhResult,
  QaGhTransport,
} from "./fdh_qa_gh_transport_types.ts";
import type {
  FinalQaArtifactIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";

// All metadata and ZIPs are synthetic. Tests run without any grants.
const HEAD = "a".repeat(40);
const PROBE = "b".repeat(40);
const WRONG = "c".repeat(40);
const SOURCE_RUN_ID = 10;
const QA_PATH = ".github/workflows/validate-final-package-qa.yml";
const SOURCE_PATH = ".github/workflows/package_and_publish_release.yml";
const TARGETS: FinalQaTarget[] = [
  "windows/x86_64",
  "linux/x86_64",
  "mac/x86_64",
  "mac/aarch64",
];
const NOW = "2026-11-13T04:15:00Z";
const SINCE = "2026-11-13T04:00:00.000Z";
const OPTIONS: QaDiscoveryOptions = {
  sourceRunId: SOURCE_RUN_ID,
  sourceAttempt: 1,
  sourceHead: HEAD,
  windowsPolicy: "require-success",
  maxAgeMs: 600000,
};
const REPO = { id: 437505831, full_name: "Floorp-Projects/Floorp" };
const stamp = (minute: number) =>
  "2026-11-13T04:" + String(minute).padStart(2, "0") + ":00Z";
const ref = (id: number, name: string): FinalQaArtifactIdentity => ({
  id,
  name,
  digest: "sha256:" + id.toString(16).padStart(64, "0"),
});
const json = (v: unknown) => JSON.stringify(v, null, 2);
const encoder = new TextEncoder();
const meta = (v: unknown): QaGhResult => ({
  ok: true,
  kind: "metadata",
  jsonText: json(v),
  publicationAuthorized: false,
});
const missing: QaGhResult = {
  ok: false,
  code: "http",
  statusClass: "notFound",
};
const serverError: QaGhResult = {
  ok: false,
  code: "http",
  statusClass: "serverError",
};

const valid: QaDiscoveryCandidateSpec = {
  id: 20,
  boundSourceRunId: SOURCE_RUN_ID,
  windows: "success",
};
const wrongSource: QaDiscoveryCandidateSpec = {
  id: 21,
  boundSourceRunId: 99,
  windows: "success",
};

function listedRun(
  id: number,
  overrides: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    id,
    workflow_id: 377351757,
    run_attempt: 1,
    repository: { ...REPO },
    path: QA_PATH,
    head_sha: PROBE,
    head_branch: "main",
    event: "workflow_dispatch",
    status: "completed",
    conclusion: "success",
    created_at: stamp(5),
    run_started_at: stamp(6),
    updated_at: stamp(12),
    ...overrides,
  };
}

function content(v: unknown, file: "package" | "runtimeLock") {
  const path = file === "package" ? "package.json" : "floorp-runtime.lock.json";
  const text = json(v);
  return {
    type: "file",
    name: path,
    path,
    encoding: "base64",
    content: btoa(text),
    size: encoder.encode(text).length,
  };
}

const RUNTIME = {
  schemaVersion: 1,
  artifacts: [{
    platform: "windows",
    architecture: "x86_64",
    version: "140.0",
    buildId: "20261113000000",
  }, {
    platform: "linux",
    architecture: "x86_64",
    version: "140.0",
    buildId: "20261113000000",
  }, {
    platform: "macos",
    architecture: "universal",
    version: "140.0",
    buildId: "20261113000000",
  }],
};
const PENDING = [{
  environment: { id: 1, name: "Deploy-to-installer-release" },
  reviewers: [{ type: "User", reviewer: { id: 2 } }],
}];
const packageRun = {
  id: SOURCE_RUN_ID,
  workflow_id: 163541311,
  run_attempt: 1,
  repository: { ...REPO },
  path: SOURCE_PATH,
  head_sha: HEAD,
  head_branch: "main",
  event: "workflow_dispatch",
  status: "waiting",
  conclusion: null,
  created_at: stamp(0),
  run_started_at: stamp(0),
  updated_at: stamp(14),
};
const bundle = ref(401, "floorp-release-bundle-v2");
const sourceSelections = new Map(
  TARGETS.map((target, index) => {
    const [platform, arch] = target.split("/");
    return [target, {
      package: platform === "mac"
        ? ref(103, "noraneko-mac-universal-installer")
        : ref(101 + index, "noraneko-" + platform + "-" + arch + "-installer"),
      record: ref(
        201 + index,
        "floorp-native-verification-" + platform + "-" + arch + "-v2",
      ),
      floorpVersion: "12.3.4",
      firefoxVersion: "140.0",
      runtimeBuildId: "20261113000000",
    }];
  }),
);
function apiArtifact(
  identity: FinalQaArtifactIdentity,
  runId: number,
  head: string,
) {
  return {
    ...identity,
    expired: false,
    created_at: stamp(4),
    expires_at: "2026-12-13T04:00:00Z",
    workflow_run: {
      id: runId,
      head_sha: head,
      head_branch: "main",
      repository_id: 437505831,
      head_repository_id: 437505831,
    },
  };
}
const packageRefs = [
  bundle,
  ...[...sourceSelections.values()].flatMap((s) => [s.package, s.record]),
];
const packageArtifacts = [
  ...new Map(packageRefs.map((r) => [r.id, r])).values(),
].map((identity) => apiArtifact(identity, SOURCE_RUN_ID, HEAD));
const sourceJobs: Record<string, unknown>[] = [
  "windows/x86_64",
  "linux/x86_64",
  "linuxAarch64/aarch64",
  "mac/x86_64",
  "mac/aarch64",
].map((t) => "Verify " + t + " package").concat([
  "Validate provenance and assemble the flat release bundle",
  "Validate immutable bundle and create draft release",
  "Manual approval and public release gate",
]).map((name, index) => ({
  id: 700 + index,
  name,
  run_id: SOURCE_RUN_ID,
  run_attempt: 1,
  head_sha: HEAD,
  head_branch: "main",
  status: index === 7 ? "waiting" : "completed",
  conclusion: index === 7 ? null : "success",
  started_at: stamp(1),
  completed_at: index === 7 ? null : stamp(5),
}));

async function candidateData(spec: QaDiscoveryCandidateSpec, index: number) {
  const attempt = spec.dataAttempt ?? 1;
  const count = spec.jobs ?? 4;
  const eligibility = {
    phase: "awaiting-publication-approval",
    bundle: { ...bundle },
    publicationApprovalPerformed: false,
  };
  const evidence = TARGETS.slice(0, count).map((target, k) => {
    const [platform, arch] = target.split("/");
    const selected = sourceSelections.get(target)!;
    return {
      artifactId: 301 + 10 * index + k,
      name: "final-package-qa-" + platform + "-" + arch + "-" + spec.id,
      entries: [
        {
          name: "input.json",
          value: {
            runId: spec.boundSourceRunId,
            floorpHead: HEAD,
            probeHead: PROBE,
            platform,
            arch,
            unsigned: false,
            floorpVersion: selected.floorpVersion,
            firefoxVersion: selected.firefoxVersion,
            runtimeBuildId: selected.runtimeBuildId,
            sourceEligibility: structuredClone(eligibility),
            package: { ...selected.package },
            record: { ...selected.record },
          },
        },
        {
          name: "source-after-qa.json",
          value: {
            sourceEligibility: structuredClone(eligibility),
            checkedAt: stamp(9),
          },
        },
      ],
    };
  });
  const qaRun = {
    id: spec.id,
    workflow_id: 377351757,
    run_attempt: attempt,
    repository: { ...REPO },
    path: QA_PATH,
    head_sha: PROBE,
    head_branch: "main",
    event: "workflow_dispatch",
    status: "completed",
    // The gate requires the qa RUN conclusion to be success; only the
    // windows JOB may fail under the explicit shadow policy.
    conclusion: "success",
    created_at: stamp(5),
    run_started_at: stamp(6),
    updated_at: stamp(12),
  };
  const qaJobs = TARGETS.slice(0, count).map((target, k) => ({
    id: 501 + 10 * index + k,
    name: "Final package QA / " + target,
    run_id: spec.id,
    run_attempt: attempt,
    head_sha: PROBE,
    head_branch: "main",
    status: "completed",
    conclusion: k === 0 ? spec.windows : "success",
    started_at: stamp(7),
    completed_at: stamp(11),
  }));
  const qaArtifacts = evidence.map((e) => ({
    ...apiArtifact(
      { id: e.artifactId, name: e.name, digest: "sha256:" + "0".repeat(64) },
      spec.id,
      PROBE,
    ),
  }));
  // Evidence artifacts are created by their target jobs, after the
  // source-after-qa.json check instant the archives must carry.
  for (const a of qaArtifacts) a.created_at = stamp(10);
  const archives = new Map<number, Uint8Array>();
  for (let k = 0; k < evidence.length; k++) {
    const payload = evidence[k];
    const bytes = zipSync(
      Object.fromEntries(
        payload.entries.map((e) => [e.name, encoder.encode(json(e.value))]),
      ),
      { level: 0 },
    );
    const hash = new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    );
    const digest = "sha256:" +
      Array.from(hash, (v) => v.toString(16).padStart(2, "0")).join("");
    qaArtifacts[k].digest = digest;
    archives.set(payload.artifactId, bytes);
  }
  return { spec, qaRun, qaJobs, qaArtifacts, archives };
}

async function fixture(
  specs: QaDiscoveryCandidateSpec[],
  opts: { listed?: unknown[]; listedTotal?: number } = {},
) {
  const candidates = await Promise.all(
    specs.map((spec, index) => candidateData(spec, index)),
  );
  const listed = opts.listed ?? specs.map((s) => listedRun(s.id));
  const listedTotal = opts.listedTotal ?? listed.length;
  const calls: QaGhOperation[] = [];
  const counts = new Map<string, number>();
  type Hook = (
    op: QaGhOperation,
    value: QaGhResult,
    ordinal: number,
  ) => QaGhResult | Promise<QaGhResult>;
  let hook: Hook = (_op, value) => value;
  const transport: QaGhTransport = {
    async request(op) {
      calls.push(op);
      const key = json(op);
      const ordinal = (counts.get(key) ?? 0) + 1;
      counts.set(key, ordinal);
      let value: QaGhResult;
      switch (op.kind) {
        case "repository":
          value = meta({ ...REPO });
          break;
        case "workflow":
          value = meta({
            id: op.workflow === "source" ? 163541311 : 377351757,
            path: op.workflow === "source" ? SOURCE_PATH : QA_PATH,
            state: "active",
          });
          break;
        case "currentRun":
        case "attemptRun":
          if (op.runId === SOURCE_RUN_ID) value = meta(packageRun);
          else {
            const c = candidates.find((x) => x.spec.id === op.runId);
            value = c ? meta(c.qaRun) : missing;
          }
          break;
        case "qaRunsPage":
          value = meta({
            total_count: listedTotal,
            workflow_runs: listed.slice((op.page - 1) * 100, op.page * 100),
          });
          break;
        case "jobsPage": {
          const jobs = op.runId === SOURCE_RUN_ID
            ? sourceJobs
            : candidates.find((x) => x.spec.id === op.runId)?.qaJobs ?? [];
          value = meta({
            total_count: jobs.length,
            jobs: jobs.slice((op.page - 1) * 100, op.page * 100),
          });
          break;
        }
        case "artifactsPage": {
          const artifacts = op.runId === SOURCE_RUN_ID
            ? packageArtifacts
            : candidates.find((x) => x.spec.id === op.runId)?.qaArtifacts ?? [];
          value = meta({
            total_count: artifacts.length,
            artifacts: artifacts.slice((op.page - 1) * 100, op.page * 100),
          });
          break;
        }
        case "pinnedContents":
          assertEquals(op.head, HEAD);
          value = meta(
            content(
              op.file === "package" ? { version: "12.3.4" } : RUNTIME,
              op.file,
            ),
          );
          break;
        case "pendingDeployments":
          value = meta(structuredClone(PENDING));
          break;
        case "releaseTag":
          value = missing;
          break;
        case "artifactMetadata": {
          const found = candidates
            .flatMap((c) => c.qaArtifacts)
            .find((a) => a.id === op.artifactId);
          value = found ? meta(found) : missing;
          break;
        }
        case "selectedZip": {
          let bytes: Uint8Array | undefined;
          for (const c of candidates) {
            const b = c.archives.get(op.artifactId);
            if (b) bytes = b;
          }
          assert(bytes);
          value = {
            ok: true,
            kind: "zip",
            bytes: new Uint8Array(bytes),
            publicationAuthorized: false,
          };
          break;
        }
      }
      return await hook(op, value, ordinal);
    },
  };
  return {
    calls,
    transport,
    setHook: (v: Hook) => {
      hook = v;
    },
    run: (
      options: QaDiscoveryOptions = OPTIONS,
      now: () => string = () => NOW,
    ) => discoverAndCollectFinalPackageQa(options, { transport, now }),
  };
}

function safe(
  result: Awaited<ReturnType<typeof discoverAndCollectFinalPackageQa>>,
) {
  for (
    const key of [
      "transportAuthenticityVerified",
      "apiAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
      "publicationAuthorized",
    ] as const
  ) assertEquals(result[key], false);
  const text = json(result);
  assert(text.length <= 256 * 1024);
  assert(!text.includes("SECRET"));
}

const mutateMeta = (
  result: QaGhResult,
  fn: (v: Record<string, unknown>) => void,
): QaGhResult => {
  assert(result.ok && result.kind === "metadata");
  const v = JSON.parse(result.jsonText);
  fn(v);
  return meta(v);
};

Deno.test("discovery limits are the frozen bounded constants", () => {
  assertEquals(QA_DISCOVERY_LIMITS, {
    pages: 3,
    pageSize: 100,
    runs: 300,
    candidates: 8,
  });
});

Deno.test("discovery unique valid qa run collects through the real collector", async () => {
  const t = await fixture([valid]);
  const r = await t.run();
  safe(r);
  assertEquals(r.matches, true);
  if (r.matches) {
    assertEquals(r.sourceRunId, SOURCE_RUN_ID);
    assertEquals(r.sourceAttempt, 1);
    assertEquals(r.sourceHead, HEAD);
    assertEquals(r.qaRunId, 20);
    assertEquals(r.qaAttempt, 1);
    assertEquals(r.qaHead, PROBE);
    assertEquals(r.checkedAt, NOW);
    assertEquals(r.archiveBytesVerified, true);
    assertEquals(r.runsListed, 1);
    assertEquals(r.pagesFetched, 1);
    assertEquals(r.candidatesChecked, 1);
  }
  assert(
    t.calls.some((c) =>
      c.kind === "qaRunsPage" && c.since === SINCE && c.page === 1
    ),
  );
  // Initial scan listing + final race-guard re-listing of the same window.
  assertEquals(t.calls.filter((c) => c.kind === "qaRunsPage").length, 2);
  // Scan collect + fresh final recheck, each downloading exactly four ZIPs.
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 8);
});

Deno.test("discovery wrong-source competitor fails closed without hidden skip", async () => {
  const t = await fixture([wrongSource, valid]);
  const r = await t.run();
  safe(r);
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
  // The collector verified the valid candidate (four ZIPs) but the
  // unprovable wrong-source competitor still blocks selection.
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 4);
});

Deno.test("discovery wrong-source-only candidate is unprovable uncertainty", async () => {
  const t = await fixture([wrongSource]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery explicit shadow accepts Windows failed qa run", async () => {
  const t = await fixture([{
    id: 30,
    boundSourceRunId: SOURCE_RUN_ID,
    windows: "failure",
  }]);
  const r = await t.run({ ...OPTIONS, windowsPolicy: "shadow-observation" });
  safe(r);
  assertEquals(r.matches, true);
  if (r.matches) assertEquals(r.qaRunId, 30);
});

Deno.test("discovery require-success windows failure fails closed", async () => {
  const t = await fixture([{
    id: 30,
    boundSourceRunId: SOURCE_RUN_ID,
    windows: "failure",
  }]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery rejects more than one valid qa run", async () => {
  const t = await fixture([
    valid,
    { id: 21, boundSourceRunId: SOURCE_RUN_ID, windows: "success" },
  ]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "ambiguous");
  }
});

Deno.test("discovery with no qa run in window yields noValidRun", async () => {
  const t = await fixture([], { listed: [] });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "noValidRun");
  }
  // noValidRun exits before the final re-listing.
  assertEquals(t.calls.filter((c) => c.kind === "qaRunsPage").length, 1);
});

for (
  const [key, value] of Object.entries({
    sourceRunId: 0,
    sourceAttempt: -1,
    sourceHead: "A".repeat(40),
    windowsPolicy: "implicit",
    maxAgeMs: 86400001,
    extra: "SECRET",
  })
) {
  Deno.test("discovery invalid option " + key, async () => {
    const t = await fixture([]);
    const r = await t.run({ ...OPTIONS, [key]: value } as QaDiscoveryOptions);
    safe(r);
    assertEquals(r.matches, false);
    if (!r.matches) {
      assertEquals(r.stage, "options");
      assertEquals(r.code, "invalidOptions");
    }
    assertEquals(t.calls.length, 0);
  });
}

for (const value of [0, -1, NaN, Infinity, 86400001, 0.5]) {
  Deno.test("discovery invalid freshness " + value, async () => {
    const t = await fixture([]);
    assertEquals((await t.run({ ...OPTIONS, maxAgeMs: value })).matches, false);
    assertEquals(t.calls.length, 0);
  });
}

Deno.test("discovery accessor options reject without invocation", async () => {
  const t = await fixture([]);
  let reads = 0;
  const options = { ...OPTIONS };
  Object.defineProperty(options, "sourceHead", {
    get() {
      reads++;
      return OPTIONS.sourceHead;
    },
  });
  assertEquals((await t.run(options)).matches, false);
  assertEquals(reads, 0);
  assertEquals(t.calls.length, 0);
});

Deno.test("discovery invalid source clock prevents all transport IO", async () => {
  const t = await fixture([]);
  const r = await t.run(OPTIONS, () => "SECRET");
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "options");
    assertEquals(r.code, "invalidOptions");
  }
  assertEquals(t.calls.length, 0);
});

Deno.test("discovery source run transport failure", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) =>
    op.kind === "currentRun" && op.runId === SOURCE_RUN_ID ? serverError : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "source");
    assertEquals(r.code, "sourceRunFailed");
  }
  assertEquals(t.calls.filter((c) => c.kind === "qaRunsPage").length, 0);
});

Deno.test("discovery source run created_at malformed", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) =>
    op.kind === "currentRun" && op.runId === SOURCE_RUN_ID
      ? mutateMeta(v, (x) => {
        x.created_at = "SECRET";
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "source");
    assertEquals(r.code, "sourceRunFailed");
  }
});

Deno.test("discovery source run head mismatch", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) =>
    op.kind === "currentRun" && op.runId === SOURCE_RUN_ID
      ? mutateMeta(v, (x) => {
        x.head_sha = WRONG;
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "source");
    assertEquals(r.code, "sourceRunFailed");
  }
});

Deno.test("discovery source run attempt mismatch", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) =>
    op.kind === "currentRun" && op.runId === SOURCE_RUN_ID
      ? mutateMeta(v, (x) => {
        x.run_attempt = 2;
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "source");
    assertEquals(r.code, "sourceRunFailed");
  }
});

for (
  const [label, mutate] of Object.entries({
    forkedRepo: (x: Record<string, unknown>) => {
      x.repository = { id: 999, full_name: "Floorp-Projects/Floorp" };
    },
    wrongWorkflowPath: (x: Record<string, unknown>) => {
      x.path = QA_PATH;
    },
    wrongEvent: (x: Record<string, unknown>) => {
      x.event = "push";
    },
    wrongBranch: (x: Record<string, unknown>) => {
      x.head_branch = "feature-x";
    },
    zeroWorkflowId: (x: Record<string, unknown>) => {
      x.workflow_id = 0;
    },
    completedSource: (x: Record<string, unknown>) => {
      x.status = "completed";
      x.conclusion = "success";
    },
  })
) {
  Deno.test(
    "discovery source preflight rejects " + label + " before listing",
    async () => {
      const t = await fixture([valid]);
      t.setHook((op, v) =>
        op.kind === "currentRun" && op.runId === SOURCE_RUN_ID
          ? mutateMeta(v, mutate)
          : v
      );
      const r = await t.run();
      assertEquals(r.matches, false);
      if (!r.matches) {
        assertEquals(r.stage, "source");
        assertEquals(r.code, "sourceRunFailed");
      }
      // Preflight happens strictly before any listing query.
      assertEquals(t.calls.filter((c) => c.kind === "qaRunsPage").length, 0);
    },
  );
}

Deno.test("discovery qaRunsPage transport failure", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) => op.kind === "qaRunsPage" ? serverError : v);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "runListFailed");
  }
});

Deno.test("discovery qaRunsPage malformed body", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) =>
    op.kind === "qaRunsPage"
      ? {
        ok: true,
        kind: "metadata",
        jsonText: "SECRET{",
        publicationAuthorized: false,
      }
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "runListFailed");
  }
});

Deno.test("discovery qaRunsPage null body", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v) => op.kind === "qaRunsPage" ? meta(null) : v);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "runListFailed");
  }
});

for (
  const [label, overrides] of Object.entries({
    wrongEvent: { event: "push" },
    uppercaseHead: { head_sha: "B".repeat(40) },
    shortHead: { head_sha: WRONG.slice(0, 39) },
    garbageHead: { head_sha: "url" },
    wrongPath: { path: SOURCE_PATH },
    wrongBranch: { head_branch: "feature-x" },
    wrongRepo: { repository: { id: 1, full_name: "Floorp-Projects/Floorp" } },
    fractionalAttempt: { run_attempt: 1.5 },
    beforeWindow: { created_at: "2026-11-13T03:59:00Z" },
    // action_required is a CONCLUSION, not a canonical run status.
    actionRequiredAsStatus: { status: "action_required" },
  })
) {
  Deno.test("discovery run listing rejects " + label, async () => {
    const t = await fixture([], { listed: [listedRun(20, overrides)] });
    const r = await t.run();
    assertEquals(r.matches, false);
    if (!r.matches) {
      assertEquals(r.stage, "runList");
      assertEquals(r.code, "runListFailed");
    }
    assert(t.calls.some((c) => c.kind === "currentRun"));
  });
}

for (
  const status of [
    "queued",
    "in_progress",
    "waiting",
    "requested",
    "pending",
  ] as const
) {
  Deno.test(
    "discovery run listing rejects unsettled status " + status,
    async () => {
      const t = await fixture([], {
        listed: [listedRun(20, { status, conclusion: null })],
      });
      const r = await t.run();
      assertEquals(r.matches, false);
      if (!r.matches) {
        assertEquals(r.stage, "runList");
        assertEquals(r.code, "pendingQaRun");
      }
    },
  );
}

Deno.test("discovery run listing rejects duplicate run ids", async () => {
  const t = await fixture([], { listed: [listedRun(20), listedRun(20)] });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "runListFailed");
  }
});

Deno.test("discovery rejects listing beyond bounded pages", async () => {
  const t = await fixture([], {
    listed: [listedRun(20)],
    listedTotal: QA_DISCOVERY_LIMITS.runs + 1,
  });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "paginationIncomplete");
  }
  assertEquals(t.calls.filter((c) => c.kind === "qaRunsPage").length, 1);
});

Deno.test("discovery rejects truncated listing", async () => {
  const t = await fixture([valid], { listedTotal: 130 });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "runListFailed");
  }
});

Deno.test("discovery rejects candidate cap overflow", async () => {
  const t = await fixture([], {
    listed: Array.from({ length: 9 }, (_, i) => listedRun(20 + i)),
  });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "runList");
    assertEquals(r.code, "candidateCapExceeded");
  }
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 0);
});

Deno.test("discovery candidate data missing fails closed", async () => {
  const t = await fixture([], { listed: [listedRun(20)] });
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery candidate attempt data mismatch fails closed", async () => {
  const t = await fixture([
    {
      id: 20,
      boundSourceRunId: SOURCE_RUN_ID,
      windows: "success",
      dataAttempt: 2,
    },
  ]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery incomplete candidate inventory is uncertain", async () => {
  const t = await fixture([
    valid,
    { id: 21, boundSourceRunId: SOURCE_RUN_ID, windows: "success", jobs: 3 },
  ]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 4);
});

Deno.test("discovery uncertainty before valid candidate fails closed", async () => {
  const t = await fixture([
    { id: 21, boundSourceRunId: SOURCE_RUN_ID, windows: "success", jobs: 3 },
    valid,
  ]);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery final recheck rejects changed pending deployments", async () => {
  const t = await fixture([valid]);
  // Only the final collect's recheck read (ordinal 4) mutates: the final
  // collect's own eligibility read stays clean, so its internal
  // eligibility comparison catches the change and reports "changed".
  t.setHook((op, v, n) =>
    op.kind === "pendingDeployments" && n >= 4
      ? meta([{
        environment: { id: 1, name: "Deploy-to-installer-release" },
        reviewers: [{ type: "Team", reviewer: { id: 3 } }],
      }])
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "changed");
  }
});

Deno.test("discovery race guard rejects new qa run after the scan", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v, n) =>
    op.kind === "qaRunsPage" && n >= 2
      ? mutateMeta(v, (x) => {
        (x.workflow_runs as unknown[]).push(listedRun(99));
        x.total_count = 2;
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "changed");
  }
  // The final collect itself succeeded before the window re-listing.
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 8);
});

Deno.test("discovery race guard rejects unsettled qa run after the scan", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v, n) =>
    op.kind === "qaRunsPage" && n >= 2
      ? mutateMeta(v, (x) => {
        (x.workflow_runs as unknown[]).push(
          listedRun(99, { status: "in_progress", conclusion: null }),
        );
        x.total_count = 2;
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "pendingQaRun");
  }
});

Deno.test("discovery race guard rejects retried qa run fingerprint", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v, n) =>
    op.kind === "qaRunsPage" && n >= 2
      ? mutateMeta(v, (x) => {
        (x.workflow_runs as Record<string, unknown>[])[0].conclusion =
          "failure";
      })
      : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "changed");
  }
});

Deno.test("discovery race guard re-list transport failure fails closed", async () => {
  const t = await fixture([valid]);
  t.setHook((op, v, n) => op.kind === "qaRunsPage" && n >= 2 ? serverError : v);
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "finalRecheckFailed");
  }
});

Deno.test("discovery final recheck transport uncertainty fails closed", async () => {
  const t = await fixture([valid]);
  // qa jobsPage: scan collect reads ordinals 1-2, final collect 3-4;
  // failing ordinal 3 hits only the final collect's inventory.
  t.setHook((op, v, n) =>
    op.kind === "jobsPage" && op.runId === 20 && n >= 3 ? serverError : v
  );
  const r = await t.run();
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "finalRecheck");
    assertEquals(r.code, "finalRecheckFailed");
  }
});

Deno.test("discovery collect seam shares one transport across candidates", async () => {
  const t = await fixture([], {
    listed: [listedRun(20), listedRun(21)],
  });
  const seen: { options: QaCollectorOptions; transport: unknown }[] = [];
  const collect = (
    options: QaCollectorOptions,
    deps?: QaCollectorDependencies,
  ): Promise<QaCollectionReport> => {
    seen.push({ options, transport: deps?.transport });
    return Promise.resolve({
      scope: "read-only-point-in-time-qa-collection",
      transportAuthenticityVerified: false,
      apiAuthenticityVerified: false,
      nativeQaVerified: false,
      liveApprovalVerified: false,
      publicationAuthorized: false,
      matches: true,
      sourceRunId: options.sourceRunId,
      sourceAttempt: options.sourceAttempt,
      qaRunId: options.qaRunId,
      qaAttempt: options.qaAttempt,
      checkedAt: NOW,
      archiveBytesVerified: true,
    });
  };
  const r = await discoverAndCollectFinalPackageQa(OPTIONS, {
    transport: t.transport,
    now: () => NOW,
    collect,
  });
  assertEquals(seen.length, 2);
  assertEquals(seen[0].options.qaRunId, 20);
  assertEquals(seen[0].options.qaHead, PROBE);
  assertEquals(seen[0].options.qaAttempt, 1);
  assertEquals(seen[0].options.sourceRunId, SOURCE_RUN_ID);
  assertEquals(seen[0].options.sourceHead, HEAD);
  assertEquals(seen[0].options.windowsPolicy, "require-success");
  assertEquals(seen[0].options.maxAgeMs, 600000);
  assertEquals(seen[0].transport, t.transport);
  assertEquals(seen[1].transport, t.transport);
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "ambiguous");
  }
});

Deno.test("discovery collect seam positive selects the candidate", async () => {
  const t = await fixture([valid]);
  const r = await discoverAndCollectFinalPackageQa(OPTIONS, {
    transport: t.transport,
    now: () => NOW,
    collect: (options) =>
      Promise.resolve({
        scope: "read-only-point-in-time-qa-collection",
        transportAuthenticityVerified: false,
        apiAuthenticityVerified: false,
        nativeQaVerified: false,
        liveApprovalVerified: false,
        publicationAuthorized: false,
        matches: true,
        sourceRunId: options.sourceRunId,
        sourceAttempt: options.sourceAttempt,
        qaRunId: options.qaRunId,
        qaAttempt: options.qaAttempt,
        checkedAt: NOW,
        archiveBytesVerified: true,
      }),
  });
  safe(r);
  assertEquals(r.matches, true);
  if (r.matches) {
    assertEquals(r.qaRunId, 20);
    assertEquals(r.qaHead, PROBE);
    assertEquals(r.candidatesChecked, 1);
  }
});

Deno.test("discovery collect seam evidenceMismatch fails closed", async () => {
  const t = await fixture([valid]);
  const r = await discoverAndCollectFinalPackageQa(OPTIONS, {
    transport: t.transport,
    now: () => NOW,
    collect: () =>
      Promise.resolve({
        scope: "read-only-point-in-time-qa-collection",
        transportAuthenticityVerified: false,
        apiAuthenticityVerified: false,
        nativeQaVerified: false,
        liveApprovalVerified: false,
        publicationAuthorized: false,
        matches: false,
        stage: "evidence",
        code: "evidenceMismatch",
      }),
  });
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery collect seam uncertainty fails closed", async () => {
  const t = await fixture([valid]);
  const r = await discoverAndCollectFinalPackageQa(OPTIONS, {
    transport: t.transport,
    now: () => NOW,
    collect: () =>
      Promise.resolve({
        scope: "read-only-point-in-time-qa-collection",
        transportAuthenticityVerified: false,
        apiAuthenticityVerified: false,
        nativeQaVerified: false,
        liveApprovalVerified: false,
        publicationAuthorized: false,
        matches: false,
        stage: "identity",
        code: "collectionFailed",
      }),
  });
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
});

Deno.test("discovery collect seam lying approval flags are rejected", async () => {
  const t = await fixture([valid]);
  const r = await discoverAndCollectFinalPackageQa(OPTIONS, {
    transport: t.transport,
    now: () => NOW,
    collect: () =>
      Promise.resolve({
        scope: "read-only-point-in-time-qa-collection",
        transportAuthenticityVerified: false,
        apiAuthenticityVerified: false,
        nativeQaVerified: false,
        liveApprovalVerified: false,
        publicationAuthorized: true,
        matches: true,
        sourceRunId: SOURCE_RUN_ID,
        sourceAttempt: 1,
        qaRunId: 20,
        qaAttempt: 1,
        checkedAt: NOW,
        archiveBytesVerified: true,
      } as unknown as QaCollectionReport),
  });
  assertEquals(r.matches, false);
  if (!r.matches) {
    assertEquals(r.stage, "candidates");
    assertEquals(r.code, "uncertainCandidate");
  }
  safe(r);
});
