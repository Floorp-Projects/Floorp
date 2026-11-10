// SPDX-License-Identifier: MPL-2.0

/**
 * GitHub poller for `fdh ext` (D-19, KAN-131): reads each linked PR with
 * `gh api` and feeds its owner fields into the ext ledger as observations.
 * No webhook; run it whenever the agent reconciles (`fdh ext poll`, then
 * `fdh ext plan`). Jira is not polled here: agents read it with Atlassian MCP
 * and pass it in with `fdh ext observe` (fdh holds no Jira credentials).
 *
 * Observed per linked PR:
 * - `pr.url` at the PR's creation time (it never changes);
 * - `pr.state` (draft | open | closed | merged) at the PR's `updated_at`;
 * - `ci.result` `<head sha>:<pending|success|failure>` from the head's check
 *   runs (the latest run of each check per check suite).
 *
 * The CI version is never older than the PR's `updated_at` (a push moves
 * it, so a new head is never stale against the old head's result), and a
 * finished result is 1 ms after its runs' newest completion, so it always
 * orders after a pending reading of the same runs.
 *
 * A 404 is an error, never a deletion: on this endpoint it far more often
 * means a wrong repository, a fork `origin`, a token without access or an
 * issue number. Record a real deletion with `fdh ext observe --deleted`.
 */

import {
  type ExtLedger,
  ingestObservation,
  type IngestResult,
  type ObservationInput,
  readExtLedger,
  withExtLedger,
} from "./fdh_ext.ts";
import { nowIso } from "./fdh_task.ts";

/** Runs `gh` with `args` and returns stdout; throws `GhNotFound` on 404. */
export type GhRunner = (args: string[]) => Promise<string>;

export class GhNotFound extends Error {}

export const defaultGhRunner: GhRunner = async (args) => {
  const output = await new Deno.Command("gh", {
    args,
    stdin: "null",
    stdout: "piped",
    stderr: "piped",
  }).output();
  const stdout = new TextDecoder().decode(output.stdout);
  if (output.success) return stdout;
  const stderr = new TextDecoder().decode(output.stderr).trim();
  if (/HTTP 404/.test(stderr)) throw new GhNotFound(stderr);
  throw new Error(`gh ${args.slice(0, 2).join(" ")}: ${stderr}`);
};

/** `owner/name` from a GitHub remote URL (https or ssh). */
export function repoSlugFromRemote(url: string): string {
  // Credentials in the URL are never echoed.
  const clean = url.trim().replace(/^(https:\/\/)[^@/]*@/, "$1");
  const match =
    /^(?:https:\/\/github\.com\/|git@github\.com:|ssh:\/\/git@github\.com\/)([A-Za-z0-9_.-]+)\/([A-Za-z0-9_.-]+?)(?:\.git)?\/?$/
      .exec(clean);
  if (match === null) {
    throw new Error(`not a GitHub remote: ${JSON.stringify(clean)}`);
  }
  return validateRepoSlug(`${match[1]}/${match[2]}`);
}

export function validateRepoSlug(slug: string): string {
  const parts = slug.split("/");
  if (
    parts.length !== 2 ||
    parts.some((p) => !/^[A-Za-z0-9_.-]+$/.test(p) || p === "." || p === "..")
  ) {
    throw new Error(`--repo must be owner/name, got ${JSON.stringify(slug)}`);
  }
  return slug;
}

interface PullRequest {
  state: string;
  draft: boolean;
  merged_at: string | null;
  updated_at: string;
  created_at: string;
  html_url: string;
  head: { sha: string };
}

export interface CheckRun {
  /** Check run id (increases with each run, also for a queued re-run). */
  id?: number;
  /** Check suite id: one per workflow run (a re-run stays in its suite). */
  suite?: number | null;
  name: string;
  status: string;
  conclusion: string | null;
  started_at: string | null;
  completed_at: string | null;
}

const PASSING = new Set(["success", "neutral", "skipped"]);
/** Waiting for someone (e.g. a workflow approval): not a result yet. */
const WAITING = new Set(["action_required"]);

/**
 * One result for a head: pending while any check's latest run is not
 * completed (or waits for an approval), failure if any latest run did not
 * pass, else success. The latest run of a check is the one with the highest
 * id within its check suite (so a queued re-run counts, and two workflows
 * with a job of the same name do not hide each other). Null when the head
 * has no check runs. `floor` (the PR's `updated_at`) is the lowest version.
 */
