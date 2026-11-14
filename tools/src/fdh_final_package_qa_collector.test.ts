// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { zipSync } from "fflate";
import { collectFinalPackageQa } from "./fdh_final_package_qa_collector.ts";
import type { QaCollectorOptions } from "./fdh_final_package_qa_collector_types.ts";
import type {
  QaGhOperation,
  QaGhResult,
  QaGhTransport,
} from "./fdh_qa_gh_transport_types.ts";
import type {
  FinalPackageQaExpectation,
  FinalQaArtifactIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";
// All metadata and ZIPs are synthetic. Tests run without any grants.
function baseFixture() {
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
    repository: { id: 437505831, fullName: "Floorp-Projects/Floorp" },
    packageRun: { id: 10, workflowId: 163541311, attempt: 1, head },
    qaRun: { id: 20, workflowId: 377351757, attempt: 1, head: probe },
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
    workflow_id: qa ? 377351757 : 163541311,
    run_attempt: 1,
    repository: { id: 437505831, full_name: "Floorp-Projects/Floorp" },
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
      repository_id: 437505831,
      head_repository_id: 437505831,
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

const json = (v: unknown) => JSON.stringify(v, null, 2);
const encoder = new TextEncoder();
const OPTIONS: QaCollectorOptions = {
  sourceRunId: 10,
  sourceAttempt: 1,
  sourceHead: "a".repeat(40),
  qaRunId: 20,
  qaAttempt: 1,
  qaHead: "b".repeat(40),
  windowsPolicy: "require-success",
  maxAgeMs: 600000,
};
const NOW = "2026-11-13T04:15:00Z";
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
async function fixture(mutate?: (f: ReturnType<typeof baseFixture>) => void) {
  const f = baseFixture();
  mutate?.(f);
  const archives = new Map<number, Uint8Array>();
  for (let i = 0; i < 4; i++) {
    const payload = f.observed.evidence[i];
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
    f.observed.qaArtifacts[i].digest = digest;
    archives.set(payload.artifactId, bytes);
  }
  const sourceNames = [
    "windows/x86_64",
    "linux/x86_64",
    "linuxAarch64/aarch64",
    "mac/x86_64",
    "mac/aarch64",
  ].map((t) => "Verify " + t + " package");
  sourceNames.push(
    "Validate provenance and assemble the flat release bundle",
    "Validate immutable bundle and create draft release",
    "Manual approval and public release gate",
  );
  const sourceJobs: Record<string, unknown>[] = sourceNames.map((
    name,
    index,
  ) => ({
    id: 700 + index,
    name,
    run_id: 10,
    run_attempt: 1,
    head_sha: OPTIONS.sourceHead,
    head_branch: "main",
    status: index === 7 ? "waiting" : "completed",
    conclusion: index === 7 ? null : "success",
    started_at: "2026-11-13T04:01:00Z",
    completed_at: index === 7 ? null : "2026-11-13T04:05:00Z",
  }));
  const runtime = {
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
  const pending = [{
    environment: { id: 1, name: "Deploy-to-installer-release" },
    reviewers: [{ type: "User", reviewer: { id: 2 } }],
  }];
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
          value = meta({ id: 437505831, full_name: "Floorp-Projects/Floorp" });
          break;
        case "workflow":
          value = meta({
            id: op.workflow === "source" ? 163541311 : 377351757,
            path: op.workflow === "source"
              ? ".github/workflows/package_and_publish_release.yml"
              : ".github/workflows/validate-final-package-qa.yml",
            state: "active",
          });
          break;
        case "currentRun":
        case "attemptRun":
          value = meta(
            op.runId === 10 ? f.observed.packageRun : f.observed.qaRun,
          );
          break;
        case "jobsPage": {
          const jobs = op.runId === 10 ? sourceJobs : f.observed.qaJobs;
          value = meta({
            total_count: jobs.length,
            jobs: jobs.slice((op.page - 1) * 100, op.page * 100),
          });
          break;
        }
        case "artifactsPage": {
          const artifacts = op.runId === 10
            ? f.observed.packageArtifacts
            : f.observed.qaArtifacts;
          value = meta({
            total_count: artifacts.length,
            artifacts: artifacts.slice((op.page - 1) * 100, op.page * 100),
          });
          break;
        }
        case "pinnedContents":
          assertEquals(op.head, OPTIONS.sourceHead);
          value = meta(
            content(
              op.file === "package" ? { version: "12.3.4" } : runtime,
              op.file,
            ),
          );
          break;
        case "pendingDeployments":
          value = meta(pending);
          break;
        case "releaseTag":
          assertEquals(op.version, "12.3.4");
          value = missing;
          break;
        case "artifactMetadata":
          value = meta(
            f.observed.qaArtifacts.find((a) => a.id === op.artifactId),
          );
          break;
        case "selectedZip": {
          const bytes = archives.get(op.artifactId);
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
    f,
    sourceJobs,
    runtime,
    pending,
    calls,
    transport,
    setHook: (v: Hook) => {
      hook = v;
    },
    collect: (options = OPTIONS, now = () => NOW) =>
      collectFinalPackageQa(options, { transport, now }),
  };
}
function safe(result: Awaited<ReturnType<typeof collectFinalPackageQa>>) {
  for (
    const key of [
      "transportAuthenticityVerified",
      "apiAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
      "publicationAuthorized",
    ] as const
  ) assertEquals(result[key], false);
  assert(encoder.encode(json(result)).length <= 256 * 1024);
  assert(!json(result).includes("SECRET"));
}
Deno.test("collector synthetic signed pending match uses real composition and exactly four ZIPs", async () => {
  const t = await fixture();
  const r = await t.collect();
  safe(r);
  assertEquals(r.matches, true);
  assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 4);
  assertEquals(t.calls.filter((c) => c.kind === "artifactMetadata").length, 8);
  assertEquals(
    t.calls.filter((c) => c.kind === "pendingDeployments").length,
    2,
  );
});
for (
  const [key, value] of Object.entries({
    sourceRunId: 0,
    sourceAttempt: -1,
    sourceHead: "A".repeat(40),
    qaRunId: 10,
    qaAttempt: 1.5,
    qaHead: "url",
    windowsPolicy: "implicit",
    maxAgeMs: 86400001,
    extra: "SECRET",
  })
) {
  Deno.test("collector invalid option " + key, async () => {
    const t = await fixture();
    const r = await t.collect(
      { ...OPTIONS, [key]: value } as QaCollectorOptions,
    );
    safe(r);
    assertEquals(r.matches, false);
    assertEquals(t.calls.length, 0);
  });
}
for (const value of [0, -1, NaN, Infinity, Number.MAX_SAFE_INTEGER, 0.5]) {
  Deno.test("collector invalid freshness " + value, async () => {
    const t = await fixture();
    assertEquals(
      (await t.collect({ ...OPTIONS, maxAgeMs: value })).matches,
      false,
    );
    assertEquals(t.calls.length, 0);
  });
}
Deno.test("collector accessor options reject without invocation", async () => {
  const t = await fixture();
  let reads = 0;
  const options = { ...OPTIONS };
  Object.defineProperty(options, "sourceHead", {
    get() {
      reads++;
      return OPTIONS.sourceHead;
    },
  });
  assertEquals((await t.collect(options)).matches, false);
  assertEquals(reads, 0);
  assertEquals(t.calls.length, 0);
});
Deno.test("collector snapshots options before awaits", async () => {
  const t = await fixture();
  const options = { ...OPTIONS };
  t.setHook((op, v) => {
    if (op.kind === "repository") options.sourceHead = "c".repeat(40);
    return v;
  });
  assertEquals((await t.collect(options)).matches, true);
});
const mutateMeta = (
  result: QaGhResult,
  fn: (v: Record<string, unknown>) => void,
): QaGhResult => {
  assert(result.ok && result.kind === "metadata");
  const v = JSON.parse(result.jsonText);
  fn(v);
  return meta(v);
};
for (
  const [label, kind, fn] of [
    ["repository id", "repository", (v: Record<string, unknown>) => {
      v.id = 1;
    }],
    ["workflow path", "workflow", (v: Record<string, unknown>) => {
      v.path = "SECRET";
    }],
    ["current attempt", "currentRun", (v: Record<string, unknown>) => {
      v.run_attempt = 2;
    }],
    ["exact attempt head", "attemptRun", (v: Record<string, unknown>) => {
      v.head_sha = "c".repeat(40);
    }],
  ] as const
) {
  Deno.test("collector rejects " + label, async () => {
    const t = await fixture();
    t.setHook((op, v) => op.kind === kind ? mutateMeta(v, fn) : v);
    const r = await t.collect();
    safe(r);
    assertEquals(r.matches, false);
    assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 0);
  });
}
for (const statusClass of ["unauthorized", "forbidden", "notFound"] as const) {
  for (
    const kind of ["repository", "pendingDeployments", "releaseTag"] as const
  ) {
    Deno.test("collector HTTP " + kind + " " + statusClass, async () => {
      const t = await fixture();
      t.setHook((op, v) =>
        op.kind === kind ? { ok: false, code: "http", statusClass } : v
      );
      const r = await t.collect();
      safe(r);
      assertEquals(
        r.matches,
        kind === "releaseTag" && statusClass === "notFound",
      );
    });
  }
}
for (
  const mode of [
    "completed",
    "public",
    "noReviewers",
    "missingVerify",
    "publicationExecuted",
  ] as const
) {
  Deno.test(
    "collector actual eligibility rejects " + mode + " before ZIP",
    async () => {
      const t = await fixture();
      if (mode === "completed") {
        t.f.observed.packageRun.status = "completed";
        t.f.observed.packageRun.conclusion = "success";
      }
      if (mode === "noReviewers") t.pending[0].reviewers = [];
      if (mode === "missingVerify") t.sourceJobs.splice(0, 1);
      if (mode === "publicationExecuted") {
        t.sourceJobs[7].status = "in_progress";
      }
      if (mode === "public") {
        t.setHook((op, v) =>
          op.kind === "releaseTag"
            ? meta({
              draft: false,
              prerelease: false,
              target_commitish: OPTIONS.sourceHead,
            })
            : v
        );
      }
      const r = await t.collect();
      assertEquals(r.matches, false);
      safe(r);
      assertEquals(t.calls.filter((c) => c.kind === "selectedZip").length, 0);
    },
  );
}
for (
  const mode of [
    "duplicateId",
    "expiredNameDuplicate",
    "truncated",
    "wrongCount",
    "tooManyQA",
    "sourceHead",
  ] as const
) {
  Deno.test("collector inventory rejects " + mode, async () => {
    const t = await fixture();
    t.setHook((op, v) =>
      op.kind === "artifactsPage" && op.runId === 10
        ? mutateMeta(v, (x) => {
          const a = x.artifacts as Record<string, unknown>[];
          if (mode === "duplicateId") a[1].id = a[0].id;
          if (mode === "expiredNameDuplicate") {
            a.push({ ...a[0], id: 999, expired: true });
            x.total_count = a.length;
          }
          if (mode === "truncated") a.pop();
          if (mode === "wrongCount") x.total_count = 1;
          if (mode === "tooManyQA") x.total_count = 65;
          if (mode === "sourceHead") {
            (a[0].workflow_run as Record<string, unknown>).head_sha = "c"
              .repeat(40);
          }
        })
        : v
    );
    assertEquals((await t.collect()).matches, false);
  });
}
for (
  const mode of [
    "packageVersion",
    "runtimeVersion",
    "missingRuntime",
    "duplicateRuntime",
    "wrongSchema",
  ] as const
) {
  Deno.test("collector pinned version rejects " + mode, async () => {
    const t = await fixture();
    if (mode === "runtimeVersion") t.runtime.artifacts[0].version = "141.0";
    if (mode === "missingRuntime") t.runtime.artifacts.pop();
    if (mode === "duplicateRuntime") {
      t.runtime.artifacts.push({ ...t.runtime.artifacts[0] });
    }
    if (mode === "wrongSchema") t.runtime.schemaVersion = 2;
    if (mode === "packageVersion") {
      t.setHook((op, v) =>
        op.kind === "pinnedContents" && op.file === "package"
          ? meta(content({ version: "13.0.0" }, "package"))
          : v
      );
    }
    assertEquals((await t.collect()).matches, false);
  });
}
for (
  const kind of [
    "currentRun",
    "attemptRun",
    "jobsPage",
    "artifactsPage",
    "artifactMetadata",
    "pendingDeployments",
    "releaseTag",
  ] as const
) {
  Deno.test("collector final recheck rejects changed " + kind, async () => {
    const t = await fixture();
    t.setHook((op, v, n) => {
      if (op.kind !== kind || n < 2) return v;
      if (kind === "releaseTag") {
        return meta({
          draft: false,
          prerelease: false,
          target_commitish: OPTIONS.sourceHead,
        });
      }
      return mutateMeta(v, (x) => {
        if (kind === "currentRun" || kind === "attemptRun") x.run_attempt = 2;
        else if (kind === "jobsPage") {
          (x.jobs as Record<string, unknown>[])[0].conclusion = "failure";
        } else if (kind === "artifactsPage") {
          (x.artifacts as Record<string, unknown>[])[0].expired = true;
        } else if (kind === "artifactMetadata") {
          x.digest = "sha256:" + "f".repeat(64);
        } else if (kind === "pendingDeployments") {
          (x as unknown as { reviewers: { reviewer: { id: number } }[] }[])[0]
            .reviewers[0].reviewer.id = 99;
        }
      });
    });
    const r = await t.collect();
    safe(r);
    assertEquals(r.matches, false);
  });
}
Deno.test("collector source job exact run binding", async () => {
  const t = await fixture();
  t.sourceJobs[0].run_id = 999;
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector source jobs paginate through 1000 exactly", async () => {
  const t = await fixture();
  while (t.sourceJobs.length < 1000) {
    t.sourceJobs.push({
      ...t.sourceJobs[0],
      id: 1000 + t.sourceJobs.length,
      name: "unrelated",
    });
  }
  assertEquals((await t.collect()).matches, true);
  assertEquals(
    t.calls.filter((op) => op.kind === "jobsPage" && op.runId === 10).length,
    20,
  );
});
Deno.test("collector pagination changed counts reject", async () => {
  const t = await fixture();
  while (t.sourceJobs.length < 101) {
    t.sourceJobs.push({
      ...t.sourceJobs[0],
      id: 1000 + t.sourceJobs.length,
      name: "unrelated",
    });
  }
  t.setHook((op, v) =>
    op.kind === "jobsPage" && op.runId === 10 && op.page === 2
      ? mutateMeta(v, (x) => {
        x.total_count = 102;
      })
      : v
  );
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector byte corruption fails real archive digest", async () => {
  const t = await fixture();
  t.setHook((op, v) => {
    if (op.kind === "selectedZip" && v.ok && v.kind === "zip") v.bytes[0] ^= 1;
    return v;
  });
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector freshness expires during collection", async () => {
  const t = await fixture();
  let count = 0;
  assertEquals(
    (await t.collect(
      OPTIONS,
      () => ++count === 1 ? NOW : "2026-11-13T04:30:00Z",
    )).matches,
    false,
  );
});
Deno.test("collector raw transport exceptions stay sanitized", async () => {
  const t = await fixture();
  t.setHook(() => {
    throw new Error("SECRET signed URL");
  });
  const r = await t.collect();
  safe(r);
  assertEquals(r.matches, false);
});

for (const head of ["sourceHead", "qaHead"] as const) {
  Deno.test("collector rejects newline head " + head, async () => {
    const t = await fixture();
    assertEquals(
      (await t.collect({ ...OPTIONS, [head]: OPTIONS[head] + "\n" })).matches,
      false,
    );
    assertEquals(t.calls.length, 0);
  });
}
Deno.test("collector complete synthetic Windows shadow remains observation only", async () => {
  const t = await fixture((f) => {
    f.observed.qaJobs[0].conclusion = "failure";
  });
  const r = await t.collect({
    ...OPTIONS,
    windowsPolicy: "shadow-observation",
  });
  safe(r);
  assertEquals(r.matches, true);
});
Deno.test("collector Windows failure is not excused under require-success", async () => {
  const t = await fixture((f) => {
    f.observed.qaJobs[0].conclusion = "failure";
  });
  assertEquals((await t.collect()).matches, false);
});
for (
  const kind of [
    "proxy",
    "uint16",
    "clamped",
    "dataview",
    "shared",
    "oversize",
    "aggregate",
  ] as const
) {
  Deno.test("collector native bytes reject " + kind, async () => {
    const t = await fixture();
    let traps = 0;
    t.setHook((op, v) => {
      if (op.kind !== "selectedZip" || !v.ok || v.kind !== "zip") return v;
      let bytes: unknown;
      if (kind === "proxy") {
        bytes = new Proxy(v.bytes, {
          get() {
            traps++;
            throw new Error("SECRET");
          },
        });
      }
      if (kind === "uint16") bytes = new Uint16Array(100);
      if (kind === "clamped") bytes = new Uint8ClampedArray(100);
      if (kind === "dataview") bytes = new DataView(new ArrayBuffer(100));
      if (kind === "shared") bytes = new Uint8Array(new SharedArrayBuffer(100));
      if (kind === "oversize") bytes = new Uint8Array(8 * 1024 * 1024 + 1);
      if (kind === "aggregate") bytes = new Uint8Array(6 * 1024 * 1024);
      return { ...v, bytes: bytes as Uint8Array<ArrayBuffer> };
    });
    const r = await t.collect();
    assertEquals(r.matches, false);
    assertEquals(traps, 0);
    if (kind === "aggregate") {
      assertEquals(t.calls.filter((op) => op.kind === "selectedZip").length, 3);
    }
  });
}
Deno.test("collector invalid source clock prevents all transport IO", async () => {
  const t = await fixture();
  assertEquals((await t.collect(OPTIONS, () => "SECRET")).matches, false);
  assertEquals(t.calls.length, 0);
});
Deno.test("collector final protected reviewer change rejects despite same eligibility result", async () => {
  const t = await fixture();
  t.setHook((op, v, n) =>
    op.kind === "pendingDeployments" && n === 2
      ? meta([{
        environment: { id: 1, name: "Deploy-to-installer-release" },
        reviewers: [{ type: "Team", reviewer: { id: 3 } }],
      }])
      : v
  );
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector duplicate jobs across page boundaries reject", async () => {
  const t = await fixture();
  while (t.sourceJobs.length < 101) {
    t.sourceJobs.push({
      ...t.sourceJobs[0],
      id: 1000 + t.sourceJobs.length,
      name: "unrelated",
    });
  }
  t.sourceJobs[100].id = t.sourceJobs[0].id;
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector retains all 64 package artifacts", async () => {
  const t = await fixture();
  while (t.f.observed.packageArtifacts.length < 64) {
    t.f.observed.packageArtifacts.push({
      ...t.f.observed.packageArtifacts[0],
      id: 1000 + t.f.observed.packageArtifacts.length,
      name: "extra-" + t.f.observed.packageArtifacts.length,
    });
  }
  assertEquals((await t.collect()).matches, true);
});
Deno.test("collector pinned contents base64 shape failure sanitized", async () => {
  const t = await fixture();
  t.setHook((op, v) =>
    op.kind === "pinnedContents"
      ? mutateMeta(v, (x) => {
        x.content = "SECRET?";
      })
      : v
  );
  const r = await t.collect();
  safe(r);
  assertEquals(r.matches, false);
});
Deno.test("collector projection removes huge irrelevant raw body from observations", async () => {
  const t = await fixture();
  t.setHook((op, v) =>
    op.kind === "currentRun" || op.kind === "attemptRun"
      ? mutateMeta(v, (x) => {
        x.body = "SECRET".repeat(10000);
      })
      : v
  );
  const r = await t.collect();
  safe(r);
  assertEquals(r.matches, true);
});
Deno.test("collector projection bound rejects oversized needed names", async () => {
  const t = await fixture();
  t.setHook((op, v) =>
    op.kind === "jobsPage"
      ? mutateMeta(v, (x) => {
        (x.jobs as Record<string, unknown>[])[0].name = "X".repeat(1024 * 1024);
      })
      : v
  );
  assertEquals((await t.collect()).matches, false);
});
Deno.test("collector required artifact expiry crossing final hash rejects", async () => {
  const t = await fixture();
  for (
    const a of [...t.f.observed.packageArtifacts, ...t.f.observed.qaArtifacts]
  ) a.expires_at = "2026-11-13T04:15:01Z";
  let clocks = 0;
  const r = await t.collect(
    OPTIONS,
    () => ++clocks < 3 ? NOW : "2026-11-13T04:15:02Z",
  );
  assertEquals(r.matches, false);
  safe(r);
  assertEquals(clocks, 3);
});
for (const kind of ["pendingDeployments", "releaseTag"] as const) {
  for (
    const mode of ["unauthorized", "forbidden", "malformed", "null"] as const
  ) {
    Deno.test(
      "collector eligibility API failures are not readiness " + kind + " " +
        mode,
      async () => {
        const t = await fixture();
        t.setHook((op, v) =>
          op.kind !== kind ? v : mode === "malformed"
            ? {
              ok: true,
              kind: "metadata",
              jsonText: "SECRET{",
              publicationAuthorized: false,
            }
            : mode === "null"
            ? meta(null)
            : { ok: false, code: "http", statusClass: mode }
        );
        const r = await t.collect();
        assert(!r.matches);
        assertEquals(r.stage, "eligibility");
        assertEquals(r.code, "collectionFailed");
        safe(r);
        assertEquals(
          t.calls.filter((op) => op.kind === "selectedZip").length,
          0,
        );
      },
    );
  }
}
for (const mode of ["completed", "missingReviewers"] as const) {
  Deno.test(
    "collector observed source not ready classification " + mode,
    async () => {
      const t = await fixture();
      if (mode === "completed") {
        t.f.observed.packageRun.status = "completed";
        t.f.observed.packageRun.conclusion = "success";
      } else t.pending[0].reviewers = [];
      const r = await t.collect();
      assert(!r.matches);
      assertEquals(r.stage, "eligibility");
      assertEquals(r.code, "sourceNotReady");
    },
  );
}
for (const kind of ["releaseTag", "pendingDeployments"] as const) {
  for (
    const mode of [
      "null",
      "malformed",
      "badShape",
      "unauthorized",
      "forbidden",
      "throw",
    ] as const
  ) {
    Deno.test(
      "collector final read failures remain collectionFailed " + kind + " " +
        mode,
      async () => {
        const t = await fixture();
        t.setHook((op, v, n) => {
          if (op.kind !== kind || n < 2) return v;
          if (mode === "throw") throw new Error("SECRET");
          if (mode === "malformed") {
            return {
              ok: true,
              kind: "metadata",
              jsonText: "SECRET{",
              publicationAuthorized: false,
            };
          }
          if (mode === "null") return meta(null);
          if (mode === "badShape") {
            return meta(
              kind === "releaseTag" ? {} : [{ environment: {}, reviewers: [] }],
            );
          }
          return { ok: false, code: "http", statusClass: mode };
        });
        const r = await t.collect();
        assert(!r.matches);
        assertEquals(r.stage, "recheck");
        assertEquals(r.code, "collectionFailed");
        safe(r);
      },
    );
  }
}
