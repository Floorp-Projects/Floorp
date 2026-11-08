// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  parseReceiptComments,
  REVIEW_TIERS,
  validateReceipt,
  validateReceiptSet,
} from "./fdh_review.ts";

const HEAD = "9955ae2e96646d1e323c241fa738b06ea33cf553";

function receipt(overrides: Record<string, unknown> = {}) {
  return {
    schemaVersion: 1,
    reviewer: "agent-a",
    model: "mimo-2.6",
    commit: HEAD,
    verdict: "approve",
    blocking: false,
    scope: "tools/src/fdh_review.ts",
    tier: "contract_spot",
    ...overrides,
  };
}

Deno.test("fdh review: a receipt at the head commit validates", () => {
  const validated = validateReceipt(receipt(), HEAD);
  assertEquals(validated.reviewer, "agent-a");
  assertEquals(validated.commit, HEAD);
  assertEquals(validated.verdict, "approve");
});

Deno.test("fdh review: a receipt for another commit fails", () => {
  assertThrows(
    () => validateReceipt(receipt({ commit: "a".repeat(40) }), HEAD),
    Error,
    "does not match PR head",
  );
  // stale receipt from an earlier push is rejected against the new head
  assertThrows(
    () => validateReceipt(receipt(), "b".repeat(40)),
    Error,
    "does not match PR head",
  );
});

Deno.test("fdh review: request_changes and unknown verdicts are not mergeable", () => {
  assertThrows(
    () => validateReceipt(receipt({ verdict: "request_changes" }), HEAD),
    Error,
    "not mergeable",
  );
  assertThrows(
    () => validateReceipt(receipt({ verdict: "lgtm" }), HEAD),
    Error,
    "verdict lgtm is not one of",
  );
});

Deno.test("fdh review: blocking=true cannot merge", () => {
  assertThrows(
    () => validateReceipt(receipt({ blocking: true }), HEAD),
    Error,
    "blocking=true",
  );
  assertThrows(
    () => validateReceipt(receipt({ blocking: "false" }), HEAD),
    Error,
    "must be a boolean",
  );
});

Deno.test("fdh review: required fields are enforced", () => {
  for (
    const [field, value] of [
      ["schemaVersion", 2],
      ["reviewer", ""],
      ["model", "  "],
      ["scope", ""],
      ["tier", "none"],
    ] as const
  ) {
    assertThrows(
      () => validateReceipt(receipt({ [field]: value }), HEAD),
      Error,
      "review receipt",
    );
  }
  assertEquals(
    (REVIEW_TIERS as readonly string[]).includes("full_read"),
    true,
  );
});

Deno.test("fdh review: parse extracts markers and ignores prose", () => {
  const comments = [
    { body: "CodeRabbit summary without a marker" },
    {
      body: [
        "Review 1",
        `<!-- fdh-review-receipt:v1 ${JSON.stringify(receipt())} -->`,
      ].join("\n"),
    },
    {
      body: `before\n<!-- fdh-review-receipt:v1 ${
        JSON.stringify(
          receipt({ reviewer: "agent-b", model: "deepseek" }),
        )
      } -->\nafter`,
    },
  ];
  const parsed = parseReceiptComments(comments);
  assertEquals(parsed.length, 2);
  assertEquals(parsed.map((r) => r.reviewer), ["agent-a", "agent-b"]);
});

Deno.test("fdh review: a malformed marker fails parsing", () => {
  assertThrows(
    () =>
      parseReceiptComments([
        { body: "<!-- fdh-review-receipt:v1 {not json} -->" },
      ]),
    Error,
    "not valid JSON",
  );
});

Deno.test("fdh review: two distinct reviewers pass the set gate", () => {
  const summary = validateReceiptSet(
    [receipt(), receipt({ reviewer: "agent-b", model: "deepseek" })],
    HEAD,
  );
  assertEquals(summary.reviewers, ["agent-a", "agent-b"]);
  assertEquals(summary.models.length, 2);
  assertEquals(summary.receipts.length, 2);
});

Deno.test("fdh review: one reviewer posting twice is not two reviews", () => {
  assertThrows(
    () => validateReceiptSet([receipt(), receipt()], HEAD),
    Error,
    "distinct reviewers",
  );
  // reviewer identity is compared case-insensitively
  assertThrows(
    () =>
      validateReceiptSet(
        [receipt({ reviewer: "Agent-A" }), receipt({ reviewer: "agent-a" })],
        HEAD,
      ),
    Error,
    "distinct reviewers",
  );
});

Deno.test("fdh review: an invalid receipt anywhere fails the set gate", () => {
  assertThrows(
    () =>
      validateReceiptSet(
        [receipt(), receipt({ reviewer: "agent-b", commit: "c".repeat(40) })],
        HEAD,
      ),
    Error,
    "does not match PR head",
  );
  assertThrows(
    () => validateReceiptSet([], HEAD),
    Error,
    "at least 2 distinct reviewers",
  );
});
