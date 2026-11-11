// SPDX-License-Identifier: MPL-2.0

/**
 * EvidenceManifest on the pull request (D-20, KAN-132).
 *
 * A manifest lives only in the worktree's `_dist/fdh/`. To keep it with the
 * change it describes, fdh renders a PR comment that carries the whole
 * manifest, bound to the repository, the task, the PR and the commit it was
 * built at, with the SHA-256 of the manifest text. The agent posts that
 * comment itself (`gh pr comment --body-file`), the same way as the D-14
 * review receipts. A PR comment is kept with the PR for as long as the
 * repository exists; a CI artifact expires (90 days by default), so it is
 * not used as the record.
 *
 * `traceEvidence` goes the other way: from a (merged) PR it reads the
 * evidence comments and the receipts and says whether the PR's final head
 * has evidence that counts and a valid receipt set.
 *
 * Trust model (until principal authentication, D-18, and a central
 * append-only store): only comments by OWNER, MEMBER or COLLABORATOR count,
 * as for receipts; the binding (repository, task, PR, commit) is what that
 * author's comment says. The SHA-256 is not a signature: it catches a
 * manifest that no longer matches its marker, not an author who rewrites
 * both. An edited evidence comment (updated after it was posted) therefore
 * does not count.
 */

import * as path from "@std/path";
import { decodeBase64, encodeBase64 } from "@std/encoding/base64";
import { encodeHex } from "@std/encoding/hex";
import type { EvidenceManifest } from "../qa3/evidence/types.ts";
import { evidenceIds, validateEvidenceManifest } from "./fdh_evidence.ts";
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
/** Largest manifest a comment may claim (bounds gzip expansion). */
export const MAX_EVIDENCE_MANIFEST_BYTES = 4 * 1024 * 1024;

const DETAILS_HEAD = "<details><summary>EvidenceManifest</summary>";
const GZIP_FENCE = "text fdh-evidence-gzip-base64";
const MARKER_OPEN = `<!-- ${FDH_EVIDENCE_MARKER} `;
const FULL_COMMIT_RE = /^[0-9a-f]{40}(?:[0-9a-f]{24})?$/;

/** What an evidence comment is bound to (the marker's JSON). */
export interface EvidenceBinding {
  schemaVersion: 1;
  /** `owner/name` of the repository the PR belongs to. */
  repo: string;
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
  repo: string;
  taskId: string;
  pr: number;
  commit: string;
  uncommitted: boolean;
  builtAt: string;
}

async function sha256Hex(bytes: Uint8Array): Promise<string> {
  return encodeHex(
    new Uint8Array(
      await crypto.subtle.digest("SHA-256", new Uint8Array(bytes)),
    ),
  );
}

async function gzip(bytes: Uint8Array): Promise<Uint8Array> {
  const stream = new Blob([new Uint8Array(bytes)]).stream()
    .pipeThrough(new CompressionStream("gzip"));
  return new Uint8Array(await new Response(stream).arrayBuffer());
}

/** Decompress, giving up as soon as more than `limit` bytes come out. */
async function gunzipAtMost(
  bytes: Uint8Array,
  limit: number,
): Promise<Uint8Array | null> {
  const reader = new Blob([new Uint8Array(bytes)]).stream()
    .pipeThrough(new DecompressionStream("gzip")).getReader();
  const chunks: Uint8Array[] = [];
  let total = 0;
  for (;;) {
    const { done, value } = await reader.read();
    if (done) break;
    total += value.length;
    if (total > limit) {
      await reader.cancel();
      return null;
    }
    chunks.push(value);
  }
  const out = new Uint8Array(total);
  let at = 0;
  for (const chunk of chunks) {
    out.set(chunk, at);
    at += chunk.length;
  }
  return out;
}

/** The manifest text that is hashed and embedded. */
export function manifestText(manifest: EvidenceManifest): string {
  return `${JSON.stringify(manifest, null, 2)}\n`;
}

/** Directories under the fdh root that hold fdh's own state. */
export const EVIDENCE_OUT_RESERVED: readonly string[] = [
  "tasks",
  "ext",
  "jobs",
  "quota",
  "reviews",
  "observations",
];

/**
 * Resolve `--out` (relative to `worktree` unless absolute): it must lie
 * under `root` (the worktree's _dist/fdh) and outside fdh's own state.
 *
 * Checked twice. Lexically, with each path segment compared the way Windows
 * opens it (case-insensitive, trailing dots and spaces dropped), and with
 * `~` (8.3 short names) and `:` (alternate data streams) refused. Then on
 * disk: the path, or its nearest existing ancestor, is resolved through
 * symlinks, junctions and short names, and must still lie under `root` and
 * outside the reserved directories.
 */
