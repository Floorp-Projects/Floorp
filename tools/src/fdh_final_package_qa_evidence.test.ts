// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { zipSync } from "fflate";
import {
  QA_EVIDENCE_LIMITS,
  verifyFinalPackageQaEvidence,
} from "./fdh_final_package_qa_evidence.ts";
import type { FinalPackageQaEvidenceRequest } from "./fdh_final_package_qa_evidence_types.ts";
import type {
  FinalPackageQaExpectation,
  FinalQaArtifactIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";

// Synthetic only: no FS/network/browser permissions or downloaded archives.
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
const json = (v: unknown) => JSON.stringify(v, null, 2);
const encoder = new TextEncoder();
async function digest(bytes: Uint8Array) {
  const hash = new Uint8Array(
    await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
  );
  return "sha256:" +
    Array.from(hash, (v) => v.toString(16).padStart(2, "0")).join("");
}
async function fixture(mutate?: (f: ReturnType<typeof baseFixture>) => void) {
  const f = baseFixture();
  mutate?.(f);
  const archives: { artifactId: number; bytes: Uint8Array }[] = [];
  for (let i = 0; i < 4; i++) {
    const payload = f.observed.evidence[i];
    const bytes = zipSync(
      Object.fromEntries(
        payload.entries.map((v) => [v.name, encoder.encode(json(v.value))]),
      ),
      { level: 0 },
    );
    const actualDigest = await digest(bytes);
    const target = Object.values(f.expected.targets)[i];
    target.evidence.digest = actualDigest;
    f.observed.qaArtifacts[i].digest = actualDigest;
    archives.push({ artifactId: payload.artifactId, bytes });
  }
  const { evidence: _evidence, ...observation } = f.observed;
  return {
    f,
    request: {
      expectedJson: json(f.expected),
      observationJson: json(observation),
      archives,
    },
  };
}
async function rejected(request: unknown, stage?: string) {
  const result = await verifyFinalPackageQaEvidence(
    request as FinalPackageQaEvidenceRequest,
  );
  assert(!result.offlineEvidenceMatched);
  if (stage) assertEquals(result.stage, stage);
  assertEquals(
    Object.keys(result).sort(),
    [
      "scope",
      "transportAuthenticityVerified",
      "apiAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
      "publicationAuthorized",
      "offlineEvidenceMatched",
      "stage",
    ].sort(),
  );
  assert(!json(result).includes("SECRET_PAYLOAD"));
}
Deno.test("actual four ZIP outputs compose only offline layers without raw payloads", async () => {
  const { request } = await fixture();
  const result = await verifyFinalPackageQaEvidence(request);
  assert(result.offlineEvidenceMatched);
  assertEquals(result.offlineArchiveIntegrity.artifacts.length, 4);
  assertEquals(result.attestationConsistency.archiveBytesVerified, false);
  assertEquals(result.attestationConsistency.publicationAuthorized, false);
  for (
    const key of [
      "publicationAuthorized",
      "apiAuthenticityVerified",
      "transportAuthenticityVerified",
      "nativeQaVerified",
      "liveApprovalVerified",
    ] as const
  ) assertEquals(result[key], false);
  assert(!json(result).includes("sourceEligibility"));
  assert(!json(result).includes("input.json"));
});
Deno.test("optional metadata LF accepted and caller archive order irrelevant", async () => {
  const { request } = await fixture();
  request.expectedJson += "\n";
  request.observationJson += "\n";
  request.archives.reverse();
  assert((await verifyFinalPackageQaEvidence(request)).offlineEvidenceMatched);
});
for (const field of ["expectedJson", "observationJson"] as const) {
  for (
    const [name, text] of [
      ["malformed", "{"],
      ["duplicate", '{"a":1,"a":2}'],
      ["escaped duplicate", '{"a":1,"\\u0061":2}'],
      ["deep", '{"a":' + "[".repeat(65) + "0" + "]".repeat(65) + "}"],
      ["oversize characters", "x".repeat(QA_EVIDENCE_LIMITS.metadataBytes + 1)],
      ["oversize UTF8", json({ s: "界".repeat(400_000) })],
      ["array root", "[]"],
      ["null root", "null"],
    ]
  ) {
    Deno.test(field + " rejects " + name, async () => {
      const { request } = await fixture();
      request[field] = text;
      await rejected(request, "snapshot");
    });
  }
}
for (const key of ["evidence", "archiveVerified", "archiveBytesVerified"]) {
  Deno.test("caller metadata claim rejected: " + key, async () => {
    const { request } = await fixture();
    request.observationJson = json({
      ...JSON.parse(request.observationJson),
      [key]: [{ SECRET_PAYLOAD: true }],
    });
    await rejected(request, "snapshot");
  });
}
for (
  const mutation of [
    "missing",
    "duplicate",
    "extra",
    "swap",
    "wrong id",
  ] as const
) {
  Deno.test("archives reject " + mutation, async () => {
    const { request } = await fixture();
    if (mutation === "missing") request.archives.pop();
    if (mutation === "extra") request.archives.push(request.archives[0]);
    if (mutation === "duplicate") request.archives[3] = request.archives[0];
    if (mutation === "swap") {
      [request.archives[0].bytes, request.archives[1].bytes] = [
        request.archives[1].bytes,
        request.archives[0].bytes,
      ];
    }
    if (mutation === "wrong id") request.archives[0].artifactId++;
    await rejected(request);
  });
}
for (const key of ["name", "digest", "id"] as const) {
  Deno.test("API artifact mapping rejects wrong " + key, async () => {
    const { request } = await fixture();
    const observed = JSON.parse(request.observationJson);
    observed.qaArtifacts[0][key] = key === "id" ? 999 : "SECRET_PAYLOAD";
    request.observationJson = json(observed);
    await rejected(request, "mapping");
  });
}
Deno.test("trusted digest mismatch cannot be repaired from evidence", async () => {
  const { request } = await fixture();
  const policy = JSON.parse(request.expectedJson),
    observed = JSON.parse(request.observationJson);
  const bad = "sha256:" + "0".repeat(64);
  policy.targets["windows/x86_64"].evidence.digest = bad;
  observed.qaArtifacts[0].digest = bad;
  request.expectedJson = json(policy);
  request.observationJson = json(observed);
  await rejected(request, "archive");
});
for (const position of [0, 3]) {
  Deno.test(
    "poisoned actual pre payload " + position + " leaks no partial result",
    async () => {
      const { request } = await fixture((f) => {
        f.observed.evidence[position].entries[0].value.unsigned = true;
      });
      await rejected(request, "consistency");
    },
  );
}
Deno.test("poisoned actual post payload rejected", async () => {
  const { request } = await fixture((f) => {
    f.observed.evidence[0].entries[1].value.sourceEligibility
      .publicationApprovalPerformed = true;
  });
  await rejected(request, "consistency");
});
Deno.test("all metadata and all byte copies precede first await", async () => {
  const { request } = await fixture();
  const pending = verifyFinalPackageQaEvidence(request);
  request.expectedJson = "null";
  request.observationJson = "null";
  for (const a of request.archives) {
    a.bytes.fill(0);
    a.artifactId = 999;
  }
  request.archives.length = 0;
  assert((await pending).offlineEvidenceMatched);
});
for (const spot of ["request", "array", "descriptor"] as const) {
  Deno.test("rejects accessor without invocation: " + spot, async () => {
    const { request } = await fixture();
    let reads = 0;
    const getter = {
      get() {
        reads++;
        throw new Error("SECRET_PAYLOAD");
      },
      configurable: true,
    };
    if (spot === "request") {
      Object.defineProperty(request, "expectedJson", getter);
    }
    if (spot === "array") {
      Object.defineProperty(request.archives, "0", getter);
    }
    if (spot === "descriptor") {
      Object.defineProperty(request.archives[0], "artifactId", getter);
    }
    await rejected(request, "snapshot");
    assertEquals(reads, 0);
  });
}
for (const own of ["length", "byteLength", "byteOffset", "buffer"]) {
  Deno.test("reject own byte size getter without reads: " + own, async () => {
    const { request } = await fixture();
    let reads = 0;
    Object.defineProperty(request.archives[0].bytes, own, {
      get() {
        reads++;
        return 1;
      },
    });
    await rejected(request, "snapshot");
    assertEquals(reads, 0);
  });
}
Deno.test("forwarding byte Proxy rejects before all traps", async () => {
  const { request } = await fixture();
  let reads = 0;
  request.archives[0].bytes = new Proxy(request.archives[0].bytes, {
    get(t, k) {
      reads++;
      return Reflect.get(t, k, t);
    },
    getPrototypeOf(t) {
      reads++;
      return Reflect.getPrototypeOf(t);
    },
    getOwnPropertyDescriptor(t, k) {
      reads++;
      return Reflect.getOwnPropertyDescriptor(t, k);
    },
  });
  await rejected(request, "snapshot");
  assertEquals(reads, 0);
});
for (
  const kind of [
    "u16",
    "clamped",
    "view",
    "shared",
    "resizable",
    "subclass",
  ] as const
) {
  Deno.test("rejects native backing/kind " + kind, async () => {
    const { request } = await fixture();
    let value: unknown;
    if (kind === "u16") {
      value = Object.setPrototypeOf(new Uint16Array(32), Uint8Array.prototype);
    }
    if (kind === "clamped") {
      value = Object.setPrototypeOf(
        new Uint8ClampedArray(32),
        Uint8Array.prototype,
      );
    }
    if (kind === "view") {
      value = Object.setPrototypeOf(
        new DataView(new ArrayBuffer(32)),
        Uint8Array.prototype,
      );
    }
    if (kind === "shared") {
      value = new Uint8Array(new SharedArrayBuffer(32));
    }
    if (kind === "resizable") {
      value = new Uint8Array(new ArrayBuffer(32, { maxByteLength: 64 }));
    }
    if (kind === "subclass") {
      class Bytes extends Uint8Array {}
      value = new Bytes(32);
    }
    request.archives[0].bytes = value as Uint8Array;
    await rejected(request, "snapshot");
  });
}
Deno.test("aggregate actual native lengths reject before iterator/copy/hash", async () => {
  const { request } = await fixture();
  let reads = 0;
  for (const a of request.archives) {
    a.bytes = new Uint8Array(5 * 1024 * 1024);
    Object.defineProperty(a.bytes, Symbol.iterator, {
      get() {
        reads++;
        throw new Error("SECRET_PAYLOAD");
      },
    });
  }
  await rejected(request, "snapshot");
  assertEquals(reads, 0);
});
Deno.test("genuine native bytes ignore poisoned tag and iterator", async () => {
  const { request } = await fixture();
  let reads = 0;
  for (const a of request.archives) {
    for (const k of [Symbol.iterator, Symbol.toStringTag]) {
      Object.defineProperty(a.bytes, k, {
        get() {
          reads++;
          throw new Error("SECRET_PAYLOAD");
        },
      });
    }
  }
  assert((await verifyFinalPackageQaEvidence(request)).offlineEvidenceMatched);
  assertEquals(reads, 0);
});
Deno.test("shadow failure retains explicit nonauthorizing classification", async () => {
  const { request } = await fixture((f) => {
    f.expected.windowsPolicy = "shadow-observation";
    f.observed.qaJobs[0].conclusion = "failure";
  });
  const r = await verifyFinalPackageQaEvidence(request);
  assert(r.offlineEvidenceMatched);
  assertEquals(
    r.attestationConsistency.windows.scope,
    "shadow-observation-only",
  );
  assertEquals(r.attestationConsistency.windows.conclusion, "failure");
});
Deno.test("clock immutable and stale cannot be overridden by current host clock", async () => {
  const { request } = await fixture();
  const p = JSON.parse(request.expectedJson);
  p.now = "2027-11-13T04:15:00Z";
  request.expectedJson = json(p);
  await rejected(request, "consistency");
});

for (const list of ["qaJobs", "qaArtifacts"] as const) {
  for (const mode of ["missing", "duplicate", "extra"] as const) {
    Deno.test(
      "fixed metadata count/identity " + list + " " + mode,
      async () => {
        const { request } = await fixture();
        const o = JSON.parse(request.observationJson);
        if (mode === "missing") o[list].pop();
        if (mode === "duplicate") {
          o[list][3] = o[list][0];
        }
        if (mode === "extra") o[list].push(o[list][0]);
        request.observationJson = json(o);
        await rejected(request);
      },
    );
  }
}
Deno.test("package artifacts allow unselected complete-list items but cap at64", async () => {
  const { request } = await fixture();
  const o = JSON.parse(request.observationJson);
  o.packageArtifacts.push({
    ...o.packageArtifacts[0],
    id: 999,
    name: "unselected-linux-aarch64",
  });
  request.observationJson = json(o);
  assert((await verifyFinalPackageQaEvidence(request)).offlineEvidenceMatched);
  while (o.packageArtifacts.length < 65) {
    o.packageArtifacts.push({ id: 1000 + o.packageArtifacts.length });
  }
  request.observationJson = json(o);
  await rejected(request, "snapshot");
});
for (const which of ["input.json", "source-after-qa.json"]) {
  Deno.test("missing actual required file " + which, async () => {
    const { request } = await fixture((f) => {
      f.observed.evidence[0].entries = f.observed.evidence[0].entries.filter(
        (e) => e.name !== which,
      );
    });
    await rejected(request, "archive");
  });
}
Deno.test("shadow cannot excuse incomplete actual post", async () => {
  const { request } = await fixture((f) => {
    f.expected.windowsPolicy = "shadow-observation";
    f.observed.qaJobs[0].conclusion = "failure";
    f.observed.evidence[0].entries.pop();
  });
  await rejected(request, "archive");
});
Deno.test("per archive budget rejects oversized actual bytes", async () => {
  const { request } = await fixture();
  request.archives[0].bytes = new Uint8Array(8 * 1024 * 1024 + 1);
  await rejected(request, "snapshot");
});
Deno.test("last archive failure cannot return previous successful decoded payloads", async () => {
  const { request } = await fixture();
  request.archives[3].bytes[0] ^= 255;
  await rejected(request, "archive");
});
Deno.test("descriptor Proxy can neither substitute metadata after snapshot nor force iterator", async () => {
  const { request } = await fixture();
  const original = request.archives[3];
  let reads = 0;
  request.archives[3] = new Proxy(original, {
    get() {
      throw new Error("SECRET_PAYLOAD");
    },
    getOwnPropertyDescriptor(t, k) {
      reads++;
      return Reflect.getOwnPropertyDescriptor(t, k);
    },
  });
  const p = verifyFinalPackageQaEvidence(request);
  original.artifactId = 999;
  original.bytes.fill(0);
  assert((await p).offlineEvidenceMatched);
  assertEquals(reads, 2);
});
Deno.test("immutable pre-await policy clock survives replacement", async () => {
  const { request } = await fixture();
  const p = verifyFinalPackageQaEvidence(request);
  const policy = JSON.parse(request.expectedJson);
  policy.now = "2027-11-13T04:15:00Z";
  request.expectedJson = json(policy);
  assert((await p).offlineEvidenceMatched);
});
Deno.test("fake byte length Proxy rejects with no property reads even last in input", async () => {
  const { request } = await fixture();
  let reads = 0;
  request.archives[3].bytes = new Proxy(new Uint8Array(8 * 1024 * 1024 + 1), {
    get() {
      reads++;
      return 22;
    },
    getPrototypeOf() {
      reads++;
      return Uint8Array.prototype;
    },
  });
  await rejected(request, "snapshot");
  assertEquals(reads, 0);
});
