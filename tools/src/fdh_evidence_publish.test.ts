// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects } from "@std/assert";
import { encodeBase64 } from "@std/encoding/base64";
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

async function manifestIn(
  dir: string,
  options: {
    outputs?: number;
    passed?: boolean;
    name?: string;
    fixed?: boolean;
    status?: "failed" | "cancelled";
  } = {},
): Promise<EvidenceManifest> {
  const state = createTaskState({
    taskId: "ev-1",
    worktree: dir,
    baseCommit: "b".repeat(40),
    portOffset: 0,
    now: () => "2026-11-11T00:00:00.000Z",
  });
  for (let i = 0; i < (options.outputs ?? 1); i++) {
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
  const passed = options.passed ?? true;
  if (options.fixed) {
    // The Gate 3 error-then-fix loop: an earlier failure of the same check.
    state.assertions.push({
      name: options.name ?? "host-test-exit-code",
      expected: "exit code 0",
      actual: "exit code 1",
      passed: false,
      at: "2026-11-11T00:01:30.000Z",
    });
  }
  if (options.status) state.status = options.status;
  state.assertions.push({
    name: options.name ?? "host-test-exit-code",
    expected: "exit code 0",
    actual: passed ? "exit code 0" : "exit code 1",
    passed,
    at: "2026-11-11T00:02:00.000Z",
  });
  writeTaskState(state);
  return await buildEvidenceManifest(state, {
    os: { name: "testos", version: "1", arch: "x86_64" },
  });
}

const input = {
  repo: "o/r",
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
  createdAt: "2026-11-11T00:04:00Z",
  updatedAt: "2026-11-11T00:04:00Z",
  ...extra,
});

