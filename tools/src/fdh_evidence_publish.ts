// SPDX-License-Identifier: MPL-2.0

/**
 * EvidenceManifest on the pull request (D-20, KAN-132).
 *
 * A manifest lives only in the worktree's `_dist/fdh/`. To keep it with the
 * change it describes, fdh renders a PR comment that carries the whole
 * manifest, bound to the task, the PR and the commit it was built at, with
 * the SHA-256 of the manifest text. The agent posts that comment itself
 * (`gh pr comment --body-file`), the same way as the D-14 review receipts. A
 * PR comment is kept with the PR for as long as the repository exists; a CI
 * artifact expires (90 days by default), so it is not used as the record.
 *
 * `traceEvidence` goes the other way: from a (merged) PR it reads the
 * evidence comments and the receipts, checks each manifest against its
 * SHA-256 and the schema, counts only trusted authors (as the review gate
 * does), and says whether the PR's final head has both valid evidence and a
 * valid receipt set.
 *
 * The central append-only store waits for principal authentication (D-18);
 * until then the comment author's GitHub association is the trust anchor.
 */

import { decodeBase64, encodeBase64 } from "@std/encoding/base64";
import { encodeHex } from "@std/encoding/hex";
import type { EvidenceManifest } from "../qa3/evidence/types.ts";
import { validateEvidenceManifest } from "./fdh_evidence.ts";
import {
  parseReceiptComments,
  type ReviewCommentInput,
  selectTrustedReceiptComments,
  TRUSTED_RECEIPT_AUTHOR_ASSOCIATIONS,
  validateReceiptSet,
} from "./fdh_review.ts";
import {
  defaultGhRunner,
  type GhRunner,
  validateRepoSlug,
} from "./fdh_ext_poll.ts";

export const FDH_EVIDENCE_MARKER = "fdh-evidence:v1";
/** GitHub's comment limit is 65 536 characters; keep a margin. */
export const MAX_EVIDENCE_COMMENT_CHARS = 60_000;

const GZIP_FENCE = "text fdh-evidence-gzip-base64";
const MARKER_RE = /<!--\s*fdh-evidence:v1\s+(\{[\s\S]*?\})\s*-->/;
const JSON_BLOCK_RE = /```json\n([\s\S]*?)\n```/;
const GZIP_BLOCK_RE =
  /```text fdh-evidence-gzip-base64\n([A-Za-z0-9+/=\n]*?)\n```/;
const FULL_COMMIT_RE = /^[0-9a-f]{40}(?:[0-9a-f]{24})?$/;

/** What an evidence comment is bound to (the marker's JSON). */
export interface EvidenceBinding {
  schemaVersion: 1;
  taskId: string;
  pr: number;
  /** Full commit SHA of the worktree HEAD when the manifest was built. */
  commit: string;
  /** SHA-256 (hex) of the manifest text (pretty JSON, LF line ends). */
  sha256: string;
  bytes: number;
  encoding: "json" | "gzip+base64";
  /** The worktree had uncommitted changes (not hashed; see limitations). */
  uncommitted: boolean;
  builtAt: string;
}

export interface EvidenceCommentInput {
  taskId: string;
  pr: number;
  commit: string;
  uncommitted: boolean;
  builtAt: string;
}

async function sha256Hex(text: string): Promise<string> {
  const bytes = new TextEncoder().encode(text);
  return encodeHex(
    new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
  );
}

