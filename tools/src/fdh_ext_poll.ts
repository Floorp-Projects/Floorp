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
 *   runs (the latest run of each check name), at the newest completion time
 *   (start time while any run is pending);
 * - a PR that is gone (HTTP 404) as a deletion of `object`, at the poll time.
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
  const match =
    /^(?:https:\/\/github\.com\/|git@github\.com:|ssh:\/\/git@github\.com\/)([A-Za-z0-9_.-]+)\/([A-Za-z0-9_.-]+?)(?:\.git)?\/?$/
      .exec(url.trim());
  if (match === null) {
    throw new Error(`not a GitHub remote: ${JSON.stringify(url)}`);
  }
  return `${match[1]}/${match[2]}`;
}

export function validateRepoSlug(slug: string): string {
  if (!/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(slug)) {
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
  name: string;
  status: string;
  conclusion: string | null;
  started_at: string | null;
  completed_at: string | null;
}

const PASSING = new Set(["success", "neutral", "skipped"]);

/**
 * One result for a head: pending while any check's latest run is not
 * completed, failure if any latest run did not pass, else success. Null when
 * the head has no check runs.
 */
export function aggregateChecks(
  runs: readonly CheckRun[],
): { result: "pending" | "success" | "failure"; version: string } | null {
  const latest = new Map<string, CheckRun>();
  const startOf = (r: CheckRun) => Date.parse(r.started_at ?? "") || 0;
  for (const run of runs) {
    const seen = latest.get(run.name);
    if (seen === undefined || startOf(run) >= startOf(seen)) {
      latest.set(run.name, run);
    }
  }
  if (latest.size === 0) return null;
  const all = [...latest.values()];
  const newest = (times: Array<string | null>) =>
    new Date(Math.max(...times.map((t) => Date.parse(t ?? "") || 0)))
      .toISOString();
  const pending = all.filter((r) => r.status !== "completed");
  if (pending.length > 0) {
    return {
      result: "pending",
      version: newest(pending.map((r) => r.started_at)),
    };
  }
  const failed = all.some((r) => !PASSING.has(r.conclusion ?? ""));
  return {
    result: failed ? "failure" : "success",
    version: newest(all.map((r) => r.completed_at)),
  };
}

function prState(pr: PullRequest): string {
  if (pr.merged_at !== null) return "merged";
  if (pr.state === "closed") return "closed";
  return pr.draft ? "draft" : "open";
}

/** Observations for every linked PR of the ledger (reads GitHub only). */
export async function pollGithub(
  ledger: ExtLedger,
  slug: string,
  gh: GhRunner = defaultGhRunner,
  at = nowIso(),
): Promise<ObservationInput[]> {
  validateRepoSlug(slug);
  const out: ObservationInput[] = [];
  for (const link of Object.values(ledger.links)) {
    if (link.pr === null) continue;
    const ref = `github:pr/${link.pr}`;
    let pr: PullRequest;
    try {
      pr = JSON.parse(
        await gh(["api", `repos/${slug}/pulls/${link.pr}`]),
      ) as PullRequest;
    } catch (error) {
      if (!(error instanceof GhNotFound)) throw error;
      out.push({
        ref,
        field: "object",
        value: null,
        version: at,
        deleted: true,
        at,
      });
      continue;
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
    if (!/^[0-9a-f]{40}$/.test(sha)) continue;
    const lines = await gh([
      "api",
      "--paginate",
      `repos/${slug}/commits/${sha}/check-runs?per_page=100`,
      "--jq",
      ".check_runs[] | {name, status, conclusion, started_at, completed_at}",
    ]);
    const runs = lines.split("\n").filter((l) => l.trim() !== "").map((l) =>
      JSON.parse(l) as CheckRun
    );
    const ci = aggregateChecks(runs);
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
): Promise<PollReport[]> {
  const observations = await pollGithub(
    readExtLedger(worktree, taskId),
    slug,
    gh,
  );
  return await withExtLedger(
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
}
