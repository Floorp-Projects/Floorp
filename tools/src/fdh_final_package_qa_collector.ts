// SPDX-License-Identifier: MPL-2.0
import { validateFinalPackageSource } from "../final-package-qa-source.mjs";
import { createQaGhTransport } from "./fdh_qa_gh_transport.ts";
import { verifyFinalPackageQaEvidence } from "./fdh_final_package_qa_evidence.ts";
import type { QaGhOperation } from "./fdh_qa_gh_transport_types.ts";
import type {
  FinalPackageQaExpectation,
  FinalQaArtifactIdentity,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";
import type {
  QaCollectionReport,
  QaCollectionStage,
  QaCollectorDependencies,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";
export type {
  QaCollectionReport,
  QaCollectorDependencies,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";

const REPO = 437505831;
const WORKFLOWS = {
  source: {
    id: 163541311,
    path: ".github/workflows/package_and_publish_release.yml",
  },
  qa: {
    id: 377351757,
    path: ".github/workflows/validate-final-package-qa.yml",
  },
} as const;
const TARGETS: readonly FinalQaTarget[] = [
  "windows/x86_64",
  "linux/x86_64",
  "mac/x86_64",
  "mac/aarch64",
];
const SCOPE = {
  scope: "read-only-point-in-time-qa-collection",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
} as const;
const MIB = 1024 * 1024;
const TA = Object.getPrototypeOf(Uint8Array.prototype);
const KIND = Object.getOwnPropertyDescriptor(TA, Symbol.toStringTag)!.get!;
const LENGTH = Object.getOwnPropertyDescriptor(TA, "length")!.get!;
const BUFFER = Object.getOwnPropertyDescriptor(TA, "buffer")!.get!;
const RESIZABLE = Object.getOwnPropertyDescriptor(
  ArrayBuffer.prototype,
  "resizable",
)!.get!;
const SET = Uint8Array.prototype.set;
// Initial readiness ceiling only, NOT a release freshness policy or authorization.
const MAX_AGE_MS = 24 * 60 * 60 * 1000;
function need(v: unknown): asserts v {
  if (!v) throw new Error("invalid collection");
}
function obj(v: unknown): Record<string, unknown> {
  need(v !== null && typeof v === "object" && !Array.isArray(v));
  return v as Record<string, unknown>;
}
function arr(v: unknown): unknown[] {
  need(Array.isArray(v));
  return v;
}
function positive(v: unknown): number {
  need(typeof v === "number" && Number.isSafeInteger(v) && v > 0);
  return v;
}
function str(v: unknown, max = 240): string {
  need(typeof v === "string" && v.length <= max);
  return v;
}
function pick(v: unknown, keys: readonly string[]): Record<string, unknown> {
  const o = obj(v);
  return Object.fromEntries(keys.map((k) => [k, o[k]]));
}
function json(v: unknown): string {
  const text = JSON.stringify(v, null, 2);
  need(text.length <= MIB && new TextEncoder().encode(text).length <= MIB);
  return text;
}
function equal(a: unknown, b: unknown): void {
  need(json(a) === json(b));
}
function timestamp(v: unknown): number {
  const s = str(v, 30);
  need(/^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d(?:\.\d{1,3})?Z$/.test(s));
  const t = Date.parse(s);
  need(
    Number.isFinite(t) &&
      new Date(t).toISOString().slice(0, 19) === s.slice(0, 19),
  );
  return t;
}
function run(v: unknown) {
  const r = pick(v, [
    "id",
    "workflow_id",
    "run_attempt",
    "head_sha",
    "path",
    "event",
    "head_branch",
    "status",
    "conclusion",
    "run_started_at",
    "updated_at",
  ]);
  r.repository = pick(obj(v).repository, ["id", "full_name"]);
  return r;
}
function artifact(v: unknown) {
  const a = pick(v, [
    "id",
    "name",
    "digest",
    "expired",
    "created_at",
    "expires_at",
    "size_in_bytes",
  ]);
  a.workflow_run = pick(obj(v).workflow_run, [
    "id",
    "head_sha",
    "head_branch",
    "repository_id",
    "head_repository_id",
  ]);
  return a;
}
function job(v: unknown) {
  return pick(v, [
    "id",
    "name",
    "run_id",
    "run_attempt",
    "head_sha",
    "head_branch",
    "status",
    "conclusion",
    "started_at",
    "completed_at",
  ]);
}
function identity(a: Record<string, unknown>): FinalQaArtifactIdentity {
  const id = positive(a.id), name = str(a.name), digest = str(a.digest);
  need(
    /^[A-Za-z0-9_.-]{1,240}$/.test(name) &&
      /^sha256:[0-9a-f]{64}$/.test(digest),
  );
  return { id, name, digest };
}
function select(items: Record<string, unknown>[], name: string) {
  const matches = items.filter((a) => a.name === name);
  need(matches.length === 1);
  return identity(matches[0]);
}
function checkRun(
  r: Record<string, unknown>,
  o: QaCollectorOptions,
  qa: boolean,
) {
  const w = WORKFLOWS[qa ? "qa" : "source"];
  need(
    r.id === (qa ? o.qaRunId : o.sourceRunId) &&
      r.run_attempt === (qa ? o.qaAttempt : o.sourceAttempt) &&
      r.head_sha === (qa ? o.qaHead : o.sourceHead) && r.workflow_id === w.id &&
      r.path === w.path && r.event === "workflow_dispatch" &&
      r.head_branch === "main",
  );
  need(
    obj(r.repository).id === REPO &&
      obj(r.repository).full_name === "Floorp-Projects/Floorp",
  );
}
function contents(value: unknown, file: "package" | "runtimeLock") {
  const c = obj(value);
  const path = file === "package" ? "package.json" : "floorp-runtime.lock.json";
  need(
    c.type === "file" && c.path === path && c.name === path &&
      c.encoding === "base64",
  );
  const content = str(c.content, 2 * MIB);
  need(
    typeof c.size === "number" && Number.isSafeInteger(c.size) && c.size >= 0 &&
      c.size <= MIB,
  );
  need(/^[A-Za-z0-9+/=\r\n]*$/.test(content));
  const raw = atob(content.replace(/[\r\n]/g, ""));
  need(raw.length === c.size);
  const bytes = Uint8Array.from(raw, (c) => c.charCodeAt(0));
  return obj(
    JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(bytes)),
  );
}

/** GET-only, bounded point-in-time observation. Host gh/config/TLS remain TCB.
 * No filesystem, browser, credential reads, approvals, publication or ledger writes.
 * Every failure is fixed/sanitized; no response, decoded payload or exception escapes. */
export async function collectFinalPackageQa(
  options: QaCollectorOptions,
  deps: QaCollectorDependencies = {},
): Promise<QaCollectionReport> {
  let stage: QaCollectionStage = "options";
  let code: Extract<QaCollectionReport, { matches: false }>["code"] =
    "invalidOptions";
  try {
    // Snapshot every primitive policy field before any await. Unknown public fields fail.
    const keys = [
      "sourceRunId",
      "sourceAttempt",
      "sourceHead",
      "qaRunId",
      "qaAttempt",
      "qaHead",
      "windowsPolicy",
      "maxAgeMs",
    ] as const;
    need(Reflect.ownKeys(options).length === keys.length);
    const snapshot: Record<string, unknown> = {};
    for (const key of keys) {
      const d = Object.getOwnPropertyDescriptor(options, key);
      need(d && "value" in d);
      snapshot[key] = d.value;
    }
    const o = snapshot as unknown as QaCollectorOptions;
    for (
      const n of [
        o.sourceRunId,
        o.sourceAttempt,
        o.qaRunId,
        o.qaAttempt,
        o.maxAgeMs,
      ]
    ) positive(n);
    need(o.sourceRunId !== o.qaRunId && o.maxAgeMs <= MAX_AGE_MS);
    need(
      typeof o.sourceHead === "string" && o.sourceHead.length === 40 &&
        /^[a-f0-9]{40}$/.test(o.sourceHead) &&
        typeof o.qaHead === "string" && o.qaHead.length === 40 &&
        /^[a-f0-9]{40}$/.test(o.qaHead),
    );
    need(
      o.windowsPolicy === "require-success" ||
        o.windowsPolicy === "shadow-observation",
    );
    const now = deps.now ?? (() => new Date().toISOString());
    const start = timestamp(now());
    const transport = deps.transport ?? createQaGhTransport();
    code = "collectionFailed";
    const get = async (
      op: QaGhOperation,
      missingRelease = false,
    ): Promise<unknown> => {
      const previousCode = code;
      code = "collectionFailed";
      const result = await transport.request(op);
      if (!result.ok) {
        if (
          missingRelease && op.kind === "releaseTag" &&
          result.code === "http" && result.statusClass === "notFound"
        ) {
          code = previousCode;
          return null;
        }
        throw new Error("collection failed");
      }
      need(
        result.kind === "metadata" && typeof result.jsonText === "string" &&
          result.jsonText.length <= 2 * MIB,
      );
      const value: unknown = JSON.parse(result.jsonText);
      // HTTP200 null is malformed, NEVER absence. Only the release404 branch above
      // yields the existing draft-visibility null sentinel.
      if (op.kind === "releaseTag") obj(value);
      code = previousCode;
      return value;
    };
    const pinnedRun = async (qa: boolean) => {
      const runId = qa ? o.qaRunId : o.sourceRunId,
        attempt = qa ? o.qaAttempt : o.sourceAttempt;
      const current = run(await get({ kind: "currentRun", runId }));
      checkRun(current, o, qa);
      const exact = run(await get({ kind: "attemptRun", runId, attempt }));
      checkRun(exact, o, qa);
      equal(current, exact);
      return current;
    };
    const inventory = async (qa: boolean, jobs: boolean) => {
      const runId = qa ? o.qaRunId : o.sourceRunId,
        attempt = qa ? o.qaAttempt : o.sourceAttempt;
      const limit = jobs ? (qa ? 4 : 1000) : (qa ? 4 : 64),
        key = jobs ? "jobs" : "artifacts";
      let total: number | undefined;
      const items: Record<string, unknown>[] = [], ids = new Set<number>();
      for (let page = 1; page <= 10; page++) {
        const response = obj(
          await get(
            jobs
              ? { kind: "jobsPage", runId, attempt, page }
              : { kind: "artifactsPage", runId, page },
          ),
        );
        const count = response.total_count;
        need(
          typeof count === "number" && Number.isSafeInteger(count) &&
            count >= 0 && count <= limit,
        );
        if (total === undefined) total = count;
        else need(total === count);
        const batch = arr(response[key]);
        need(batch.length === Math.min(100, total - items.length));
        for (const raw of batch) {
          const item = jobs ? job(raw) : artifact(raw);
          const id = positive(item.id);
          need(!ids.has(id));
          ids.add(id);
          if (jobs) {
            need(
              item.run_id === runId && item.run_attempt === attempt &&
                item.head_sha === (qa ? o.qaHead : o.sourceHead) &&
                item.head_branch === "main",
            );
          }
          items.push(item);
        }
        if (items.length === total) {
          json(items);
          return items;
        }
      }
      throw new Error("incomplete inventory");
    };
    stage = "identity";
    const repo = obj(await get({ kind: "repository" }));
    need(repo.id === REPO && repo.full_name === "Floorp-Projects/Floorp");
    for (const workflow of ["source", "qa"] as const) {
      const w = obj(await get({ kind: "workflow", workflow }));
      need(
        w.id === WORKFLOWS[workflow].id &&
          w.path === WORKFLOWS[workflow].path && w.state === "active",
      );
    }
    const packageRun = await pinnedRun(false), qaRun = await pinnedRun(true);
    stage = "inventory";
    const sourceJobs = await inventory(false, true),
      qaJobs = await inventory(true, true),
      packageArtifacts = await inventory(false, false),
      qaArtifacts = await inventory(true, false);
    need(
      qaJobs.length === 4 && qaArtifacts.length === 4 &&
        packageArtifacts.length > 0,
    );
    stage = "versions";
    const pkg = contents(
      await get({
        kind: "pinnedContents",
        file: "package",
        head: o.sourceHead,
      }),
      "package",
    );
    const lock = contents(
      await get({
        kind: "pinnedContents",
        file: "runtimeLock",
        head: o.sourceHead,
      }),
      "runtimeLock",
    );
    const version = str(pkg.version, 128);
    need(
      /^[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$/.test(
        version,
      ),
    );
    need(lock.schemaVersion === 1);
    const runtimes = arr(lock.artifacts).map(obj);
    const targets = {} as FinalPackageQaExpectation["targets"];
    for (const target of TARGETS) {
      const [platform, arch] = target.split("/");
      const match = runtimes.filter((r) =>
        r.platform === (platform === "mac" ? "macos" : platform) &&
        r.architecture === (platform === "mac" ? "universal" : arch)
      );
      need(match.length === 1);
      const firefoxVersion = str(match[0].version, 128),
        runtimeBuildId = str(match[0].buildId, 14);
      need(
        /^[0-9]+(?:\.[0-9]+)*(?:[a-zA-Z0-9.-]*)$/.test(firefoxVersion) &&
          /^\d{14}$/.test(runtimeBuildId),
      );
      targets[target] = {
        package: select(
          packageArtifacts,
          platform === "mac"
            ? "noraneko-mac-universal-installer"
            : "noraneko-" + platform + "-" + arch + "-installer",
        ),
        record: select(
          packageArtifacts,
          "floorp-native-verification-" + platform + "-" + arch + "-v2",
        ),
        evidence: select(
          qaArtifacts,
          "final-package-qa-" + platform + "-" + arch + "-" + o.qaRunId,
        ),
        floorpVersion: version,
        firefoxVersion,
        runtimeBuildId,
      };
    }
    const bundle = select(packageArtifacts, "floorp-release-bundle-v2");
    const eligibility = async (
      r: Record<string, unknown>,
      jobs: Record<string, unknown>[],
      artifacts: Record<string, unknown>[],
    ) => {
      const previousCode = code;
      code = "collectionFailed";
      const pending = arr(
        await get({ kind: "pendingDeployments", sourceRunId: o.sourceRunId }),
      ).map((v) => {
        const p = obj(v);
        return {
          environment: pick(p.environment, ["id", "name"]),
          reviewers: arr(p.reviewers).map((v) => {
            const e = obj(v);
            return { type: e.type, reviewer: pick(e.reviewer, ["id"]) };
          }),
        };
      });
      const rawRelease = await get({ kind: "releaseTag", version }, true);
      const release = rawRelease === null
        ? null
        : pick(rawRelease, ["draft", "prerelease", "target_commitish"]);
      // Unreadable protected-state shapes are collection failures, not a
      // negative observation of the source predicate. Empty reviewers are valid
      // metadata and deliberately reach the unchanged source validator.
      for (const p of pending) {
        positive(p.environment.id);
        str(p.environment.name);
        for (const e of p.reviewers) {
          str(e.type);
          positive(e.reviewer.id);
        }
      }
      if (release !== null) {
        need(
          typeof release.draft === "boolean" &&
            typeof release.prerelease === "boolean",
        );
        str(release.target_commitish);
      }
      const input = {
        run: r,
        headSha: o.sourceHead,
        jobs,
        prePublication: true,
        unsigned: false,
        pendingDeployments: pending,
        release,
        bundleArtifacts: artifacts.filter((a) =>
          a.name === "floorp-release-bundle-v2"
        ),
      };
      json(input);
      // Shapes are projected from parsed JSON, never synthesized pending approvals.
      code = stage === "eligibility" ? "sourceNotReady" : "changed";
      const result = validateFinalPackageSource(
        input as unknown as Parameters<typeof validateFinalPackageSource>[0],
      );
      code = previousCode;
      return { result, pending, release };
    };
    stage = "eligibility";
    code = "collectionFailed";
    const eligible = await eligibility(
      packageRun,
      sourceJobs,
      packageArtifacts,
    );
    equal(eligible.result.bundle, bundle);
    code = "collectionFailed";
    const expected: FinalPackageQaExpectation = {
      repository: { id: REPO, fullName: "Floorp-Projects/Floorp" },
      packageRun: {
        id: o.sourceRunId,
        attempt: o.sourceAttempt,
        head: o.sourceHead,
        workflowId: WORKFLOWS.source.id,
      },
      qaRun: {
        id: o.qaRunId,
        attempt: o.qaAttempt,
        head: o.qaHead,
        workflowId: WORKFLOWS.qa.id,
      },
      bundle,
      targets,
      now: new Date(start).toISOString(),
      maxAgeMs: o.maxAgeMs,
      windowsPolicy: o.windowsPolicy,
    };
    const observation = {
      packageRun,
      qaRun,
      qaJobs,
      packageArtifacts,
      qaArtifacts,
    };
    json(expected);
    json(observation);
    stage = "archives";
    const archives: { artifactId: number; bytes: Uint8Array }[] = [];
    let compressed = 0;
    for (const target of TARGETS) {
      const selected = targets[target].evidence;
      const listed = qaArtifacts.find((a) => a.id === selected.id);
      need(listed);
      equal(
        artifact(
          await get({ kind: "artifactMetadata", artifactId: selected.id }),
        ),
        listed,
      );
      const result = await transport.request({
        kind: "selectedZip",
        artifactId: selected.id,
      });
      need(result.ok && result.kind === "zip");
      // Native guards precede size reads/copies even for host-injected transports.
      const bytes = result.bytes;
      need(ArrayBuffer.isView(bytes));
      need(Reflect.apply(KIND, bytes, []) === "Uint8Array");
      const backing = Reflect.apply(BUFFER, bytes, []);
      need(
        backing instanceof ArrayBuffer &&
          Object.getPrototypeOf(backing) === ArrayBuffer.prototype,
      );
      need(!Reflect.apply(RESIZABLE, backing, []));
      const length = Reflect.apply(LENGTH, bytes, []) as number;
      need(length >= 22 && length <= 8 * MIB);
      compressed += length;
      need(compressed <= 16 * MIB);
      // Snapshot now: a trusted injected transport must not mutate earlier results.
      const copy = new Uint8Array(length);
      Reflect.apply(SET, copy, [bytes]);
      archives.push({ artifactId: selected.id, bytes: copy });
    }
    stage = "evidence";
    code = "evidenceMismatch";
    const initial = await verifyFinalPackageQaEvidence({
      expectedJson: json(expected),
      observationJson: json(observation),
      archives,
    });
    need(initial.offlineEvidenceMatched);
    stage = "recheck";
    code = "changed";
    const finalSource = await pinnedRun(false), finalQa = await pinnedRun(true);
    equal(finalSource, packageRun);
    equal(finalQa, qaRun);
    const finalSourceJobs = await inventory(false, true),
      finalQaJobs = await inventory(true, true),
      finalPackageArtifacts = await inventory(false, false),
      finalQaArtifacts = await inventory(true, false);
    equal(sourceJobs, finalSourceJobs);
    equal(qaJobs, finalQaJobs);
    equal(packageArtifacts, finalPackageArtifacts);
    equal(qaArtifacts, finalQaArtifacts);
    for (const target of TARGETS) {
      const id = targets[target].evidence.id;
      equal(
        artifact(await get({ kind: "artifactMetadata", artifactId: id })),
        qaArtifacts.find((a) => a.id === id),
      );
    }
    const finalEligible = await eligibility(
      finalSource,
      finalSourceJobs,
      finalPackageArtifacts,
    );
    equal(finalEligible, eligible);
    equal(await pinnedRun(false), finalSource);
    equal(await pinnedRun(true), finalQa);
    const checkedAt = now(), end = timestamp(checkedAt);
    need(end >= start);
    const final = await verifyFinalPackageQaEvidence({
      expectedJson: json({ ...expected, now: checkedAt }),
      observationJson: json({
        packageRun: finalSource,
        qaRun: finalQa,
        qaJobs: finalQaJobs,
        packageArtifacts: finalPackageArtifacts,
        qaArtifacts: finalQaArtifacts,
      }),
      archives,
    });
    need(final.offlineEvidenceMatched);
    // Check internal freshness again after the final hash/extraction await.
    const done = timestamp(now());
    const requiredIds = new Set([
      bundle.id,
      ...TARGETS.flatMap(
        (target) => [
          targets[target].package.id,
          targets[target].record.id,
          targets[target].evidence.id,
        ],
      ),
    ]);
    for (const a of [...finalPackageArtifacts, ...finalQaArtifacts]) {
      if (requiredIds.has(positive(a.id))) {
        need(a.expired === false && timestamp(a.expires_at) > done);
      }
    }
    need(done >= end && done - timestamp(finalQa.run_started_at) <= o.maxAgeMs);
    return {
      ...SCOPE,
      matches: true,
      sourceRunId: o.sourceRunId,
      sourceAttempt: o.sourceAttempt,
      qaRunId: o.qaRunId,
      qaAttempt: o.qaAttempt,
      checkedAt,
      archiveBytesVerified: true,
    };
  } catch {
    return { ...SCOPE, matches: false, stage, code };
  }
}
