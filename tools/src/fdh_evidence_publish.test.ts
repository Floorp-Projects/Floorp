// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import { createTaskState, writeTaskState } from "./fdh_task.ts";
import { buildEvidenceManifest } from "./fdh_evidence.ts";
import {
  evidenceCommentBody,
  FDH_EVIDENCE_MARKER,
  MAX_EVIDENCE_COMMENT_CHARS,
  parseEvidenceComments,
  traceEvidence,
} from "./fdh_evidence_publish.ts";
import { FDH_REVIEW_RECEIPT_MARKER } from "./fdh_review.ts";
import type { GhRunner } from "./fdh_ext_poll.ts";
import type { EvidenceManifest } from "../qa3/evidence/types.ts";

const HEAD = "1d687fb15a6dfb9b1aba11386d9e8396852d1e41";
const OLD = "c8e265fa8665d420d126ba87a87feaa7e8c1764a";

async function manifestIn(dir: string, outputs = 1): Promise<EvidenceManifest> {
  const state = createTaskState({
    taskId: "ev-1",
    worktree: dir,
    baseCommit: "b".repeat(40),
    portOffset: 0,
    now: () => "2026-11-11T00:00:00.000Z",
  });
  for (let i = 0; i < outputs; i++) {
    state.operations.push({
      command: "/fake/deno",
      args: ["task", "test:host"],
      displayCommand: "deno task test:host",
      cwd: dir,
      startedAt: "2026-11-11T00:01:00.000Z",
      endedAt: "2026-11-11T00:02:00.000Z",
      exitCode: 0,
      // Random text compresses poorly: enough of it forces gzip, then too big.
      output: i === 0 ? "ok" : crypto.randomUUID().repeat(40),
    });
  }
  state.assertions.push({
    name: "host-test-exit-code",
    expected: "exit code 0",
    actual: "exit code 0",
    passed: true,
    at: "2026-11-11T00:02:00.000Z",
  });
  writeTaskState(state);
  return await buildEvidenceManifest(state, {
    os: { name: "testos", version: "1", arch: "x86_64" },
  });
}

const input = {
  taskId: "ev-1",
  pr: 7,
  commit: HEAD,
  uncommitted: false,
  builtAt: "2026-11-11T00:03:00.000Z",
};
const trustedComment = (body: string, extra = {}) => ({
  body,
  author: "dev",
  authorAssociation: "MEMBER",
  url: "https://example/c",
  ...extra,
});

