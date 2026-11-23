// SPDX-License-Identifier: MPL-2.0

import type { ReviewCommentInput } from "./fdh_review.ts";

export interface PullRequestIdentity {
  number: number;
  headSha: string;
  headRef: string;
  headRepo: string;
}

export interface TrustedCheckInput {
  prNumber: number;
  expectedHeadSha: string;
  currentHeadSha: string;
  comments: readonly ReviewCommentInput[];
}

export interface TrustedCheckResult {
  name: "fdh-trusted-receipts";
  headSha: string;
  conclusion: "success" | "failure";
  summary: string;
}

export interface TrustedCheckCliInput {
  prNumber: number;
  expectedHeadSha: string;
  currentHeadSha: string;
  comments: readonly ReviewCommentInput[];
}