export function resolveEvidenceOut(
  root: string,
  worktree: string,
  raw: string,
): string {
  const base = path.resolve(root);
  const resolved = path.isAbsolute(raw)
    ? path.resolve(raw)
    : path.resolve(worktree, raw);
  const outside = () =>
    new Error(
      `--out must be inside ${base} (the fdh evidence root), got ${resolved}`,
    );
  const reserved = (top: string) =>
    new Error(
      `--out must not write into fdh's own state under ${
        path.join(base, top)
      }, got ${resolved}`,
    );
  const relative = path.relative(base, resolved);
  if (escapes(relative)) throw outside();
  const segments = relative.split(/[\\/]/);
  if (segments.some((s) => s.includes("~") || s.includes(":"))) {
    throw new Error(
      `--out must not use short names (~) or streams (:), got ${resolved}`,
    );
  }
  const top = windowsName(segments[0]);
  if (EVIDENCE_OUT_RESERVED.includes(top)) throw reserved(top);

  // On disk: follow symlinks, junctions and short names. Nothing under a
  // root that does not exist yet can be a link (and the root's own path may
  // differ from its real path only once it exists).
  if (!pathExists(base)) return resolved;
  const realBase = realPathOrSelf(base);
  // Start at the path itself: an existing file there may be a link too.
  let ancestor = resolved;
  while (!pathExists(ancestor) && path.dirname(ancestor) !== ancestor) {
    ancestor = path.dirname(ancestor);
  }
  const realRelative = path.relative(realBase, realPathOrSelf(ancestor));
  if (realRelative !== "") {
    if (escapes(realRelative)) throw outside();
    const realTop = windowsName(realRelative.split(/[\\/]/)[0]);
    if (EVIDENCE_OUT_RESERVED.includes(realTop)) throw reserved(realTop);
  }
  return resolved;
}

/** A path segment as Windows matches it: case-insensitive, no trailing dots or spaces. */
function windowsName(segment: string): string {
  return segment.replace(/[. ]+$/, "").toLowerCase();
}

function escapes(relative: string): boolean {
  return relative === "" || relative === ".." ||
    relative.startsWith(`..${path.SEPARATOR}`) || relative.startsWith("../") ||
    path.isAbsolute(relative);
}

function pathExists(p: string): boolean {
  try {
    Deno.lstatSync(p);
    return true;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false;
    throw error;
  }
}

function realPathOrSelf(p: string): string {
  try {
    return Deno.realPathSync(p);
  } catch {
    return p;
  }
}

/**
 * Refuse to overwrite either half of an `evidence publish` output pair
 * (`<x>` and `<x>.manifest.json`), which `fdh evidence --out` would
 * otherwise do: the pair would no longer match.
 */
export function assertNotPublishOutput(outPath: string): void {
  const suffix = ".manifest.json";
  const pairedWith = pathExists(`${outPath}${suffix}`)
    ? `${outPath}${suffix}`
    : outPath.endsWith(suffix) &&
        pathExists(outPath.slice(0, -suffix.length))
    ? outPath.slice(0, -suffix.length)
    : undefined;
  if (pairedWith !== undefined) {
    throw new Error(
      `--out ${outPath} is one half of an evidence publish output (with ${pairedWith}); use another path`,
    );
  }
}

/**
 * Write the plain review copy (`<outPath>.manifest.json`, exactly the bytes
 * the binding's SHA-256 covers) and then the comment body. Both are created
 * new: a second publication to the same `--out` fails rather than pairing
 * one run's comment with another run's review copy. Returns the review path.
 */
export async function writeEvidenceOutputs(
  outPath: string,
  manifestJson: string,
  body: string,
): Promise<string> {
  const reviewPath = `${outPath}.manifest.json`;
  await Deno.mkdir(path.dirname(outPath), { recursive: true });
  for (const p of [outPath, reviewPath]) {
    if (pathExists(p)) {
      throw new Error(`${p} already exists; publish to a new --out`);
    }
  }
  try {
    await Deno.writeTextFile(reviewPath, manifestJson, { createNew: true });
  } catch (error) {
    if (error instanceof Deno.errors.AlreadyExists) {
      throw new Error(`${reviewPath} already exists; publish to a new --out`);
    }
    throw error;
  }
  try {
    await Deno.writeTextFile(outPath, body, { createNew: true });
  } catch (error) {
    await Deno.remove(reviewPath).catch(() => {});
    if (error instanceof Deno.errors.AlreadyExists) {
      throw new Error(`${outPath} already exists; publish to a new --out`);
    }
    throw error;
  }
  return reviewPath;
}