async function gzip(text: string): Promise<Uint8Array> {
  const stream = new Blob([new TextEncoder().encode(text)]).stream()
    .pipeThrough(new CompressionStream("gzip"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

async function gunzip(bytes: Uint8Array): Promise<string> {
  const stream = new Blob([new Uint8Array(bytes)]).stream()
    .pipeThrough(new DecompressionStream("gzip"));
  return await new Response(stream).text();
}

/** The manifest text that is hashed and embedded. */
export function manifestText(manifest: EvidenceManifest): string {
  return `${JSON.stringify(manifest, null, 2)}\n`;
}

function summaryLines(
  manifest: EvidenceManifest,
  binding: EvidenceBinding,
): string[] {
  const passed = manifest.assertions.filter((a) => a.passed).length;
  const lines = [
    `**fdh evidence** for task \`${binding.taskId}\` at \`${binding.commit}\`` +
    (binding.uncommitted ? " (with uncommitted changes, not hashed)" : ""),
    "",
    `- State: \`${manifest.envelope.taskStatus.state}\``,
    `- Assertions: ${passed} passed, ${
      manifest.assertions.length - passed
    } failed`,
  ];
  for (const a of manifest.assertions) {
    lines.push(`  - ${a.passed ? "passed" : "FAILED"}: \`${a.name}\``);
  }
  lines.push(
    `- Limitations: ${manifest.limitations.length} (in the manifest below)`,
    `- Manifest SHA-256: \`${binding.sha256}\` (${binding.bytes} bytes)`,
    "",
  );
  return lines;
}

/**
 * Render the PR comment for `manifest`. The manifest is embedded as JSON, or
 * gzip+base64 when the JSON would not fit in one comment; a manifest that
 * does not fit either way is refused.
 */
export async function evidenceCommentBody(
  manifest: EvidenceManifest,
  input: EvidenceCommentInput,
): Promise<{ body: string; binding: EvidenceBinding }> {
  if (!FULL_COMMIT_RE.test(input.commit)) {
    throw new Error(
      `commit must be a full hex SHA, got ${JSON.stringify(input.commit)}`,
    );
  }
  if (!Number.isSafeInteger(input.pr) || input.pr <= 0) {
    throw new Error(`pr must be a positive integer, got ${input.pr}`);
  }
  const text = manifestText(manifest);
  const base = {
    schemaVersion: 1 as const,
    taskId: input.taskId,
    pr: input.pr,
    commit: input.commit,
    sha256: await sha256Hex(text),
    bytes: new TextEncoder().encode(text).length,
    uncommitted: input.uncommitted,
    builtAt: input.builtAt,
  };
  const render = (binding: EvidenceBinding, block: string) =>
    [
      `<!-- ${FDH_EVIDENCE_MARKER} ${JSON.stringify(binding)} -->`,
      ...summaryLines(manifest, binding),
      "<details><summary>EvidenceManifest</summary>",
      "",
      block,
      "",
      "</details>",
      "",
    ].join("\n");
  const json: EvidenceBinding = { ...base, encoding: "json" };
  const plain = render(json, `\`\`\`json\n${text.trimEnd()}\n\`\`\``);
  if (plain.length <= MAX_EVIDENCE_COMMENT_CHARS) {
    return { body: plain, binding: json };
  }
  const packed: EvidenceBinding = { ...base, encoding: "gzip+base64" };
  const b64 = encodeBase64(await gzip(text)).replace(/(.{76})/g, "$1\n")
    .trimEnd();
  const compressed = render(packed, `\`\`\`${GZIP_FENCE}\n${b64}\n\`\`\``);
  if (compressed.length > MAX_EVIDENCE_COMMENT_CHARS) {
    throw new Error(
      `the manifest (${base.bytes} bytes) does not fit in one PR comment, even compressed`,
    );
  }
  return { body: compressed, binding: packed };
}

export interface ParsedEvidence {
  binding: EvidenceBinding | null;
  author: string;
  authorAssociation: string;
  trusted: boolean;
  url: string | null;
  /** The manifest decoded, its SHA-256 matched and the schema accepted it. */
  ok: boolean;
  problems: string[];
  state: string | null;
}

export interface EvidenceCommentSource extends ReviewCommentInput {
  url?: string | null;
}

function isBinding(value: unknown): value is EvidenceBinding {
  if (value === null || typeof value !== "object") return false;
  const v = value as Record<string, unknown>;
  return v.schemaVersion === 1 && typeof v.taskId === "string" &&
    Number.isSafeInteger(v.pr) && typeof v.commit === "string" &&
    FULL_COMMIT_RE.test(v.commit) && typeof v.sha256 === "string" &&
    /^[0-9a-f]{64}$/.test(v.sha256) && Number.isSafeInteger(v.bytes) &&
    (v.encoding === "json" || v.encoding === "gzip+base64") &&
    typeof v.uncommitted === "boolean" && typeof v.builtAt === "string";
}

/** Read every evidence comment; a broken one is reported, not dropped. */
export async function parseEvidenceComments(
  comments: readonly EvidenceCommentSource[],
  trustedAssociations: readonly string[] = TRUSTED_RECEIPT_AUTHOR_ASSOCIATIONS,
): Promise<ParsedEvidence[]> {
  const out: ParsedEvidence[] = [];
  for (const comment of comments) {
    const body = (comment.body ?? "").replaceAll("\r\n", "\n");
    if (!body.includes(FDH_EVIDENCE_MARKER)) continue;
    const association = (comment.authorAssociation ?? "").toUpperCase();
    const entry: ParsedEvidence = {
      binding: null,
      author: comment.author ?? "(unknown)",
      authorAssociation: association || "(missing)",
      trusted: trustedAssociations.includes(association),
      url: comment.url ?? null,
      ok: false,
      problems: [],
      state: null,
    };
    out.push(entry);
    const marker = MARKER_RE.exec(body);
    let parsed: unknown;
    try {
      parsed = marker === null ? null : JSON.parse(marker[1]);
    } catch {
      parsed = null;
    }
    if (!isBinding(parsed)) {
      entry.problems.push("the evidence marker is missing or malformed");
      continue;
    }
    entry.binding = parsed;
    let text: string | null = null;
    if (parsed.encoding === "json") {
      const block = JSON_BLOCK_RE.exec(body);
      text = block === null ? null : `${block[1]}\n`;
    } else {
      const block = GZIP_BLOCK_RE.exec(body);
      if (block !== null) {
        try {
          text = await gunzip(decodeBase64(block[1].replaceAll("\n", "")));
        } catch {
          entry.problems.push("the compressed manifest cannot be decoded");
          continue;
        }
      }
    }
    if (text === null) {
      entry.problems.push("the manifest block is missing");
      continue;
    }
    if ((await sha256Hex(text)) !== parsed.sha256) {
      entry.problems.push("the manifest does not match its SHA-256 (edited?)");
      continue;
    }
    let manifest: EvidenceManifest;
    try {
      manifest = JSON.parse(text) as EvidenceManifest;
    } catch {
      entry.problems.push("the manifest is not valid JSON");
      continue;
    }
    const errors = validateEvidenceManifest(manifest);
    if (errors.length > 0) {
      entry.problems.push(
        ...errors.map((e) => `schema: ${e.code} ${e.path}: ${e.message}`),
      );
      continue;
    }
    entry.state = manifest.envelope?.taskStatus?.state ?? null;
    entry.ok = true;
  }
  return out;
}

export interface TraceReport {
  repo: string;
  pr: number;
  state: string;
  merged: boolean;
  mergeCommit: string | null;
  /** The PR's head when it was read (for a merged PR: the merged head). */
  headSha: string;
  evidence: Array<ParsedEvidence & { atHead: boolean }>;
  receipts: {
    atHead: number;
    reviewers: string[];
    models: string[];
    stale: number;
    ignored: number;
    error: string | null;
  };
  /** Trusted, valid evidence at the head, and a valid receipt set there. */
  complete: boolean;
  problems: string[];
}

interface PullFacts {
  state: string;
  merged_at: string | null;
  merge_commit_sha: string | null;
  head: { sha: string };
}

/** From a PR to its evidence and receipts (reads GitHub with `gh api`). */
export async function traceEvidence(
  slug: string,
  pr: number,
  gh: GhRunner = defaultGhRunner,
): Promise<TraceReport> {
  validateRepoSlug(slug);
  if (!Number.isSafeInteger(pr) || pr <= 0) {
    throw new Error(`--pr must be a positive integer, got ${pr}`);
  }
  const facts = JSON.parse(
    await gh(["api", `repos/${slug}/pulls/${pr}`]),
  ) as PullFacts;
  if (typeof facts?.head?.sha !== "string") {
    throw new Error(`unexpected PR response for #${pr}`);
  }
  const lines = await gh([
    "api",
    "--paginate",
    `repos/${slug}/issues/${pr}/comments?per_page=100`,
    "--jq",
    ".[] | {body, author: .user.login, authorAssociation: .author_association, url: .html_url}",
  ]);
  const comments = lines.split("\n").filter((l) => l.trim() !== "").map((l) =>
    JSON.parse(l) as EvidenceCommentSource
  );
  const headSha = facts.head.sha.toLowerCase();
  const evidence = (await parseEvidenceComments(comments)).map((e) => ({
    ...e,
    atHead: e.binding?.commit === headSha,
  }));
  const { trusted, ignored } = selectTrustedReceiptComments(comments);
  const receipts: TraceReport["receipts"] = {
    atHead: 0,
    reviewers: [],
    models: [],
    stale: 0,
    ignored: ignored.length,
    error: null,
  };
  try {
    const summary = validateReceiptSet(parseReceiptComments(trusted), headSha);
    receipts.atHead = summary.receipts.length;
    receipts.reviewers = summary.reviewers;
    receipts.models = summary.models;
    receipts.stale = summary.staleReceipts;
  } catch (error) {
    receipts.error = error instanceof Error ? error.message : String(error);
  }
  const problems: string[] = [];
  const good = evidence.filter((e) =>
    e.ok && e.trusted && e.atHead && e.binding?.pr === pr
  );
  if (good.length === 0) {
    problems.push(
      `no valid evidence comment by a trusted author at head ${headSha}`,
    );
  }
  for (const e of evidence) {
    if (e.ok && e.binding !== null && e.binding.pr !== pr) {
      problems.push(`an evidence comment is bound to PR #${e.binding.pr}`);
    }
  }
  if (receipts.error !== null) problems.push(receipts.error);
  return {
    repo: slug,
    pr,
    state: facts.state,
    merged: facts.merged_at !== null,
    mergeCommit: facts.merge_commit_sha,
    headSha,
    evidence,
    receipts,
    complete: problems.length === 0,
    problems,
  };
}
