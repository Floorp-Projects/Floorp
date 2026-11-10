// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects, assertThrows } from "@std/assert";
import { readExtLedger, upsertLink, withExtLedger } from "./fdh_ext.ts";
import {
  aggregateChecks,
  type CheckRun,
  GhNotFound,
  type GhRunner,
  pollAndIngest,
  repoSlugFromRemote,
} from "./fdh_ext_poll.ts";
import { createTaskState, writeTaskState } from "./fdh_task.ts";

const SHA = "025ec7679c2b0d8b15956c44a92fafe122f15334";
const run = (
  name: string,
  status: string,
  conclusion: string | null,
  started: string,
  completed: string | null,
): CheckRun => ({
  name,
  status,
  conclusion,
  started_at: started,
  completed_at: completed,
});

Deno.test("fdh ext poll: GitHub remotes give owner/name", () => {
  assertEquals(
    repoSlugFromRemote("https://github.com/Floorp-Projects/Floorp"),
    "Floorp-Projects/Floorp",
  );
  assertEquals(repoSlugFromRemote("git@github.com:o/r.git"), "o/r");
  assertEquals(repoSlugFromRemote("ssh://git@github.com/o/r.git\n"), "o/r");
  assertThrows(
    () => repoSlugFromRemote("https://gitlab.com/o/r"),
    Error,
    "not a GitHub remote",
  );
});

Deno.test("fdh ext poll: checks aggregate over the latest run of each check", () => {
  const t = (s: number) =>
    new Date(Date.UTC(2026, 10, 11, 9, 0, s)).toISOString();
  assertEquals(aggregateChecks([]), null);
  // A failed run re-run to success counts as success.
  assertEquals(
    aggregateChecks([
      run("ci", "completed", "failure", t(0), t(10)),
      run("ci", "completed", "success", t(20), t(30)),
      run("lint", "completed", "skipped", t(1), t(2)),
    ]),
    { result: "success", version: t(30) },
  );
  assertEquals(
    aggregateChecks([
      run("ci", "completed", "success", t(0), t(10)),
      run("e2e", "in_progress", null, t(15), null),
    ]),
    { result: "pending", version: t(15) },
  );
  assertEquals(
    aggregateChecks([
      run("ci", "completed", "success", t(0), t(10)),
      run("e2e", "completed", "cancelled", t(5), t(12)),
    ]),
    { result: "failure", version: t(12) },
  );
});

function taskIn(dir: string): void {
  writeTaskState(createTaskState({
    taskId: "t1",
    worktree: dir,
    baseCommit: "0123456789abcdef0123456789abcdef01234567",
    portOffset: 0,
  }));
}

function fakeGh(
  pr: Record<string, unknown> | "404",
  runs: CheckRun[],
): GhRunner & { calls: string[][] } {
  const calls: string[][] = [];
  const gh = ((args: string[]) => {
    calls.push(args);
    if (args[1] === "repos/o/r/pulls/7") {
      if (pr === "404") return Promise.reject(new GhNotFound("HTTP 404"));
      return Promise.resolve(JSON.stringify(pr));
    }
    if (args.includes(`repos/o/r/commits/${SHA}/check-runs?per_page=100`)) {
      return Promise.resolve(
        runs.map((r) => JSON.stringify(r)).join("\n") + "\n",
      );
    }
    return Promise.reject(new Error(`unexpected gh ${args.join(" ")}`));
  }) as GhRunner & { calls: string[][] };
  gh.calls = calls;
  return gh;
}

const PR = {
  state: "open",
  draft: false,
  merged_at: null,
  updated_at: "2026-01-11T09:00:10Z",
  created_at: "2026-01-11T08:00:00Z",
  html_url: "https://github.com/o/r/pull/7",
  head: { sha: SHA },
};

Deno.test("fdh ext poll: a linked PR's URL, state and CI result become observations; a repeat is a duplicate", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => {
      upsertLink(l, "KAN-1", 7);
      upsertLink(l, "KAN-2"); // no PR yet: not polled
    });
    const gh = fakeGh(PR, [
      run(
        "ci",
        "completed",
        "success",
        "2026-11-11T09:00:00Z",
        "2026-11-11T09:05:00Z",
      ),
    ]);
    const first = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(first.map((o) => [o.field, o.value, o.result]), [
      ["pr.url", "https://github.com/o/r/pull/7", "applied"],
      ["pr.state", "open", "applied"],
      ["ci.result", `${SHA}:success`, "applied"],
    ]);
    const again = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(again.map((o) => o.result), [
      "duplicate",
      "duplicate",
      "duplicate",
    ]);
    assertEquals(gh.calls.length, 4);
    const merged = fakeGh(
      {
        ...PR,
        state: "closed",
        merged_at: "2026-11-11T10:00:00Z",
        updated_at: "2026-11-11T10:00:01Z",
      },
      [],
    );
    const later = await pollAndIngest(dir, "t1", "o/r", merged);
    assertEquals(later.map((o) => [o.field, o.value, o.result]), [
      ["pr.url", "https://github.com/o/r/pull/7", "duplicate"],
      ["pr.state", "merged", "applied"],
    ]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: a draft is draft, and a PR that is gone is recorded as deleted", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-1", 7));
    const draft = await pollAndIngest(
      dir,
      "t1",
      "o/r",
      fakeGh({ ...PR, draft: true }, []),
    );
    assertEquals(draft.find((o) => o.field === "pr.state")?.value, "draft");
    const gone = await pollAndIngest(dir, "t1", "o/r", fakeGh("404", []));
    assertEquals(gone.map((o) => [o.field, o.result]), [["object", "deleted"]]);
    assertEquals(
      readExtLedger(dir, "t1").observations["github:pr/7"].object.deleted,
      true,
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: a gh failure other than 404 stops the poll and writes nothing", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-1", 7));
    const broken: GhRunner = () =>
      Promise.reject(new Error("gh api: HTTP 502"));
    await assertRejects(
      () => pollAndIngest(dir, "t1", "o/r", broken),
      Error,
      "502",
    );
    assertEquals(readExtLedger(dir, "t1").observations, {});
    await assertRejects(
      () => pollAndIngest(dir, "t1", "o/../r", broken),
      Error,
      "owner/name",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