/** Inline code safe for any assertion name (no backticks, no newlines). */
function inlineName(name: string): string {
  return `\`${name.replace(/[`\r\n]/g, " ")}\``;
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
    lines.push(`  - ${a.passed ? "passed" : "FAILED"}: ${inlineName(a.name)}`);
  }
  lines.push(
    `- Limitations: ${manifest.limitations.length} (in the manifest below)`,
    `- Manifest SHA-256: \`${binding.sha256}\` (${binding.bytes} bytes)`,
    "",
  );
  return lines;
}

/**
 * Render the PR comment for `manifest`. The manifest is embedded as
 * gzip+base64 (see below); one that does not fit in a comment is refused.
 * The rendered comment is read back before it is returned.
 */
export async function evidenceCommentBody(
  manifest: EvidenceManifest,
  input: EvidenceCommentInput,
): Promise<{ body: string; binding: EvidenceBinding }> {
  validateRepoSlug(input.repo);
  if (!FULL_COMMIT_RE.test(input.commit)) {
    throw new Error(
      `commit must be a full hex SHA, got ${JSON.stringify(input.commit)}`,
    );
  }
  if (!Number.isSafeInteger(input.pr) || input.pr <= 0) {
    throw new Error(`pr must be a positive integer, got ${input.pr}`);
  }
  const text = manifestText(manifest);
  const bytes = new TextEncoder().encode(text);
  if (bytes.length > MAX_EVIDENCE_MANIFEST_BYTES) {
    throw new Error(
      `the manifest (${bytes.length} bytes) is larger than ${MAX_EVIDENCE_MANIFEST_BYTES} bytes`,
    );
  }
  const base = {
    schemaVersion: 1 as const,
    repo: input.repo,
    taskId: input.taskId,
    pr: input.pr,
    commit: input.commit,
    sha256: await sha256Hex(bytes),
    bytes: bytes.length,
    uncommitted: input.uncommitted,
    builtAt: input.builtAt,
  };
  const render = (binding: EvidenceBinding, block: string) =>
    [
      `${MARKER_OPEN}${JSON.stringify(binding)} -->`,
      ...summaryLines(manifest, binding),
      DETAILS_HEAD,
      "",
      block,
      "",
      "</details>",
      "",
    ].join("\n");
  // Always gzip+base64. `gh api` rewrites JSON-escaped control characters
  // (\u001b from ANSI colours, and likely \b and \f) to caret notation when
  // it prints a response, so a manifest embedded as JSON may not read back
  // through gh; base64 has no backslashes for it to rewrite. The summary
  // above the block stays readable. (The reader still accepts "json".)
  const packed: EvidenceBinding = { ...base, encoding: "gzip+base64" };
  const b64 = encodeBase64(await gzip(bytes)).replace(/(.{76})/g, "$1\n")
    .trimEnd();
  const compressed = render(packed, `\`\`\`${GZIP_FENCE}\n${b64}\n\`\`\``);
  if (compressed.length > MAX_EVIDENCE_COMMENT_CHARS) {
    throw new Error(
      `the manifest (${base.bytes} bytes) does not fit in one PR comment, even compressed`,
    );
  }
  const rendered = { body: compressed, binding: packed };
  const [check] = await parseEvidenceComments([{
    body: rendered.body,
    authorAssociation: "OWNER",
  }]);
  if (!check?.ok) {
    throw new Error(
      `the rendered evidence comment does not read back: ${
        check?.problems.join("; ") ?? "no marker"
      }`,
    );
  }
  return rendered;
}

export interface ParsedEvidence {
  binding: EvidenceBinding | null;
  author: string;
  authorAssociation: string;
  trusted: boolean;
  url: string | null;
  /** The comment was changed after it was posted. */
  edited: boolean;
  /** The manifest decoded, matched its SHA-256 and size, the schema accepted it, and it names the bound task. */
  ok: boolean;
  problems: string[];
  state: string | null;
  assertions: number;
  failedAssertions: number;
}

export interface EvidenceCommentSource extends ReviewCommentInput {
  url?: string | null;
  createdAt?: string | null;
  updatedAt?: string | null;
}

