// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertThrows } from "@std/assert";
import {
  parseReceiptComments,
  receiptFileName,
  REVIEW_TIERS,
  selectTrustedReceiptComments,
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

Deno.test("fdh review: receipts for other commits are ignored, not counted", () => {
  // A stale receipt from an earlier head cannot make up the second reviewer.
  assertThrows(
    () =>
      validateReceiptSet(
        [receipt(), receipt({ reviewer: "agent-b", commit: "c".repeat(40) })],
        HEAD,
      ),
    Error,
    "1 receipt(s) for other commits ignored",
  );
  // Nor can it block a head that has two valid receipts.
  const summary = validateReceiptSet(
    [
      receipt({ commit: "c".repeat(40), verdict: "request_changes" }),
      receipt({ commit: "d".repeat(40), schemaVersion: 99 }),
      { not: "a receipt" },
      receipt(),
      receipt({ reviewer: "agent-b" }),
    ],
    HEAD,
  );
  assertEquals(summary.reviewers, ["agent-a", "agent-b"]);
  assertEquals(summary.staleReceipts, 3);
  // Upper-case head SHAs in receipts still count as the head.
  assertEquals(
    validateReceiptSet(
      [receipt(), receipt({ reviewer: "agent-b", commit: HEAD.toUpperCase() })],
      HEAD,
    ).staleReceipts,
    0,
  );
  assertThrows(
    () => validateReceiptSet([], HEAD),
    Error,
    "at least 2 distinct reviewers",
  );
});

Deno.test("fdh review: an invalid receipt at the head still fails the set gate", () => {
  assertThrows(
    () =>
      validateReceiptSet(
        [receipt(), receipt({ reviewer: "agent-b", blocking: true })],
        HEAD,
      ),
    Error,
    "blocking",
  );
  assertThrows(
    () =>
      validateReceiptSet(
        [
          receipt(),
          receipt({ reviewer: "agent-b", verdict: "request_changes" }),
        ],
        HEAD,
      ),
    Error,
    "not mergeable",
  );
});

Deno.test("fdh review: only receipts from trusted comment authors count", () => {
  const marker = (r: Record<string, unknown>) =>
    `<!-- fdh-review-receipt:v1 ${JSON.stringify(r)} -->`;
  const comments = [
    {
      body: marker(receipt()),
      author: "maintainer",
      authorAssociation: "MEMBER",
    },
    {
      body: marker(receipt({ reviewer: "agent-b" })),
      author: "outsider",
      authorAssociation: "NONE",
    },
    // A broken marker from an outsider must not fail the gate either.
    {
      body: "<!-- fdh-review-receipt:v1 {not json -->",
      author: "spammer",
      authorAssociation: "FIRST_TIME_CONTRIBUTOR",
    },
    { body: "plain discussion", author: "outsider", authorAssociation: "NONE" },
    {
      body: marker(receipt({ reviewer: "agent-c" })),
      author: "collab",
      authorAssociation: "collaborator",
    },
    { body: marker(receipt({ reviewer: "agent-d" })), author: "anon" },
  ];
  const { trusted, ignored } = selectTrustedReceiptComments(comments);
  assertEquals(trusted.map((c) => c.author), ["maintainer", "collab"]);
  assertEquals(ignored, [
    { author: "outsider", authorAssociation: "NONE" },
    { author: "spammer", authorAssociation: "FIRST_TIME_CONTRIBUTOR" },
    { author: "anon", authorAssociation: "(missing)" },
  ]);
  const summary = validateReceiptSet(parseReceiptComments(trusted), HEAD);
  assertEquals(summary.reviewers, ["agent-a", "agent-c"]);
  // The outsider's receipt alone would not make a second reviewer.
  assertThrows(
    () =>
      validateReceiptSet(
        parseReceiptComments(
          selectTrustedReceiptComments(comments.slice(0, 2)).trusted,
        ),
        HEAD,
      ),
    Error,
    "distinct reviewers",
  );
});

Deno.test("fdh review: non-mergeable receipts validate locally", () => {
  const entry = receipt({
    verdict: "request_changes",
    blocking: true,
  });
  const validated = validateReceipt(entry, HEAD, { requireMergeable: false });
  assertEquals(validated.verdict, "request_changes");
  assertEquals(validated.blocking, true);
});

Deno.test("fdh review: receipts of two reviewers at one head get distinct files", () => {
  const head = "66a1ed79ba18c067921f9a450646aea5100df683";
  const a = receiptFileName("win-t01", head, "reviewer-a-fable");
  const b = receiptFileName("win-t01", head, "reviewer-b-opus");
  assertEquals(a, "win-t01-66a1ed79ba18-reviewer-a-fable.json");
  assertEquals(b, "win-t01-66a1ed79ba18-reviewer-b-opus.json");
  // Path separators and dot prefixes cannot escape the reviews directory.
  assertEquals(
    receiptFileName("t", head, "../..\\evil/name"),
    "t-66a1ed79ba18-evil_name.json",
  );
  // The commit segment must be a full hex SHA, so it cannot carry a path.
  for (const commit of ["/../../../..", "HEAD", "66a1ed79", ""]) {
    assertThrows(() => receiptFileName("t", commit, "r"), Error, "SHA");
  }
  assertEquals(
    receiptFileName("t", "a".repeat(64), "r"),
    "t-aaaaaaaaaaaa-r.json",
  );
  assertEquals(
    receiptFileName("t", head, "..."),
    "t-66a1ed79ba18-reviewer.json",
  );
});
