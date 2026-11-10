// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import {
  emptyLedger,
  extLedgerPath,
  ingestObservation,
  planSync,
  readExtLedger,
  upsertLink,
  withExtLedger,
} from "./fdh_ext.ts";
import {
  aggregateChecks,
  type CheckRun,
  GhNotFound,
  type GhRunner,
  makeGhRunner,
  pollAndIngest,
  repoSlugFromRemote,
  validateRepoSlug,
} from "./fdh_ext_poll.ts";
import { createTaskState, writeTaskState } from "./fdh_task.ts";

const SHA = "025ec7679c2b0d8b15956c44a92fafe122f15334";
const SHA2 = "1111111111111111111111111111111111111111";
const t = (s: number) => new Date(Date.UTC(2026, 0, 11, 9, 0, s)).toISOString();
const run = (
  id: number,
  name: string,
  status: string,
  conclusion: string | null,
  started: string | null,
  completed: string | null,
  app = "github-actions",
): CheckRun => ({
  id,
  app,
  name,
  status,
  conclusion,
  started_at: started,
  completed_at: completed,
});
const plus1 = (iso: string) => new Date(Date.parse(iso) + 1).toISOString();

Deno.test("fdh ext poll: GitHub remotes give owner/name; dot segments and credentials are refused or hidden", () => {
  assertEquals(
    repoSlugFromRemote("https://github.com/Floorp-Projects/Floorp"),
    "Floorp-Projects/Floorp",
  );
  assertEquals(repoSlugFromRemote("git@github.com:o/r.git"), "o/r");
  assertEquals(repoSlugFromRemote("ssh://git@github.com/o/r.git\n"), "o/r");
  assertEquals(
    repoSlugFromRemote("https://x-access-token:DUMMY@github.com/o/r.git"),
    "o/r",
  );
  const err = assertThrows(() =>
    repoSlugFromRemote("https://x-access-token:DUMMY@gitlab.com/o/r")
  );
  assert(err instanceof Error && !err.message.includes("DUMMY"), String(err));
  for (const bad of ["../..", "o/..", "./x", "o/r/x", "o"]) {
    assertThrows(() => validateRepoSlug(bad), Error, "owner/name");
  }
  assertThrows(() => repoSlugFromRemote("https://github.com/../.."), Error);
});

Deno.test("fdh ext poll: checks use the latest run per app and name, as branch protection does", () => {
  assertEquals(aggregateChecks([]), null);
  // A failed run re-run to success (same suite, higher id) is success.
  assertEquals(
    aggregateChecks([
      run(1, "ci", "completed", "failure", t(0), t(10)),
      run(2, "ci", "completed", "success", t(20), t(30)),
      run(3, "lint", "completed", "skipped", t(1), t(2)),
    ]),
    { result: "success", version: plus1(t(30)) },
  );
  // A queued re-run (no start time yet) counts: pending, not the old failure.
  assertEquals(
    aggregateChecks([
      run(1, "ci", "completed", "failure", t(0), t(10)),
      run(2, "ci", "queued", null, null, null),
    ])?.result,
    "pending",
  );
  // A gate run for an earlier event (opened, then ready_for_review) that
  // failed does not count once a later run of the same check passed.
  assertEquals(
    aggregateChecks([
      run(1, "review-gate", "completed", "failure", t(0), t(10)),
      run(2, "review-gate", "completed", "failure", t(20), t(30)),
      run(3, "review-gate", "completed", "success", t(40), t(50)),
    ])?.result,
    "success",
  );
  // Another app's check of the same name is a separate check.
  assertEquals(
    aggregateChecks([
      run(1, "build", "completed", "failure", t(0), t(10), "other-ci"),
      run(2, "build", "completed", "success", t(1), t(11)),
    ])?.result,
    "failure",
  );
  // Waiting for an approval is not a result yet.
  assertEquals(
    aggregateChecks([
      run(1, "deploy", "completed", "action_required", t(0), t(5)),
    ])?.result,
    "pending",
  );
  assertEquals(
    aggregateChecks([
      run(1, "ci", "completed", "success", t(0), t(10)),
      run(2, "e2e", "completed", "cancelled", t(5), t(12)),
    ]),
    { result: "failure", version: plus1(t(12)) },
  );
});