Deno.test("fdh evidence publish: a comment carries the manifest and reads back intact", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const { body, binding } = await evidenceCommentBody(manifest, input);
    assert(body.startsWith(`<!-- ${FDH_EVIDENCE_MARKER} `));
    assertEquals(binding.encoding, "json");
    assert(body.includes("passed: `host-test-exit-code`"));
    const [parsed] = await parseEvidenceComments([trustedComment(body)]);
    assertEquals([parsed.ok, parsed.trusted, parsed.problems], [
      true,
      true,
      [],
    ]);
    assertEquals(parsed.binding?.sha256, binding.sha256);
    // GitHub may hand the body back with CRLF line ends.
    const [crlf] = await parseEvidenceComments([
      trustedComment(body.replaceAll("\n", "\r\n")),
    ]);
    assertEquals(crlf.ok, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence publish: an edited manifest, a broken marker and an untrusted author are caught", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const { body } = await evidenceCommentBody(await manifestIn(dir), input);
    const edited = body.replace('"passed": true', '"passed": false');
    assert(edited !== body);
    const [tampered] = await parseEvidenceComments([trustedComment(edited)]);
    assertEquals(tampered.ok, false);
    assert(tampered.problems[0].includes("SHA-256"));
    const [broken] = await parseEvidenceComments([
      trustedComment(body.replace('"schemaVersion":1', '"schemaVersion":2')),
    ]);
    assert(broken.problems[0].includes("marker"));
    const [outsider] = await parseEvidenceComments([
      trustedComment(body, { authorAssociation: "CONTRIBUTOR" }),
    ]);
    assertEquals([outsider.ok, outsider.trusted], [true, false]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence publish: a large manifest is compressed, a too large one is refused", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const big = await manifestIn(dir, 60);
    const { body, binding } = await evidenceCommentBody(big, input);
    assertEquals(binding.encoding, "gzip+base64");
    assert(body.length <= MAX_EVIDENCE_COMMENT_CHARS);
    const [parsed] = await parseEvidenceComments([trustedComment(body)]);
    assertEquals(parsed.ok, true);
    const huge = await manifestIn(dir, 1200);
    await assertRejects(
      () => evidenceCommentBody(huge, input),
      Error,
      "does not fit",
    );
    await assertRejects(
      () => evidenceCommentBody(big, { ...input, commit: "abc" }),
      Error,
      "full hex SHA",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

function receipt(reviewer: string, model: string, commit = HEAD): string {
  return `<!-- ${FDH_REVIEW_RECEIPT_MARKER} ${
    JSON.stringify({
      schemaVersion: 1,
      reviewer,
      model,
      commit,
      verdict: "approve",
      blocking: false,
      scope: "full",
      tier: "full_read",
    })
  } -->`;
}

function fakeGh(
  pr: Record<string, unknown>,
  comments: Array<Record<string, unknown>>,
): GhRunner {
  return (args) => {
    if (args[1] === "repos/o/r/pulls/7") {
      return Promise.resolve(JSON.stringify(pr));
    }
    if (args.includes("repos/o/r/issues/7/comments?per_page=100")) {
      return Promise.resolve(
        comments.map((c) => JSON.stringify(c)).join("\n") + "\n",
      );
    }
    return Promise.reject(new Error(`unexpected gh ${args.join(" ")}`));
  };
}

const MERGED = {
  state: "closed",
  merged_at: "2026-11-11T01:00:00Z",
  merge_commit_sha: "6fe7bc6d731f2ebaa96c22371e58a7ca217272d9",
  head: { sha: HEAD },
};

Deno.test("fdh evidence trace: a merged PR leads to its evidence and receipts at the final head", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const atHead = (await evidenceCommentBody(manifest, input)).body;
    const early =
      (await evidenceCommentBody(manifest, { ...input, commit: OLD }))
        .body;
    const report = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(early),
        trustedComment(atHead),
        trustedComment(receipt("reviewer-a", "m-a")),
        trustedComment(receipt("reviewer-b", "m-b")),
        trustedComment(receipt("reviewer-c", "m-c"), {
          authorAssociation: "NONE",
        }),
      ]),
    );
    assertEquals(report.complete, true, report.problems.join("; "));
    assertEquals(report.merged, true);
    assertEquals(report.evidence.map((e) => e.atHead), [false, true]);
    assertEquals(report.receipts.reviewers, ["reviewer-a", "reviewer-b"]);
    assertEquals(report.receipts.ignored, 1);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence trace: evidence only at an older head, by an outsider, or with too few receipts is incomplete", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const early =
      (await evidenceCommentBody(manifest, { ...input, commit: OLD }))
        .body;
    const atHead = (await evidenceCommentBody(manifest, input)).body;
    const stale = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(early),
        trustedComment(receipt("reviewer-a", "m-a")),
        trustedComment(receipt("reviewer-b", "m-b")),
      ]),
    );
    assertEquals(stale.complete, false);
    assert(stale.problems[0].includes("no valid evidence"));
    const outsider = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(atHead, { authorAssociation: "NONE" }),
        trustedComment(receipt("reviewer-a", "m-a")),
        trustedComment(receipt("reviewer-b", "m-b")),
      ]),
    );
    assertEquals(outsider.complete, false);
    const oneReview = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(atHead),
        trustedComment(receipt("reviewer-a", "m-a")),
      ]),
    );
    assertEquals(oneReview.complete, false);
    assert(oneReview.receipts.error?.includes("distinct reviewers"));
    // Evidence rendered for another PR does not count for this one.
    const other =
      (await evidenceCommentBody(manifest, { ...input, pr: 8 })).body;
    const wrongPr = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(other),
        trustedComment(receipt("reviewer-a", "m-a")),
        trustedComment(receipt("reviewer-b", "m-b")),
      ]),
    );
    assertEquals(wrongPr.complete, false);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
