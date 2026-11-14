// SPDX-License-Identifier: MPL-2.0
import { collectFinalPackageQa } from "./fdh_final_package_qa_collector.ts";
import type {
  QaCollectorDependencies,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";
import { createQaGhTransport } from "./fdh_qa_gh_transport.ts";
import type {
  QaGhOperation,
  QaGhTransport,
} from "./fdh_qa_gh_transport_types.ts";
import type {
  QaDiscoveryCandidate,
  QaDiscoveryClassifiedRun,
  QaDiscoveryDependencies,
  QaDiscoveryFailureCode,
  QaDiscoveryOptions,
  QaDiscoveryReport,
  QaDiscoveryStage,
} from "./fdh_qa_discovery_types.ts";
export type {
  QaDiscoveryCandidate,
  QaDiscoveryClassifiedRun,
  QaDiscoveryCollect,
  QaDiscoveryDependencies,
  QaDiscoveryFailureCode,
  QaDiscoveryOptions,
  QaDiscoveryReport,
  QaDiscoveryScope,
  QaDiscoveryStage,
} from "./fdh_qa_discovery_types.ts";

/** Hard discovery bounds: every listed page must be fetched and every listed
 * completed run must be classified, or the whole discovery fails closed.
 * Never scan-first-N-and-succeed. The transport page cap (10) is wider. */
export const QA_DISCOVERY_LIMITS = Object.freeze({
  pages: 3,
  pageSize: 100,
  runs: 300,
  candidates: 8,
});

const QA_WORKFLOW_PATH = ".github/workflows/validate-final-package-qa.yml";
const SOURCE_WORKFLOW_PATH =
  ".github/workflows/package_and_publish_release.yml";
const REPO_ID = 437505831;
const REPO_FULL_NAME = "Floorp-Projects/Floorp";
// Same initial readiness ceiling as the collector, not a freshness policy.
const MAX_AGE_MS = 24 * 60 * 60 * 1000;
const SCOPE = {
  scope: "read-only-point-in-time-qa-discovery",
  transportAuthenticityVerified: false,
  apiAuthenticityVerified: false,
  nativeQaVerified: false,
  liveApprovalVerified: false,
  publicationAuthorized: false,
} as const;

function need(v: unknown): asserts v {
  if (!v) throw new Error("invalid discovery");
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
async function metadata(
  transport: QaGhTransport,
  operation: QaGhOperation,
): Promise<unknown> {
  const result = await transport.request(operation);
  need(result.ok && result.kind === "metadata");
  return JSON.parse(result.jsonText);
}
/** Listing shape projection. The qaHead stays INDEPENDENT of the source head:
 * the QA probe on main may advance while the package run is still pending.
 * Every listed run must satisfy the frozen filter contract exactly; any
 * deviation means the listing lied and the whole window is untrustworthy. */
function classifyRun(v: unknown, sinceMs: number): QaDiscoveryClassifiedRun {
  const r = obj(v);
  const id = positive(r.id);
  const attempt = positive(r.run_attempt);
  const head = str(r.head_sha, 40);
  need(/^[a-f0-9]{40}$/.test(head));
  need(str(r.event, 30) === "workflow_dispatch");
  need(str(r.path, 240) === QA_WORKFLOW_PATH);
  need(str(r.head_branch, 240) === "main");
  const repository = obj(r.repository);
  need(positive(repository.id) === REPO_ID);
  need(str(repository.full_name, 240) === REPO_FULL_NAME);
  const createdAt = str(r.created_at, 30);
  need(timestamp(createdAt) >= sinceMs);
  const workflowId = positive(r.workflow_id);
  const status = str(r.status, 30);
  const conclusion = r.conclusion === null ? null : str(r.conclusion, 30);
  // Canonical fingerprint over every validated identity/state field; later
  // compared against the end-of-discovery re-listing to fail closed on any
  // new/retried/unsettled QA run racing the selection.
  const fingerprint = {
    id,
    attempt,
    head,
    branch: "main",
    event: "workflow_dispatch",
    path: QA_WORKFLOW_PATH,
    workflowId,
    status,
    conclusion,
    createdAt,
    repository: { id: REPO_ID, fullName: REPO_FULL_NAME },
  };
  if (status === "completed") {
    return {
      kind: "candidate",
      candidate: { id, attempt, head },
      fingerprint,
    };
  }
  // Unsettled known statuses are point-in-time uncertainty: the valid set can
  // still change, so discovery must not answer at all. Unknown garbage status
  // is a malformed listing instead; action_required is a CONCLUSION, never a
  // canonical run status, so it is rejected here.
  need(
    status === "queued" || status === "in_progress" ||
      status === "waiting" || status === "requested" || status === "pending",
  );
  return { kind: "pending", fingerprint };
}

/** GET-only, bounded point-in-time discovery + collection composition.
 * Finds the exact unique valid final-package QA run for one source package
 * run, proven ONLY by the existing collector archive input binding, then
 * re-collects the selected run fresh AND re-lists the whole bounded window
 * after the complete candidate scan, so a racing new/retried QA run fails
 * closed instead of hiding behind a stale selection.
 * One shared transport/now/collector budget across every candidate: no
 * per-candidate transport resets, no budget loopholes.
 * Availability limit: ANY completed candidate whose collector verification
 * fails (including evidenceMismatch) fails the whole discovery as uncertain
 * because the redacted collector report cannot distinguish a confident
 * wrong-source binding from malformed/missing archive input; skipping could
 * hide a competing valid QA run behind a false skip.
 * No filesystem, browser, credential reads, approvals, publication or ledger
 * writes. Every failure is fixed/sanitized; nothing escapes. */
export async function discoverAndCollectFinalPackageQa(
  options: QaDiscoveryOptions,
  deps: QaDiscoveryDependencies = {},
): Promise<QaDiscoveryReport> {
  let stage: QaDiscoveryStage = "options";
  let code: QaDiscoveryFailureCode = "invalidOptions";
  try {
    // Snapshot every primitive policy field before any await (collector pattern).
    const keys = [
      "sourceRunId",
      "sourceAttempt",
      "sourceHead",
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
    const o = snapshot as unknown as QaDiscoveryOptions;
    for (const n of [o.sourceRunId, o.sourceAttempt, o.maxAgeMs]) positive(n);
    need(
      typeof o.sourceHead === "string" && o.sourceHead.length === 40 &&
        /^[a-f0-9]{40}$/.test(o.sourceHead),
    );
    need(
      o.windowsPolicy === "require-success" ||
        o.windowsPolicy === "shadow-observation",
    );
    need(o.maxAgeMs <= MAX_AGE_MS);
    const readClock = deps.now ?? (() => new Date().toISOString());
    let lastClock = -Infinity;
    // Collector-local ordering does not span separate invocations. Enforce one
    // monotonic observation clock across discovery and every collector call.
    const now = (): string => {
      const value = readClock();
      const current = timestamp(value);
      need(current >= lastClock);
      lastClock = current;
      return value;
    };
    const start = timestamp(now());
    const transport = deps.transport ?? createQaGhTransport();
    const collect = deps.collect ?? collectFinalPackageQa;
    const collectDeps: QaCollectorDependencies = { transport, now };

    stage = "source";
    code = "sourceRunFailed";
    // The QA listing window starts exactly at the source run creation instant.
    // Dispatch inputs are not in run metadata, so nothing else may narrow it.
    const source = obj(
      await metadata(transport, { kind: "currentRun", runId: o.sourceRunId }),
    );
    need(positive(source.id) === o.sourceRunId);
    need(positive(source.run_attempt) === o.sourceAttempt);
    need(str(source.head_sha, 64) === o.sourceHead);
    // Additive canonical preflight of the source run BEFORE any listing query
    // (the collector remains the full proof, not this preflight): canonical
    // repository identity, package workflow path, dispatch event, main branch,
    // positive workflow id, and the exact prepublication state the legacy gate
    // accepts (waiting/in_progress with null conclusion).
    need(positive(source.workflow_id));
    need(str(source.path, 240) === SOURCE_WORKFLOW_PATH);
    need(str(source.event, 30) === "workflow_dispatch");
    need(str(source.head_branch, 240) === "main");
    const sourceRepo = obj(source.repository);
    need(positive(sourceRepo.id) === REPO_ID);
    need(str(sourceRepo.full_name, 240) === REPO_FULL_NAME);
    const sourceStatus = str(source.status, 30);
    need(sourceStatus === "waiting" || sourceStatus === "in_progress");
    need(source.conclusion === null);
    const sinceMs = timestamp(source.created_at);
    const since = new Date(sinceMs).toISOString();

    // Full bounded listing of the fixed source-created window. Shared by the
    // initial scan and the final race-guard re-listing (same caps, same
    // transport budget); truncation/volume/caps never degrade into a
    // scan-first-N-and-succeed answer.
    const listWindow = async () => {
      let total = -1;
      let fetched = 0;
      let pages = 0;
      const found: QaDiscoveryCandidate[] = [];
      const fingerprints: Record<string, unknown>[] = [];
      const seen = new Set<number>();
      for (let page = 1; page <= QA_DISCOVERY_LIMITS.pages; page++) {
        const response = obj(
          await metadata(transport, { kind: "qaRunsPage", since, page }),
        );
        const count = response.total_count;
        need(
          typeof count === "number" && Number.isSafeInteger(count) &&
            count >= 0,
        );
        if (total < 0) total = count;
        else need(total === count);
        if (total > QA_DISCOVERY_LIMITS.runs) {
          code = "paginationIncomplete";
          throw new Error("run listing exceeds the bounded window");
        }
        const batch = arr(response.workflow_runs);
        need(
          batch.length ===
            Math.min(QA_DISCOVERY_LIMITS.pageSize, total - fetched),
        );
        pages = page;
        for (const raw of batch) {
          const classified = classifyRun(raw, sinceMs);
          fingerprints.push(classified.fingerprint);
          if (classified.kind === "pending") {
            code = "pendingQaRun";
            throw new Error("unsettled qa run inside the window");
          }
          const candidate = classified.candidate;
          need(!seen.has(candidate.id));
          seen.add(candidate.id);
          found.push(candidate);
          if (found.length > QA_DISCOVERY_LIMITS.candidates) {
            code = "candidateCapExceeded";
            throw new Error("qa candidate cap exceeded");
          }
        }
        fetched += batch.length;
        if (fetched >= total) break;
      }
      need(total >= 0);
      if (fetched < total) {
        code = "paginationIncomplete";
        throw new Error("run listing incomplete inside the bounded window");
      }
      return { total, pages, found, fingerprints };
    };

    stage = "runList";
    code = "runListFailed";
    const initial = await listWindow();

    stage = "candidates";
    let selected: QaDiscoveryCandidate | undefined;
    for (const candidate of initial.found) {
      code = "uncertainCandidate";
      const report = await collect(
        {
          sourceRunId: o.sourceRunId,
          sourceAttempt: o.sourceAttempt,
          sourceHead: o.sourceHead,
          qaRunId: candidate.id,
          qaAttempt: candidate.attempt,
          qaHead: candidate.head,
          windowsPolicy: o.windowsPolicy,
          maxAgeMs: o.maxAgeMs,
        } satisfies QaCollectorOptions,
        collectDeps,
      );
      need(report.transportAuthenticityVerified === false);
      need(report.apiAuthenticityVerified === false);
      need(report.nativeQaVerified === false);
      need(report.liveApprovalVerified === false);
      need(report.publicationAuthorized === false);
      if (report.matches) {
        if (selected) {
          code = "ambiguous";
          throw new Error("more than one valid qa run");
        }
        selected = candidate;
      } else {
        // evidenceMismatch included: the redacted collector report cannot
        // separate a confident wrong-source binding from malformed/missing
        // archive input, so every non-matching candidate is unresolvable
        // point-in-time uncertainty. Fail closed, never skip and never
        // accept another run while one candidate is unverifiable.
        throw new Error("unclassifiable candidate");
      }
    }
    if (!selected) {
      code = "noValidRun";
      throw new Error("no valid qa run inside the window");
    }

    stage = "finalRecheck";
    code = "finalRecheckFailed";
    // Report instant captured BEFORE the final selected collect: the collector
    // validates evidence age at its own completion clock, which is >= this
    // instant under the shared monotonic clock, so the reported point-in-time
    // stays inside the collector-verified freshness window even when the
    // window re-listing completes later.
    const checkedAt = now();
    need(timestamp(checkedAt) >= start);
    // Fresh re-collection of the selected run after the COMPLETE candidate
    // scan, so pending deployments/source/job bindings are current.
    const final = await collect(
      {
        sourceRunId: o.sourceRunId,
        sourceAttempt: o.sourceAttempt,
        sourceHead: o.sourceHead,
        qaRunId: selected.id,
        qaAttempt: selected.attempt,
        qaHead: selected.head,
        windowsPolicy: o.windowsPolicy,
        maxAgeMs: o.maxAgeMs,
      } satisfies QaCollectorOptions,
      collectDeps,
    );
    need(final.transportAuthenticityVerified === false);
    need(final.apiAuthenticityVerified === false);
    need(final.nativeQaVerified === false);
    need(final.liveApprovalVerified === false);
    need(final.publicationAuthorized === false);
    if (!final.matches) {
      // Only the collector's own "changed" observation names a state change
      // confidently; everything else stays unexplained failure.
      code = final.code === "changed" ? "changed" : "finalRecheckFailed";
      throw new Error("final recheck failed");
    }
    // The collector checks ordering within its own invocation. Also bind its
    // report instant to discovery: wall time may regress between invocations
    // and later recover before the final completion-clock check.
    need(timestamp(final.checkedAt) >= timestamp(checkedAt));
    // Bounded race guard: the selected-only re-collect cannot prove that no
    // competing QA run appeared during the scan, so re-list the SAME bounded
    // window and require identical totals plus identical full fingerprints.
    const recheck = await listWindow();
    if (
      recheck.total !== initial.total ||
      JSON.stringify(recheck.fingerprints) !==
        JSON.stringify(initial.fingerprints)
    ) {
      code = "changed";
      throw new Error("qa window changed during discovery");
    }
    // Final clock monotonic validation is separate from the report instant:
    // the collector's completion clock must not precede the report instant.
    const completedAt = now();
    need(timestamp(completedAt) >= timestamp(checkedAt));
    return {
      ...SCOPE,
      matches: true,
      sourceRunId: o.sourceRunId,
      sourceAttempt: o.sourceAttempt,
      sourceHead: o.sourceHead,
      qaRunId: selected.id,
      qaAttempt: selected.attempt,
      qaHead: selected.head,
      checkedAt,
      archiveBytesVerified: true,
      runsListed: initial.total,
      pagesFetched: initial.pages,
      candidatesChecked: initial.found.length,
    };
  } catch {
    return { ...SCOPE, matches: false, stage, code };
  }
}