Deno.test("fdh evidence publish: a comment carries the manifest and reads back intact", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const { body, binding } = await evidenceCommentBody(manifest, input);
    assert(body.startsWith(`<!-- ${FDH_EVIDENCE_MARKER} `));
    assertEquals([binding.encoding, binding.repo], ["json", "o/r"]);
    assert(body.includes("passed: `host-test-exit-code`"));
    const [parsed] = await parseEvidenceComments([trustedComment(body)]);
    assertEquals([parsed.ok, parsed.trusted, parsed.problems], [
      true,
      true,
      [],
    ]);
    assertEquals([parsed.assertions, parsed.failedAssertions], [1, 0]);
    // GitHub may hand the body back with CRLF line ends.
    const [crlf] = await parseEvidenceComments([
      trustedComment(body.replaceAll("\n", "\r\n")),
    ]);
    assertEquals(crlf.ok, true);
    // An assertion name with a code fence in it does not break the comment.
    const odd = await manifestIn(dir, { name: "x`\n```json\n{}\n```" });
    const [oddBack] = await parseEvidenceComments([
      trustedComment((await evidenceCommentBody(odd, input)).body),
    ]);
    assertEquals(oddBack.ok, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence publish: a stale hash, a broken marker, another task and an untrusted author are caught", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const { body } = await evidenceCommentBody(await manifestIn(dir), input);
    // Same length, so only the SHA-256 can tell.
    const edited = body.replace('"exit code 0"', '"exit code 9"');
    assert(edited !== body);
    const [tampered] = await parseEvidenceComments([trustedComment(edited)]);
    assertEquals(tampered.ok, false);
    assert(tampered.problems.some((p) => p.includes("SHA-256")));
    const [broken] = await parseEvidenceComments([
      trustedComment(body.replace('"schemaVersion":1', '"schemaVersion":2')),
    ]);
    assert(broken.problems[0].includes("marker"));
    const [otherTask] = await parseEvidenceComments([
      trustedComment(body.replace('"taskId":"ev-1"', '"taskId":"ev-2"')),
    ]);
    assert(otherTask.problems[0].includes("not for task ev-2"));
    const [outsider] = await parseEvidenceComments([
      trustedComment(body, { authorAssociation: "CONTRIBUTOR" }),
    ]);
    assertEquals([outsider.ok, outsider.trusted], [false, false]);
    const [later] = await parseEvidenceComments([
      trustedComment(body, { updatedAt: "2026-11-11T05:00:00Z" }),
    ]);
    assertEquals(later.edited, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence publish: a large manifest is compressed, a too large one is refused, a gzip bomb is stopped", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const big = await manifestIn(dir, { outputs: 60 });
    const { body, binding } = await evidenceCommentBody(big, input);
    assertEquals(binding.encoding, "gzip+base64");
    assert(body.length <= MAX_EVIDENCE_COMMENT_CHARS);
    const [parsed] = await parseEvidenceComments([trustedComment(body)]);
    assertEquals(parsed.ok, true);
    const huge = await manifestIn(dir, { outputs: 1200 });
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
    // A block that inflates far beyond the bytes its marker claims.
    const zeros = new Uint8Array(8 * 1024 * 1024);
    const bomb = new Uint8Array(
      await new Response(
        new Blob([zeros]).stream().pipeThrough(new CompressionStream("gzip")),
      ).arrayBuffer(),
    );
    const marker = JSON.stringify({ ...binding, bytes: 1000 });
    const bombBody = `<!-- ${FDH_EVIDENCE_MARKER} ${marker} -->\n` +
      "<details><summary>EvidenceManifest</summary>\n\n" +
      "```text fdh-evidence-gzip-base64\n" + encodeBase64(bomb) +
      "\n```\n\n</details>\n";
    const [stopped] = await parseEvidenceComments([trustedComment(bombBody)]);
    assert(stopped.problems[0].includes("larger than the 1000 bytes"));
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
const reviews = () => [
  trustedComment(receipt("reviewer-a", "m-a")),
  trustedComment(receipt("reviewer-b", "m-b")),
];

Deno.test("fdh evidence trace: a merged PR leads to its evidence and receipts at the final head", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const atHead = (await evidenceCommentBody(manifest, input)).body;
    const early =
      (await evidenceCommentBody(manifest, { ...input, commit: OLD })).body;
    const report = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(early),
        trustedComment(atHead),
        ...reviews(),
        trustedComment(receipt("reviewer-c", "m-c"), {
          authorAssociation: "NONE",
        }),
      ]),
      { requireMerged: true },
    );
    assertEquals(report.complete, true, report.problems.join("; "));
    assertEquals(report.evidence.map((e) => e.counts), [false, true]);
    assertEquals(report.receipts.reviewers, ["reviewer-a", "reviewer-b"]);
    assertEquals(report.receipts.ignored, 1);
    assertEquals(report.notes.length, 1);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence trace: a failure fixed later counts, and the repository name is case-insensitive", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const fixed = await manifestIn(dir, { fixed: true });
    const body = (await evidenceCommentBody(fixed, { ...input, repo: "O/R" }))
      .body;
    const report = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [trustedComment(body), ...reviews()]),
    );
    assertEquals(report.complete, true, report.problems.join("; "));
    assertEquals(report.evidence[0].failedAssertions, 1);
    assertEquals(report.evidence[0].state, "succeeded");
    // No timestamps (a degraded runner): treated as edited.
    const [bare] = await parseEvidenceComments([{
      body,
      authorAssociation: "MEMBER",
    }]);
    assertEquals(bare.edited, true);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence trace: stray or pasted comments never break a PR that has counting evidence", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const manifest = await manifestIn(dir);
    const atHead = (await evidenceCommentBody(manifest, input)).body;
    const otherPr = (await evidenceCommentBody(manifest, { ...input, pr: 8 }))
      .body;
    const report = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [
        trustedComment(atHead),
        trustedComment(otherPr, { authorAssociation: "NONE" }),
        trustedComment(otherPr),
        ...reviews(),
      ]),
    );
    assertEquals(report.complete, true, report.problems.join("; "));
    assertEquals(report.notes.length, 2);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence trace: evidence that does not count leaves the trace incomplete", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const good = await manifestIn(dir);
    const failed = await manifestIn(dir, { passed: false });
    const cases: Array<[string, Record<string, unknown>]> = [
      [
        "older head",
        trustedComment(
          (await evidenceCommentBody(good, { ...input, commit: OLD })).body,
        ),
      ],
      [
        "outsider",
        trustedComment((await evidenceCommentBody(good, input)).body, {
          authorAssociation: "NONE",
        }),
      ],
      [
        "edited",
        trustedComment((await evidenceCommentBody(good, input)).body, {
          updatedAt: "2026-11-11T09:00:00Z",
        }),
      ],
      [
        "uncommitted",
        trustedComment(
          (await evidenceCommentBody(good, { ...input, uncommitted: true }))
            .body,
        ),
      ],
      [
        "failed run",
        trustedComment((await evidenceCommentBody(failed, input)).body),
      ],
      [
        "failed task",
        trustedComment(
          (await evidenceCommentBody(
            await manifestIn(dir, { status: "failed" }),
            input,
          )).body,
        ),
      ],
      [
        "cancelled task",
        trustedComment(
          (await evidenceCommentBody(
            await manifestIn(dir, { status: "cancelled" }),
            input,
          )).body,
        ),
      ],
      [
        "other repo",
        trustedComment(
          (await evidenceCommentBody(good, { ...input, repo: "fork/r" })).body,
        ),
      ],
      [
        "other PR",
        trustedComment(
          (await evidenceCommentBody(good, { ...input, pr: 8 })).body,
        ),
      ],
    ];
    for (const [label, comment] of cases) {
      const report = await traceEvidence(
        "o/r",
        7,
        fakeGh(MERGED, [comment, ...reviews()]),
      );
      assertEquals(report.complete, false, label);
      assert(report.problems[0].includes("no evidence that counts"), label);
    }
    // Good evidence, but one receipt short; and not merged when required.
    const atHead = trustedComment(
      (await evidenceCommentBody(good, input)).body,
    );
    const oneReview = await traceEvidence(
      "o/r",
      7,
      fakeGh(MERGED, [atHead, trustedComment(receipt("reviewer-a", "m-a"))]),
    );
    assert(oneReview.receipts.error?.includes("distinct reviewers"));
    const open = await traceEvidence(
      "o/r",
      7,
      fakeGh({ ...MERGED, state: "open", merged_at: null }, [
        atHead,
        ...reviews(),
      ]),
      { requireMerged: true },
    );
    assertEquals(open.complete, false);
    assert(open.problems[0].includes("not merged"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh evidence publish: a manifest with ANSI colours survives gh's control-character rewriting", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const state = createTaskState({
      taskId: "ev-1",
      worktree: dir,
      baseCommit: "b".repeat(40),
      portOffset: 0,
      now: () => "2026-11-11T00:00:00.000Z",
    });
    state.operations.push({
      command: "/fake/deno",
      args: ["task", "test:host"],
      displayCommand: "deno task test:host",
      cwd: dir,
      startedAt: "2026-11-11T00:01:00.000Z",
      endedAt: "2026-11-11T00:02:00.000Z",
      exitCode: 0,
      output: "\u001b[32mok\u001b[0m | 1 passed",
    });
    state.assertions.push({
      name: "host-test-exit-code",
      expected: "exit code 0",
      actual: "exit code 0",
      passed: true,
      at: "2026-11-11T00:02:00.000Z",
    });
    writeTaskState(state);
    const manifest = await buildEvidenceManifest(state, {
      os: { name: "testos", version: "1", arch: "x86_64" },
    });
    const { body, binding } = await evidenceCommentBody(manifest, input);
    assertEquals(binding.encoding, "gzip+base64");
    // What `gh api` prints for such a body: \u001b becomes ^[.
    const rewritten = body.replaceAll("\u001b", "^[");
    const [parsed] = await parseEvidenceComments([trustedComment(rewritten)]);
    assertEquals(parsed.ok, true, parsed.problems.join("; "));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