Deno.test("fdh ext poll: a result orders after a pending reading of the same second, and never before the PR's update", () => {
  const pending = aggregateChecks([
    run(1, "ci", "in_progress", null, t(9), null),
  ]);
  const done = aggregateChecks([
    run(1, "ci", "completed", "failure", t(9), t(9)),
  ]);
  assert(Date.parse(done!.version) > Date.parse(pending!.version));
  // A head whose checks ran long ago (a force-push back): the PR's
  // updated_at, which the push moved, is the floor.
  assertEquals(
    aggregateChecks(
      [run(1, "ci", "completed", "success", t(0), t(5))],
      t(50),
    ),
    { result: "success", version: plus1(t(50)) },
  );
  // Only null times: the floor, not 1970.
  assertEquals(
    aggregateChecks([run(1, "ci", "queued", null, null, null)], t(50))
      ?.version,
    t(50),
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

type Fake = GhRunner & { calls: string[][] };

function fakeGh(
  prs: Record<string, Record<string, unknown> | "404">,
  runs: Record<string, CheckRun[]>,
): Fake {
  const calls: string[][] = [];
  const gh = ((args: string[]) => {
    calls.push(args);
    const pr = /^repos\/o\/r\/pulls\/(\d+)$/.exec(args[1] ?? "");
    if (pr) {
      const value = prs[pr[1]];
      if (value === undefined || value === "404") {
        return Promise.reject(new GhNotFound("HTTP 404"));
      }
      return Promise.resolve(JSON.stringify(value));
    }
    const checks = args.find((a) => a.includes("/check-runs?per_page=100"));
    const sha = checks ? /commits\/([0-9a-f]+)\//.exec(checks)?.[1] : undefined;
    if (sha !== undefined) {
      return Promise.resolve(
        (runs[sha] ?? []).map((r) => JSON.stringify(r)).join("\n") + "\n",
      );
    }
    return Promise.reject(new Error(`unexpected gh ${args.join(" ")}`));
  }) as Fake;
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

Deno.test("fdh ext poll: a linked PR's URL, state and CI become observations; a repeat is a duplicate", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => {
      upsertLink(l, "KAN-1", 7);
      upsertLink(l, "KAN-2"); // no PR yet: not polled
    });
    const gh = fakeGh({ "7": PR }, {
      [SHA]: [
        run(
          1,
          "ci",
          "completed",
          "success",
          "2026-01-11T09:00:00Z",
          "2026-01-11T09:05:00Z",
        ),
      ],
    });
    const first = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(first.errors, []);
    assertEquals(first.observed.map((o) => [o.field, o.value, o.result]), [
      ["pr.url", "https://github.com/o/r/pull/7", "applied"],
      ["pr.state", "open", "applied"],
      ["ci.result", `${SHA}:success`, "applied"],
    ]);
    const again = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(again.observed.map((o) => o.result), [
      "duplicate",
      "duplicate",
      "duplicate",
    ]);
    assertEquals(gh.calls.length, 4);
    // A force-push to another head whose checks finished before: not stale.
    const pushed = fakeGh(
      {
        "7": { ...PR, updated_at: "2026-01-11T10:00:00Z", head: { sha: SHA2 } },
      },
      {
        [SHA2]: [
          run(
            5,
            "ci",
            "completed",
            "failure",
            "2026-01-11T08:30:00Z",
            "2026-01-11T08:40:00Z",
          ),
        ],
      },
    );
    const later = await pollAndIngest(dir, "t1", "o/r", pushed);
    assertEquals(later.observed.map((o) => [o.field, o.value, o.result]), [
      ["pr.url", "https://github.com/o/r/pull/7", "duplicate"],
      ["pr.state", "open", "applied"],
      ["ci.result", `${SHA2}:failure`, "applied"],
    ]);
    const merged = fakeGh(
      {
        "7": {
          ...PR,
          state: "closed",
          merged_at: "2026-01-11T11:00:00Z",
          updated_at: "2026-01-11T11:00:01Z",
        },
      },
      {},
    );
    assertEquals(
      (await pollAndIngest(dir, "t1", "o/r", merged)).observed.find((o) =>
        o.field === "pr.state"
      )?.value,
      "merged",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: a 404 is an error, never a deletion, and the other links are still polled", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => {
      upsertLink(l, "KAN-1", 7);
      upsertLink(l, "KAN-2", 8);
    });
    const gh = fakeGh({
      "7": "404",
      "8": { ...PR, draft: true, html_url: "https://github.com/o/r/pull/8" },
    }, {});
    const result = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(result.errors.map((e) => e.ref), ["github:pr/7"]);
    assert(result.errors[0].error.includes("not recorded as a deletion"));
    assertEquals(
      result.observed.find((o) => o.field === "pr.state")?.value,
      "draft",
    );
    assertEquals(
      readExtLedger(dir, "t1").observations["github:pr/7"],
      undefined,
    );
    // A gh failure other than 404 is an error for that link too.
    const broken: GhRunner = () =>
      Promise.reject(new Error("gh api: HTTP 502"));
    const down = await pollAndIngest(dir, "t1", "o/r", broken);
    assertEquals(down.observed, []);
    assertEquals(down.errors.map((e) => e.error.includes("502")), [true, true]);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: a linked PR number that is not a positive integer is refused when the ledger is read", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    const ledger = emptyLedger("t1");
    upsertLink(ledger, "KAN-1", 7);
    (ledger.links["KAN-1"] as unknown as { pr: string }).pr = "7/../../user";
    Deno.mkdirSync(`${dir}/_dist/fdh/ext`, { recursive: true });
    Deno.writeTextFileSync(extLedgerPath(dir, "t1"), JSON.stringify(ledger));
    assertThrows(
      () => readExtLedger(dir, "t1"),
      Error,
      "invalid fdh ext ledger",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: a missing head's check runs is reported as such, and http credentials are hidden too", async () => {
  const dir = await Deno.makeTempDir();
  try {
    taskIn(dir);
    await withExtLedger(dir, "t1", (l) => upsertLink(l, "KAN-1", 7));
    const gh: GhRunner = (args) =>
      args[1] === "repos/o/r/pulls/7"
        ? Promise.resolve(JSON.stringify(PR))
        : Promise.reject(new GhNotFound("HTTP 404"));
    const result = await pollAndIngest(dir, "t1", "o/r", gh);
    assertEquals(result.errors.length, 1);
    assert(
      result.errors[0].error.includes("check runs"),
      result.errors[0].error,
    );
    const err = assertThrows(() =>
      repoSlugFromRemote("http://user:SECRET@example.com/o/r")
    );
    assert(err instanceof Error && !err.message.includes("SECRET"));
    // A malformed PR response is an error, not an "open" PR.
    const odd: GhRunner = () =>
      Promise.resolve(JSON.stringify({ ...PR, draft: "no" }));
    const bad = await pollAndIngest(dir, "t1", "o/r", odd);
    assert(bad.errors[0].error.includes("unexpected PR response"));
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("fdh ext poll: ssh remotes keep git@ and drop any other user or password", () => {
  assertEquals(
    repoSlugFromRemote("ssh://git:SECRET@github.com/o/r.git"),
    "o/r",
  );
  const err = assertThrows(() =>
    repoSlugFromRemote("ssh://ghp_TOKEN@example.com/o/r.git")
  );
  assert(
    err instanceof Error && !err.message.includes("ghp_TOKEN"),
    String(err),
  );
  const pw = assertThrows(() =>
    repoSlugFromRemote("ssh://alice:SECRET@example.com/o/r.git")
  );
  assert(pw instanceof Error && !pw.message.includes("SECRET"), String(pw));
  // A raw @ in a password, an upper-case GIT user, the scp form.
  assertEquals(
    repoSlugFromRemote("ssh://git:p@ss@github.com/o/r.git"),
    "o/r",
  );
  assertEquals(repoSlugFromRemote("ssh://GIT@github.com/o/r.git"), "o/r");
  const at = assertThrows(() =>
    repoSlugFromRemote("https://u:p@SECRET@example.com/o/r")
  );
  assert(at instanceof Error && !at.message.includes("SECRET"), String(at));
  const scp = assertThrows(() =>
    repoSlugFromRemote("ghp_TOKEN@example.com:o/r.git")
  );
  assert(
    scp instanceof Error && !scp.message.includes("ghp_TOKEN"),
    String(scp),
  );
});

Deno.test({
  name: "fdh ext poll: a missing gh and a gh that hangs give clear errors",
  // test:host runs without --allow-run; run with -A to include this one.
  ignore: Deno.permissions.querySync({ name: "run" }).state !== "granted",
}, async () => {
  await assertRejects(
    () => makeGhRunner("fdh-no-such-gh-binary")(["api", "x"]),
    Error,
    "is not installed or not on PATH",
  );
  // A stand-in that hangs: this Deno, sleeping for 30 s.
  const hang = makeGhRunner(Deno.execPath(), 300);
  const started = Date.now();
  await assertRejects(
    () => hang(["eval", "await new Promise((r) => setTimeout(r, 30000))"]),
    Error,
    "took longer than 300 ms",
  );
  assert(Date.now() - started < 20_000, "the hanging command was not stopped");
});

Deno.test("fdh ext poll: the PR URL is mirrored as one fdh comment per URL", () => {
  const ledger = emptyLedger("t1");
  upsertLink(ledger, "KAN-1", 7);
  ingestObservation(ledger, {
    ref: "github:pr/7",
    field: "pr.url",
    value: "https://github.com/o/r/pull/7",
    version: t(0),
  });
  const [action] = planSync(ledger).actions;
  assertEquals(
    [action.mirrorField, action.retry, action.key],
    [
      "pr-link-comment",
      "keyed-once",
      "KAN-1:pr.url:https://github.com/o/r/pull/7",
    ],
  );
});
