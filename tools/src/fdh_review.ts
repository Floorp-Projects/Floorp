// SPDX-License-Identifier: MPL-2.0

/**
 * fdh review receipts (FDH D-14).
 *
 * A review receipt is the machine-readable artifact of an independent LLM
 * review: who reviewed (`reviewer`), with which model (`model`), at which
 * commit (`commit`), with which verdict (`verdict`), whether it blocks
 * (`blocking`), what was reviewed (`scope`), and at which review tier
 * (`tier`). Receipts are posted as PR comments inside a marker so a CI gate
 * can parse them without trusting free-form prose:
 *
 *   <!-- fdh-review-receipt:v1 {"schemaVersion":1,...} -->
 *
 * This module is local-only and deterministic: no network, no GitHub calls.
 * The workflow (`.github/workflows/fdh-review-gate.yml`) fetches comments and
 * feeds them to `parseReceiptComments` + `validateReceiptSet`.
 */

/** Schema version of the receipt payload carried in the marker. */
export const FDH_REVIEW_RECEIPT_SCHEMA_VERSION = 1;

/** Marker prefix that identifies a receipt comment. */
export const FDH_REVIEW_RECEIPT_MARKER = "fdh-review-receipt:v1";

/** Verdicts a reviewer may record on a receipt. */
export const REVIEW_VERDICTS = [
  "approve",
  "approve_with_notes",
  "request_changes",
] as const;
export type ReviewVerdict = (typeof REVIEW_VERDICTS)[number];

/** Verdicts that allow a merge (the gate rejects `request_changes`). */
export const MERGEABLE_REVIEW_VERDICTS = [
  "approve",
  "approve_with_notes",
] as const;

/** Review tiers from D-11: full read, evidence only, contract + spot read. */
export const REVIEW_TIERS = [
  "full_read",
  "evidence_only",
  "contract_spot",
] as const;
export type ReviewTier = (typeof REVIEW_TIERS)[number];

/** The artifact an independent reviewer posts for a PR head commit. */
export interface ReviewReceipt {
  schemaVersion: 1;
  /** Reviewer identity (agent name, model-run id, or bot login). */
  reviewer: string;
  /** LLM model that performed the review. */
  model: string;
  /** Full commit SHA the review was performed against. */
  commit: string;
  verdict: ReviewVerdict;
  /** True when the reviewer found a merge-blocking defect. */
  blocking: boolean;
  /** What the reviewer covered (paths, tiers, or a short scope label). */
  scope: string;
  tier: ReviewTier;
}

/** A PR comment body, as returned by the GitHub comments API. */
export interface ReviewCommentInput {
  body?: string | null;
}

/** Distinct-reviewer summary produced by `validateReceiptSet`. */
export interface ReviewReceiptSummary {
  headSha: string;
  receipts: ReviewReceipt[];
  /** Lower-cased distinct reviewer identities (sorted). */
  reviewers: string[];
  /** Distinct model ids among the receipts (sorted). */
  models: string[];
}

const RECEIPT_MARKER_RE = /<!--\s*fdh-review-receipt:v1\s+([\s\S]*?)-->/g;

function requireString(value: unknown, field: string): string {
  if (typeof value !== "string" || value.trim() === "") {
    throw new Error(`review receipt field ${field} must be a non-empty string`);
  }
  return value.trim();
}

/**
 * Extract receipt payloads from PR comments. A marker with malformed JSON
 * throws, so a corrupted receipt fails the gate instead of being ignored.
 */
export function parseReceiptComments(
  comments: readonly ReviewCommentInput[],
): ReviewReceipt[] {
  const receipts: ReviewReceipt[] = [];
  comments.forEach((comment, commentIndex) => {
    const body = comment.body ?? "";
    for (const match of body.matchAll(RECEIPT_MARKER_RE)) {
      const payload = match[1].trim();
      let parsed: unknown;
      try {
        parsed = JSON.parse(payload);
      } catch {
        throw new Error(
          `review receipt marker in comment #${commentIndex} is not valid JSON`,
        );
      }
      receipts.push(parsed as ReviewReceipt);
    }
  });
  return receipts;
}

/**
 * Validate one receipt against the PR head commit. Enforces the receipt
 * schema, `commit === headSha`, a mergeable verdict, and `blocking === false`.
 */
export interface ValidateReceiptOptions {
  /** When false, accept request_changes/blocking=true (a local record only). */
  requireMergeable?: boolean;
}