export function aggregateChecks(
  runs: readonly CheckRun[],
  floor?: string,
): { result: "pending" | "success" | "failure"; version: string } | null {
  const latest = new Map<string, CheckRun>();
  const order = (r: CheckRun) => r.id ?? (Date.parse(r.started_at ?? "") || 0);
  for (const run of runs) {
    const key = `${run.suite ?? ""}\0${run.name}`;
    const seen = latest.get(key);
    if (seen === undefined || order(run) >= order(seen)) latest.set(key, run);
  }
  if (latest.size === 0) return null;
  const all = [...latest.values()];
  const newest = (times: Array<string | null | undefined>, plus = 0) => {
    const values = times.map((t) => Date.parse(t ?? "")).filter(
      Number.isFinite,
    );
    const top = values.length === 0 ? 0 : Math.max(...values) + plus;
    const low = Date.parse(floor ?? "");
    return new Date(Number.isFinite(low) ? Math.max(top, low + plus) : top)
      .toISOString();
  };
  const pending = all.filter((r) =>
    r.status !== "completed" || WAITING.has(r.conclusion ?? "")
  );
  if (pending.length > 0) {
    return {
      result: "pending",
      version: newest(all.flatMap((r) => [r.started_at, r.completed_at])),
    };
  }
  const failed = all.some((r) => !PASSING.has(r.conclusion ?? ""));
  return {
    result: failed ? "failure" : "success",
    version: newest(all.flatMap((r) => [r.started_at, r.completed_at]), 1),
  };
}

function prState(pr: PullRequest): string {
  if (pr.merged_at !== null) return "merged";
  if (pr.state === "closed") return "closed";
  return pr.draft ? "draft" : "open";
}

export interface PollError {
  ref: string;
  error: string;
}

/**
 * Observations for every linked PR of the ledger (reads GitHub only). A link
 * whose PR cannot be read is reported in `errors`; the others still count.
 */
export async function pollGithub(
  ledger: ExtLedger,
  slug: string,
  gh: GhRunner = defaultGhRunner,
  at = nowIso(),
): Promise<{ observations: ObservationInput[]; errors: PollError[] }> {
  validateRepoSlug(slug);
  const observations: ObservationInput[] = [];
  const errors: PollError[] = [];
  for (const link of Object.values(ledger.links)) {
    if (link.pr === null) continue;
    const ref = `github:pr/${link.pr}`;
    try {
      observations.push(...await pollPr(slug, link.pr, ref, gh, at));
    } catch (error) {
      errors.push({
        ref,
        error: error instanceof GhNotFound
          ? `PR #${link.pr} not found in ${slug} (wrong --repo, a fork origin, no access, or an issue number); not recorded as a deletion`
          : error instanceof Error
          ? error.message
          : String(error),
      });
    }
  }
  return { observations, errors };
}

async function pollPr(
  slug: string,
  number: number,
  ref: string,
  gh: GhRunner,
  at: string,
): Promise<ObservationInput[]> {
  const out: ObservationInput[] = [];
  {
    const pr = JSON.parse(
      await gh(["api", `repos/${slug}/pulls/${number}`]),
    ) as PullRequest;
    if (
      typeof pr?.html_url !== "string" || typeof pr.updated_at !== "string" ||
      typeof pr.created_at !== "string" || typeof pr.head?.sha !== "string"
    ) {
      throw new Error(`unexpected PR response for #${number}`);
    }
    out.push({
      ref,
      field: "pr.url",
      value: pr.html_url,
      version: pr.created_at,
      at,
    });
    out.push({
      ref,
      field: "pr.state",
      value: prState(pr),
      version: pr.updated_at,
      at,
    });
    const sha = pr.head.sha;
    if (!/^[0-9a-f]{40}$/.test(sha)) return out;
    const lines = await gh([
      "api",
      "--paginate",
      `repos/${slug}/commits/${sha}/check-runs?per_page=100`,
      "--jq",
      ".check_runs[] | {id, suite: .check_suite.id, name, status, conclusion, started_at, completed_at}",
    ]);
    const runs = lines.split("\n").filter((l) => l.trim() !== "").map((l) =>
      JSON.parse(l) as CheckRun
    );
    const ci = aggregateChecks(runs, pr.updated_at);
    if (ci !== null) {
      out.push({
        ref,
        field: "ci.result",
        value: `${sha}:${ci.result}`,
        version: ci.version,
        at,
      });
    }
  }
  return out;
}

export interface PollReport {
  ref: string;
  field: string;
  value: string | null;
  result: IngestResult;
}

export interface PollResult {
  observed: PollReport[];
  errors: PollError[];
}

/**
 * Poll GitHub for the task's linked PRs and take the observations into the
 * ledger. GitHub is read before the ledger lock is taken (the lock is held
 * only for the ledger update).
 */
export async function pollAndIngest(
  worktree: string,
  taskId: string,
  slug: string,
  gh: GhRunner = defaultGhRunner,
): Promise<PollResult> {
  const { observations, errors } = await pollGithub(
    readExtLedger(worktree, taskId),
    slug,
    gh,
  );
  const observed = await withExtLedger(
    worktree,
    taskId,
    (ledger) =>
      observations.map((o) => ({
        ref: o.ref,
        field: o.field,
        value: o.value,
        result: ingestObservation(ledger, o),
      })),
  );
  return { observed, errors };
}