function isBinding(value: unknown): value is EvidenceBinding {
  if (value === null || typeof value !== "object") return false;
  const v = value as Record<string, unknown>;
  return v.schemaVersion === 1 && typeof v.repo === "string" &&
    typeof v.taskId === "string" && Number.isSafeInteger(v.pr) &&
    typeof v.commit === "string" && FULL_COMMIT_RE.test(v.commit) &&
    typeof v.sha256 === "string" && /^[0-9a-f]{64}$/.test(v.sha256) &&
    Number.isSafeInteger(v.bytes) && (v.bytes as number) > 0 &&
    (v.bytes as number) <= MAX_EVIDENCE_MANIFEST_BYTES &&
    (v.encoding === "json" || v.encoding === "gzip+base64") &&
    typeof v.uncommitted === "boolean" && typeof v.builtAt === "string";
}

function readMarker(body: string): unknown {
  const at = body.indexOf(MARKER_OPEN);
  if (at < 0) return null;
  const end = body.indexOf("-->", at + MARKER_OPEN.length);
  if (end < 0) return null;
  try {
    return JSON.parse(body.slice(at + MARKER_OPEN.length, end).trim());
  } catch {
    return null;
  }
}

/** The fenced block right after the manifest's own details heading. */
function manifestBlock(body: string, encoding: EvidenceBinding["encoding"]) {
  const at = body.indexOf(DETAILS_HEAD);
  if (at < 0) return null;
  const rest = body.slice(at + DETAILS_HEAD.length);
  const re = encoding === "json"
    ? /^\n\n```json\n([\s\S]*?)\n```\n/
    : /^\n\n```text fdh-evidence-gzip-base64\n([A-Za-z0-9+/=\n]*?)\n```\n/;
  return re.exec(rest)?.[1] ?? null;
}

/**
 * Read every evidence comment; a broken one is reported, not dropped. The
 * manifest of a comment by an untrusted author is not decoded.
 */
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
      // Missing timestamps count as edited (the API always gives them).
      edited: typeof comment.createdAt !== "string" ||
        typeof comment.updatedAt !== "string" ||
        comment.createdAt !== comment.updatedAt,
      ok: false,
      problems: [],
      state: null,
      assertions: 0,
      failedAssertions: 0,
    };
    out.push(entry);
    const parsed = readMarker(body);
    if (!isBinding(parsed)) {
      entry.problems.push("the evidence marker is missing or malformed");
      continue;
    }
    entry.binding = parsed;
    if (!entry.trusted) {
      entry.problems.push(
        `not counted: ${entry.author} (${entry.authorAssociation}) is not OWNER, MEMBER or COLLABORATOR`,
      );
      continue;
    }
    const block = manifestBlock(body, parsed.encoding);
    if (block === null) {
      entry.problems.push("the manifest block is missing");
      continue;
    }
    let bytes: Uint8Array | null;
    if (parsed.encoding === "json") {
      bytes = new TextEncoder().encode(`${block}\n`);
    } else {
      try {
        bytes = await gunzipAtMost(
          decodeBase64(block.replaceAll("\n", "")),
          parsed.bytes,
        );
      } catch {
        entry.problems.push("the compressed manifest cannot be decoded");
        continue;
      }
      if (bytes === null) {
        entry.problems.push(
          `the compressed manifest is larger than the ${parsed.bytes} bytes its marker claims`,
        );
        continue;
      }
    }
    if (bytes.length !== parsed.bytes) {
      entry.problems.push(
        `the manifest is ${bytes.length} bytes, its marker says ${parsed.bytes}`,
      );
      continue;
    }
    if ((await sha256Hex(bytes)) !== parsed.sha256) {
      entry.problems.push("the manifest does not match its SHA-256 (edited?)");
      continue;
    }
    let manifest: EvidenceManifest;
    try {
      manifest = JSON.parse(
        new TextDecoder().decode(bytes),
      ) as EvidenceManifest;
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
    const taskEntity = evidenceIds(parsed.taskId).task;
    if (!manifest.envelope.entities.some((e) => e.id === taskEntity)) {
      entry.problems.push(
        `the manifest is not for task ${parsed.taskId} (no ${taskEntity})`,
      );
      continue;
    }
    entry.state = manifest.envelope.taskStatus.state;
    entry.assertions = manifest.assertions.length;
    entry.failedAssertions = manifest.assertions.filter((a) => !a.passed)
      .length;
    entry.ok = true;
  }
  return out;
}

export interface TracedEvidence extends ParsedEvidence {
  atHead: boolean;
  /** Counts for `complete` (see `traceEvidence`). */
  counts: boolean;
}

