// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  parsePullRequestIdentity,
  trustedReceiptFailure,
  validateTrustedReceiptCheck,
} from "./fdh_trusted_review.ts";

const HEAD = "9955ae2e96646d1e323c241fa738b06ea33cf553";
const OTHER = "a".repeat(40);

function receipt(reviewer: string, commit = HEAD) {
  return {
    schemaVersion: 1,
    reviewer,
    model: "test-model",
    commit,
    verdict: "approve",
    blocking: false,
    scope: "changed files",
    tier: "contract_spot",
  };
}
function comment(value: unknown, author: string, authorAssociation: string) {
  return {
    body: `<!-- fdh-review-receipt:v1 ${JSON.stringify(value)} -->`,
    author,
    authorAssociation,
  };
}

Deno.test("trusted receipt check publishes success only for two head-bound trusted receipts", () => {
  const result = validateTrustedReceiptCheck({
    prNumber: 42,
    expectedHeadSha: HEAD,
    currentHeadSha: HEAD,
    comments: [
      comment(receipt("reviewer-a"), "member", "MEMBER"),
      comment(receipt("reviewer-b"), "collab", "COLLABORATOR"),
    ],
  });
  assertEquals(result.name, "fdh-trusted-receipts");
  assertEquals(result.headSha, HEAD);
  assertEquals(result.conclusion, "success");
});

Deno.test("trusted receipt check retains legacy MEMBER association policy", () => {
  const result = validateTrustedReceiptCheck({
    prNumber: 42,
    expectedHeadSha: HEAD,
    currentHeadSha: HEAD,
    comments: [
      comment(receipt("reviewer-a"), "member-a", "MEMBER"),
      comment(receipt("reviewer-b"), "member-b", "member"),
    ],
  });
  assertEquals(result.conclusion, "success");
});

Deno.test("trusted receipt check ignores stale and untrusted receipts per D-14", () => {
  const result = validateTrustedReceiptCheck({
    prNumber: 42,
    expectedHeadSha: HEAD,
    currentHeadSha: HEAD,
    comments: [
      comment(receipt("old-reviewer", OTHER), "member", "MEMBER"),
      comment(receipt("outsider"), "outsider", "NONE"),
      comment(receipt("reviewer-a"), "member-a", "MEMBER"),
      comment(receipt("reviewer-b"), "collab", "COLLABORATOR"),
    ],
  });
  assertEquals(result.conclusion, "success");
  assertEquals(result.summary.includes("1 stale receipt(s) ignored"), true);
  assertEquals(
    result.summary.includes("1 untrusted receipt comment(s) ignored"),
    true,
  );
});

Deno.test("trusted receipt check fails for insufficient, malformed, or nonmergeable head receipts", () => {
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 42,
        expectedHeadSha: HEAD,
        currentHeadSha: HEAD,
        comments: [comment(receipt("reviewer-a"), "member", "MEMBER")],
      }),
    Error,
    "distinct reviewers",
  );
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 42,
        expectedHeadSha: HEAD,
        currentHeadSha: HEAD,
        comments: [{
          body: "<!-- fdh-review-receipt:v1 {bad} -->",
          author: "member",
          authorAssociation: "MEMBER",
        }],
      }),
    Error,
    "not valid JSON",
  );
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 42,
        expectedHeadSha: HEAD,
        currentHeadSha: HEAD,
        comments: [
          comment(receipt("reviewer-a"), "member-a", "MEMBER"),
          comment(
            { ...receipt("reviewer-b"), blocking: true },
            "member-b",
            "MEMBER",
          ),
        ],
      }),
    Error,
    "blocking",
  );
});

Deno.test("trusted receipt check refuses stale head races and malformed SHA/PR number", () => {
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 42,
        expectedHeadSha: HEAD,
        currentHeadSha: OTHER,
        comments: [],
      }),
    Error,
    "head changed",
  );
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 0,
        expectedHeadSha: HEAD,
        currentHeadSha: HEAD,
        comments: [],
      }),
    Error,
    "positive safe integer",
  );
  assertThrows(
    () =>
      validateTrustedReceiptCheck({
        prNumber: 42,
        expectedHeadSha: "deadbeef",
        currentHeadSha: HEAD,
        comments: [],
      }),
    Error,
    "40-character",
  );
});

Deno.test("pull request identity parser binds PR number, SHA, repo, and safe ref data", () => {
  const value = {
    number: 42,
    head: { sha: HEAD, ref: "topic", repo: { full_name: "fork/project" } },
  };
  assertEquals(parsePullRequestIdentity(value, 42), {
    number: 42,
    headSha: HEAD,
    headRef: "topic",
    headRepo: "fork/project",
  });
  assertThrows(
    () => parsePullRequestIdentity(value, 41),
    Error,
    "does not match",
  );
  assertThrows(
    () =>
      parsePullRequestIdentity({
        ...value,
        head: { ...value.head, ref: "bad\tref" },
      }, 42),
    Error,
    "invalid",
  );
  assertThrows(
    () =>
      parsePullRequestIdentity({
        ...value,
        head: { ...value.head, sha: OTHER.toUpperCase() },
      }, 42),
    Error,
    "lowercase",
  );
});

Deno.test("trusted receipt failure is an exact-head failure result", () => {
  const result = trustedReceiptFailure(HEAD, new Error("invalid receipt set"));
  assertEquals(result.name, "fdh-trusted-receipts");
  assertEquals(result.headSha, HEAD);
  assertEquals(result.conclusion, "failure");
  assertEquals(result.summary, "invalid receipt set");
});
