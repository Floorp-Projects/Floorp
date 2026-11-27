// SPDX-License-Identifier: MPL-2.0
import type { AuditEvent, AuditTarget } from "./fdh_audit_types.ts";
import { REVIEW_VERDICTS, type ReviewVerdict } from "./fdh_review.ts";

export function isAuditId(value: unknown): value is string {
  return typeof value === "string" &&
    /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
      .test(value);
}

export function isAuditDigest(value: unknown): value is string {
  return typeof value === "string" && /^[0-9a-f]{64}$/.test(value);
}

function invalid(): never {
  throw new Error("Invalid audit event");
}

function fields(value: unknown, allowed: string[]): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) invalid();
  const record = value as Record<string, unknown>;
  if (Object.keys(record).some((key) => !allowed.includes(key))) invalid();
  return record;
}

function parseTarget(input: unknown): AuditTarget {
  const kind = fields(input, ["kind", "key", "repo", "pr", "run", "head"]).kind;
  if (kind === "jira-issue") {
    const { key } = fields(input, ["kind", "key"]);
    if (typeof key !== "string" || !/^KAN-[1-9][0-9]{0,8}$/.test(key)) {
      invalid();
    }
    return { kind, key };
  }
  if (kind !== "pull-request" && kind !== "ci-run") invalid();
  const numberKey = kind === "pull-request" ? "pr" : "run";
  const { repo, head, [numberKey]: number } = fields(input, [
    "kind",
    "repo",
    "head",
    numberKey,
  ]);
  if (
    (repo !== "Floorp" && repo !== "Floorp-Runtime") ||
    typeof head !== "string" || !/^[0-9a-f]{40}$/.test(head) ||
    typeof number !== "number" || !Number.isSafeInteger(number) || number < 1
  ) invalid();
  return kind === "pull-request"
    ? { kind, repo, pr: number, head }
    : { kind, repo, run: number, head };
}

function parseOutcome(input: unknown): AuditEvent["outcome"] {
  const kind = fields(input, [
    "kind",
    "intentId",
    "status",
    "artifactSha256",
    "reviewVerdict",
  ]).kind;
  if (kind === "intent") {
    fields(input, ["kind"]);
    return { kind };
  }
  if (kind === "receipt") {
    const { artifactSha256, reviewVerdict } = fields(input, [
      "kind",
      "artifactSha256",
      "reviewVerdict",
    ]);
    if (
      !isAuditDigest(artifactSha256) || (reviewVerdict !== undefined &&
        !REVIEW_VERDICTS.some((verdict) => verdict === reviewVerdict))
    ) invalid();
    return {
      kind,
      artifactSha256,
      reviewVerdict: reviewVerdict as ReviewVerdict | undefined,
    };
  }
  if (kind !== "result") invalid();
  const { intentId, status, artifactSha256 } = fields(input, [
    "kind",
    "intentId",
    "status",
    "artifactSha256",
  ]);
  if (
    !isAuditId(intentId) ||
    (status !== "succeeded" && status !== "failed" && status !== "denied") ||
    (artifactSha256 !== undefined && !isAuditDigest(artifactSha256))
  ) invalid();
  return { kind, intentId, status, artifactSha256 };
}

/** Allow structured references only. Never copy caller objects or diagnostics. */
export function parseAuditEvent(input: unknown): AuditEvent {
  const {
    id,
    traceId,
    operation,
    target,
    agentRunId,
    toolRunId,
    runnerRunId,
    outcome,
  } = fields(input, [
    "id",
    "traceId",
    "operation",
    "target",
    "agentRunId",
    "toolRunId",
    "runnerRunId",
    "outcome",
  ]);
  if (
    !isAuditId(id) || !isAuditId(traceId) ||
    (operation !== "review" && operation !== "evidence" && operation !== "qa" &&
      operation !== "github-change" && operation !== "jira-change" &&
      operation !== "release") ||
    (agentRunId !== undefined && !isAuditId(agentRunId)) ||
    (toolRunId !== undefined && !isAuditId(toolRunId)) ||
    (runnerRunId !== undefined && !isAuditId(runnerRunId))
  ) invalid();
  const parsedTarget = parseTarget(target);
  const parsedOutcome = parseOutcome(outcome);
  if (
    (operation === "jira-change") !== (parsedTarget.kind === "jira-issue") ||
    (parsedOutcome.kind === "receipt" &&
      ((operation !== "review" && operation !== "evidence" &&
        operation !== "qa") ||
        (operation === "review") !==
          (parsedOutcome.reviewVerdict !== undefined)))
  ) invalid();
  return {
    id,
    traceId,
    operation,
    target: parsedTarget,
    agentRunId,
    toolRunId,
    runnerRunId,
    outcome: parsedOutcome,
  };
}