export interface TraceReport {
  repo: string;
  pr: number;
  state: string;
  merged: boolean;
  mergeCommit: string | null;
  /** The PR's head when it was read (for a merged PR: the merged head). */
  headSha: string;
  evidence: TracedEvidence[];
  receipts: {
    atHead: number;
    reviewers: string[];
    models: string[];
    stale: number;
    ignored: number;
    error: string | null;
  };
  /**
   * Evidence that counts at the final head and a valid receipt set there
   * (and, with `requireMerged`, the PR is merged).
   */
  complete: boolean;
  problems: string[];
  /** Things worth a look that do not make the trace incomplete. */
  notes: string[];
}

interface PullFacts {
  state: string;
  merged_at: string | null;
  merge_commit_sha: string | null;
  head: { sha: string };
}

export interface TraceOptions {
  requireMerged?: boolean;
}

/** GitHub repository names are case-insensitive. */
function sameRepo(a: string, b: string): boolean {
  return a.toLowerCase() === b.toLowerCase();
}

/**
 * From a PR to its evidence and receipts (reads GitHub with `gh api`).
 * Evidence counts when it is by a trusted author, not edited, decodes and
 * matches, is bound to this repository, this PR and its final head, was
 * built without uncommitted changes, and the manifest's own verdict is
 * `succeeded` with at least one assertion. That verdict takes the latest
 * assertion of each name (a failure fixed later, the Gate 3 error-then-fix
 * loop, still succeeds) and a failed or cancelled task never succeeds.
 * Comments that do not count are reported, and never make a PR with counting
 * evidence incomplete.
 */
export async function traceEvidence(
  slug: string,
  pr: number,
  gh: GhRunner = defaultGhRunner,
  options: TraceOptions = {},
): Promise<TraceReport> {
  validateRepoSlug(slug);
  if (!Number.isSafeInteger(pr) || pr <= 0) {
    throw new Error(`--pr must be a positive integer, got ${pr}`);
  }
  const facts = JSON.parse(
    await gh(["api", `repos/${slug}/pulls/${pr}`]),
  ) as PullFacts;
  if (
    typeof facts?.head?.sha !== "string" || typeof facts.state !== "string" ||
    (facts.merged_at !== null && typeof facts.merged_at !== "string")
  ) {
    throw new Error(`unexpected PR response for #${pr}`);
  }
  const lines = await gh([
    "api",
    "--paginate",
    `repos/${slug}/issues/${pr}/comments?per_page=100`,
    "--jq",
    ".[] | {body, author: .user.login, authorAssociation: .author_association, url: .html_url, createdAt: .created_at, updatedAt: .updated_at}",
  ]);
  const comments = lines.split("\n").filter((l) => l.trim() !== "").map((l) =>
    JSON.parse(l) as EvidenceCommentSource
  );
  const headSha = facts.head.sha.toLowerCase();
  const evidence: TracedEvidence[] = (await parseEvidenceComments(comments))
    .map((e) => {
      const b = e.binding;
      const atHead = b?.commit === headSha;
      const counts = e.ok && e.trusted && !e.edited && b !== null &&
        sameRepo(b.repo, slug) && b.pr === pr && atHead && !b.uncommitted &&
        e.state === "succeeded" && e.assertions > 0;
      return { ...e, atHead, counts };
    });
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
  const notes: string[] = [];
  if (!evidence.some((e) => e.counts)) {
    problems.push(`no evidence that counts at head ${headSha}`);
  }
  for (const e of evidence.filter((x) => !x.counts)) {
    const b = e.binding;
    const why = [
      ...e.problems,
      ...(e.edited ? ["edited after it was posted"] : []),
      ...(b !== null && !sameRepo(b.repo, slug) ? [`bound to ${b.repo}`] : []),
      ...(b !== null && b.pr !== pr ? [`bound to PR #${b.pr}`] : []),
      ...(b !== null && !e.atHead
        ? [`bound to ${b.commit}, not the head`]
        : []),
      ...(b?.uncommitted ? ["built with uncommitted changes"] : []),
      ...(e.ok && e.assertions === 0 ? ["records no assertion"] : []),
      ...(e.ok && e.state !== "succeeded"
        ? [`the manifest's state is ${e.state}`]
        : []),
    ];
    notes.push(
      `not counted (${e.author}${e.url ? `, ${e.url}` : ""}): ${
        why.join("; ") || "unknown"
      }`,
    );
  }
  if (receipts.error !== null) problems.push(receipts.error);
  const merged = facts.merged_at !== null;
  if (options.requireMerged && !merged) {
    problems.push(`PR #${pr} is not merged (${facts.state})`);
  }
  return {
    repo: slug,
    pr,
    state: facts.state,
    merged,
    mergeCommit: facts.merge_commit_sha,
    headSha,
    evidence,
    receipts,
    complete: problems.length === 0,
    problems,
    notes,
  };
}
