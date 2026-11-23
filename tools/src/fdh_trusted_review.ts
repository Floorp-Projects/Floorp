// SPDX-License-Identifier: MPL-2.0

import {
  parseReceiptComments,
  selectTrustedReceiptComments,
  validateReceiptSet,
} from "./fdh_review.ts";
import type {
  PullRequestIdentity,
  TrustedCheckInput,
  TrustedCheckResult,
} from "./fdh_trusted_review_types.ts";

const FULL_SHA = /^[0-9a-f]{40}$/;

function validSha(value: unknown, label: string): string {
  if (typeof value !== "string" || !FULL_SHA.test(value)) {
    throw new Error(`${label} must be a lowercase 40-character commit SHA`);
  }
  return value;
}

/** Normalize untrusted GitHub API data and require the exact requested PR. */
export function parsePullRequestIdentity(
  value: unknown,
  expectedNumber: number,
): PullRequestIdentity {
  if (typeof value !== "object" || value === null) {
    throw new Error("pull request response must be an object");
  }
  const pr = value as Record<string, unknown>;
  const head = pr.head as Record<string, unknown> | null;
  const repo = head?.repo as Record<string, unknown> | null;
  if (pr.number !== expectedNumber) {
    throw new Error("pull request number does not match requested PR");
  }
  const headSha = validSha(head?.sha, "pull request head SHA");
  const headRef = head?.ref;
  const headRepo = repo?.full_name;
  if (
    typeof headRef !== "string" || headRef.length === 0 ||
    headRef.includes("\t")
  ) {
    throw new Error("pull request head ref is missing or invalid");
  }
  if (typeof headRepo !== "string" || !/^[^/\s]+\/[^/\s]+$/.test(headRepo)) {
    throw new Error("pull request head repository is missing or invalid");
  }
  return { number: expectedNumber, headSha, headRef, headRepo };
}

/**
 * Build the exact receipt-only check result. The caller must fetch the PR and
 * comments with a trusted API client, create a pending check on expectedHeadSha,
 * then call this again with a freshly fetched currentHeadSha before success.
 */
export function validateTrustedReceiptCheck(
  input: TrustedCheckInput,
): TrustedCheckResult {
  const headSha = validSha(input.expectedHeadSha, "expected head SHA");
  const currentHeadSha = validSha(input.currentHeadSha, "current head SHA");
  if (!Number.isSafeInteger(input.prNumber) || input.prNumber <= 0) {
    throw new Error("PR number must be a positive safe integer");
  }
  if (headSha !== currentHeadSha) {
    throw new Error("pull request head changed during receipt validation");
  }
  const { trusted, ignored } = selectTrustedReceiptComments(input.comments);
  const summary = validateReceiptSet(parseReceiptComments(trusted), headSha);
  return {
    name: "fdh-trusted-receipts",
    headSha,
    conclusion: "success",
    summary:
      `Two distinct trusted receipts validated at ${headSha}; ${summary.staleReceipts} stale receipt(s) ignored; ${ignored.length} untrusted receipt comment(s) ignored.`,
  };
}

/** Failure is a check result, not an exception that could skip publication. */
export function trustedReceiptFailure(
  expectedHeadSha: string,
  error: unknown,
): TrustedCheckResult {
  const headSha = validSha(expectedHeadSha, "expected head SHA");
  const safeMessage = error instanceof Error
    ? error.message
    : "receipt validation failed";
  return {
    name: "fdh-trusted-receipts",
    headSha,
    conclusion: "failure",
    summary: safeMessage.slice(0, 1000),
  };
}