export function validateReceipt(
  receipt: unknown,
  headSha: string,
  options: ValidateReceiptOptions = {},
): ReviewReceipt {
  const requireMergeable = options.requireMergeable ?? true;
  if (typeof receipt !== "object" || receipt === null) {
    throw new Error("review receipt must be a JSON object");
  }
  const value = receipt as Record<string, unknown>;
  const expectedHead = requireString(headSha, "head SHA").toLowerCase();

  if (value.schemaVersion !== FDH_REVIEW_RECEIPT_SCHEMA_VERSION) {
    throw new Error(
      `review receipt schemaVersion must be ${FDH_REVIEW_RECEIPT_SCHEMA_VERSION}`,
    );
  }
  const reviewer = requireString(value.reviewer, "reviewer");
  const model = requireString(value.model, "model");
  const commit = requireString(value.commit, "commit").toLowerCase();
  if (commit !== expectedHead) {
    throw new Error(
      `review receipt commit ${commit} does not match PR head ${expectedHead}`,
    );
  }
  const verdict = requireString(value.verdict, "verdict");
  if (!(REVIEW_VERDICTS as readonly string[]).includes(verdict)) {
    throw new Error(
      `review receipt verdict ${verdict} is not one of ${
        REVIEW_VERDICTS.join(", ")
      }`,
    );
  }
  if (typeof value.blocking !== "boolean") {
    throw new Error("review receipt field blocking must be a boolean");
  }
  if (
    requireMergeable &&
    !(MERGEABLE_REVIEW_VERDICTS as readonly string[]).includes(verdict)
  ) {
    throw new Error(
      `review receipt verdict ${verdict} is not mergeable; use approve or approve_with_notes`,
    );
  }
  if (requireMergeable && value.blocking) {
    throw new Error(
      "review receipt has blocking=true; a blocking review cannot merge",
    );
  }
  const scope = requireString(value.scope, "scope");
  const tier = requireString(value.tier, "tier");
  if (!(REVIEW_TIERS as readonly string[]).includes(tier)) {
    throw new Error(
      `review receipt tier ${tier} is not one of ${REVIEW_TIERS.join(", ")}`,
    );
  }

  return {
    schemaVersion: FDH_REVIEW_RECEIPT_SCHEMA_VERSION,
    reviewer,
    model,
    commit,
    verdict: verdict as ReviewVerdict,
    blocking: value.blocking,
    scope,
    tier: tier as ReviewTier,
  };
}

/**
 * Validate every receipt against the head commit and require at least
 * `minDistinctReviewers` distinct reviewer identities. The count is by
 * reviewer, not by receipt, so one reviewer posting twice cannot pass.
 */
export function validateReceiptSet(
  receipts: readonly unknown[],
  headSha: string,
  minDistinctReviewers = 2,
): ReviewReceiptSummary {
  const expectedHead = requireString(headSha, "head SHA").toLowerCase();
  const valid = receipts.map((receipt) =>
    validateReceipt(receipt, expectedHead)
  );
  const reviewers = [...new Set(valid.map((r) => r.reviewer.toLowerCase()))]
    .sort();
  const models = [...new Set(valid.map((r) => r.model))].sort();
  if (reviewers.length < minDistinctReviewers) {
    throw new Error(
      `review gate requires at least ${minDistinctReviewers} distinct reviewers at head ${expectedHead}; found ${reviewers.length} (${
        reviewers.join(", ") || "none"
      })`,
    );
  }
  return {
    headSha: expectedHead,
    receipts: valid,
    reviewers,
    models,
  };
}

/** A full git object id: SHA-1 (40 hex) or SHA-256 (64 hex), lower case. */
const FULL_COMMIT_RE = /^[0-9a-f]{40}(?:[0-9a-f]{24})?$/;

/**
 * Local receipt file name: `<task>-<commit12>-<reviewer>.json`. The commit
 * must be a full hex SHA and the reviewer is reduced to `[A-Za-z0-9._-]`, so
 * the name cannot leave the reviews directory (the task id is validated by
 * the CLI). Reviewers that differ only in characters outside that set, or
 * after 64 characters, share a file.
 */
export function receiptFileName(
  taskId: string,
  commit: string,
  reviewer: string,
): string {
  if (!FULL_COMMIT_RE.test(commit)) {
    throw new Error(
      `--commit must be a full lower-case hex commit SHA (40 or 64 characters), got ${
        JSON.stringify(commit)
      }`,
    );
  }
  const safeReviewer = reviewer.replace(/[^A-Za-z0-9._-]+/g, "_")
    .replace(/^[._]+/, "")
    .slice(0, 64) || "reviewer";
  return `${taskId}-${commit.slice(0, 12)}-${safeReviewer}.json`;
}
