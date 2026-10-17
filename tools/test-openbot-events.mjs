// SPDX-License-Identifier: MPL-2.0
import assert from "node:assert/strict";
import { createHmac } from "node:crypto";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

// Test exactly the privileged inline code, rather than a second implementation.
const workflow = readFileSync(
  new URL("../.github/workflows/openbot-events.yml", import.meta.url),
  "utf8",
);
const match = workflow.match(
  / {10}node --input-type=module <<'NODE'\r?\n([\s\S]*?) {10}NODE(?:\r?\n|$)/,
);
assert.ok(match, "the sender must remain an inline, quoted heredoc");
const source = match[1].replace(/^ {10}/gm, "");
const { formatEvent, deliveryIdFor, sendEvent, main, automationMarker } =
  await import(
    `data:text/javascript;base64,${Buffer.from(source).toString("base64")}`
  );
const repository = { id: 437505831, full_name: "Floorp-Projects/Floorp" };
const time = "2026-10-18T01:00:00Z";
const sha = "a".repeat(40);
const base = () => ({
  repository: { ...repository },
  sender: { login: "contributor", type: "User" },
});
/** @typedef {{number:number, html_url:string, state:string, title:string, body:string, updated_at:string, pull_request?:Record<string, never>}} Issue */
/** @typedef {{body:string, headers:Record<string, string>, redirect:string}} TransportOptions */
/** @returns {Issue} */
const issue = () => ({
  number: 42,
  html_url: "https://github.com/Floorp-Projects/Floorp/issues/42",
  state: "open",
  title: "A bug",
  body: "Reproduction steps",
  updated_at: time,
});
const pr = () => ({
  ...issue(),
  html_url: "https://github.com/Floorp-Projects/Floorp/pull/42",
  head: { sha, repo: { ...repository, fork: false } },
  base: { ref: "main", repo: { ...repository } },
  draft: false,
});
/** @param {string} action */
const issueEvent = (action) => ({ ...base(), action, issue: issue() });
/** @param {string} action */
const prEvent = (action) => ({ ...base(), action, pull_request: pr() });
const commentEvent = () => ({
  ...issueEvent("created"),
  comment: {
    id: 71,
    html_url: `${issue().html_url}#issuecomment-71`,
    created_at: time,
    body: "Here are more steps",
  },
});
/** @param {string} action */
const reviewEvent = (action) => ({
  ...prEvent(action),
  review: {
    id: 72,
    state: "approved",
    html_url: `${pr().html_url}#pullrequestreview-72`,
    commit_id: sha,
    submitted_at: time,
    body: "Looks good",
  },
});
/** @param {string} conclusion */
const runEvent = (conclusion) => ({
  ...base(),
  action: "completed",
  workflow_run: {
    id: 73,
    run_attempt: 1,
    html_url: "https://github.com/Floorp-Projects/Floorp/actions/runs/73",
    name: "Unit tests",
    path: ".github/workflows/colocated_runner_test.yml",
    status: "completed",
    conclusion,
    event: "pull_request",
    head_sha: sha,
    head_branch: "fix/42",
    updated_at: time,
    pull_requests: [{ number: 42 }],
  },
});

test("the privileged workflow only runs inline Node without repository checkout", () => {
  assert.match(workflow, /^permissions: \{\}$/m);
  assert.doesNotMatch(workflow, /^\s*uses:/m);
  assert.doesNotMatch(match[1], /\$\{\{/);
  assert.equal((workflow.match(/\brun: \|/g) ?? []).length, 1);
  for (
    const trigger of [
      "issues",
      "issue_comment",
      "pull_request_target",
      "pull_request_review",
      "workflow_run",
    ]
  ) {
    assert.match(workflow, new RegExp(`^  ${trigger}:`, "m"));
  }
});

for (const action of ["opened", "reopened", "edited"]) {
  test(`issue ${action} creates a bounded source reference`, () => {
    const event = formatEvent(issueEvent(action), "issues");
    assert.equal(event.type, "github.repository.event");
    assert.equal(event.data.kind, "issue");
    assert.equal(event.data.action, action);
    assert.equal(event.data.repository_id, repository.id);
    assert.equal(event.data.number, 42);
    assert.equal(event.data.body, undefined);
  });
}
for (
  const action of [
    "opened",
    "reopened",
    "synchronize",
    "ready_for_review",
    "edited",
  ]
) {
  test(`PR ${action} records the exact head for independent verification`, () => {
    const event = formatEvent(prEvent(action), "pull_request_target");
    assert.equal(event.data.kind, "pull_request");
    assert.equal(event.data.action, action);
    assert.equal(event.data.head_sha, sha);
  });
}
for (const action of ["submitted", "edited", "dismissed"]) {
  test(`review ${action} wakes reevaluation without forwarding its body`, () => {
    const event = formatEvent(reviewEvent(action), "pull_request_review");
    assert.equal(event.data.kind, "pull_request_review");
    assert.equal(event.data.review_id, 72);
    assert.equal(event.data.review_commit_sha, sha);
    assert.equal(event.data.body, undefined);
  });
}
for (
  const conclusion of [
    "success",
    "failure",
    "cancelled",
    "skipped",
    "timed_out",
    "neutral",
    "action_required",
  ]
) {
  test(`completed CI ${conclusion} wakes reevaluation`, () => {
    const event = formatEvent(runEvent(conclusion), "workflow_run");
    assert.equal(event.data.kind, "workflow_run");
    assert.equal(event.data.conclusion, conclusion);
    assert.deepEqual(event.data.pull_request_numbers, [42]);
  });
}
test("CI with no associated PR keeps the head SHA for API lookup", () => {
  const input = runEvent("success");
  input.workflow_run.pull_requests = [];
  const event = formatEvent(input, "workflow_run");
  assert.deepEqual(event.data.pull_request_numbers, []);
  assert.equal(event.data.head_sha, sha);
});
test("extra issue and PR comments include the exact source comment reference", () => {
  const input = commentEvent();
  assert.equal(formatEvent(input, "issue_comment").data.comment_id, 71);
  input.issue = { ...pr(), pull_request: {} };
  input.comment.html_url = `${input.issue.html_url}#issuecomment-71`;
  assert.equal(formatEvent(input, "issue_comment").data.is_pull_request, true);
});
test("known OpenBot, blank and punctuation-only comments do not cause loops", () => {
  /** @type {Array<(input: ReturnType<typeof commentEvent>) => void>} */
  const changes = [
    (x) => {
      x.sender.login = "openbotgit[bot]";
      x.sender.type = "Bot";
    },
    (x) => {
      x.sender.login = "OpenBotGit[Bot]";
      x.comment.body = "Unmarked reply";
    },
    (x) => {
      x.comment.body = " <!-- only a hidden comment --> ";
    },
    (x) => {
      x.comment.body = "👍 +1!!!".replace("1", "");
    },
  ];
  for (const change of changes) {
    const input = commentEvent();
    change(input);
    assert.equal(formatEvent(input, "issue_comment"), null);
  }
});
test("known OpenBot reviews are excluded with or without a content marker", () => {
  for (const body of [automationMarker, "Unmarked review"]) {
    const input = reviewEvent("submitted");
    input.sender.login = "openbotgit[bot]";
    input.review.body = body;
    assert.equal(formatEvent(input, "pull_request_review"), null);
  }
});
test("human marker quotations and other bots' additional information are retained", () => {
  for (
    const sender of [
      { login: "contributor", type: "User" },
      { login: "coderabbitai[bot]", type: "Bot" },
    ]
  ) {
    const comment = commentEvent();
    comment.sender = sender;
    comment.comment.body =
      `The bot wrote ${automationMarker}, but here are new reproduction steps.`;
    assert.equal(formatEvent(comment, "issue_comment").data.comment_id, 71);
    const review = reviewEvent("submitted");
    review.sender = sender;
    review.review.body =
      `Quoting ${automationMarker}; this change still has a defect.`;
    assert.equal(formatEvent(review, "pull_request_review").data.review_id, 72);
  }
});
test("notifier completion is excluded by immutable workflow path as well as name", () => {
  /** @type {Array<(input: ReturnType<typeof runEvent>) => void>} */
  const changes = [
    (x) => {
      x.workflow_run.name = "OpenBot event notification";
    },
    (x) => {
      x.workflow_run.path = ".github/workflows/openbot-events.yml@main";
    },
  ];
  for (const change of changes) {
    const input = runEvent("success");
    change(input);
    assert.equal(formatEvent(input, "workflow_run"), null);
  }
});
test("repository ID, name, source URLs, state, timestamps and head are validated", () => {
  /** @type {Array<(input: ReturnType<typeof issueEvent>) => void>} */
  const changes = [
    (x) => {
      x.repository.id++;
    },
    (x) => {
      x.repository.full_name = "other/Floorp";
    },
    (x) => {
      x.issue.html_url = "https://attacker.invalid/issues/42";
    },
    (x) => {
      x.issue.number = -1;
    },
    (x) => {
      x.issue.state = "closed";
    },
    (x) => {
      x.issue.updated_at = "not a date";
    },
    (x) => {
      x.issue.pull_request = {};
    },
  ];
  for (const change of changes) {
    const input = issueEvent("edited");
    change(input);
    assert.equal(formatEvent(input, "issues"), null);
  }
  const input = prEvent("synchronize");
  input.pull_request.head.sha = "invalid";
  assert.equal(formatEvent(input, "pull_request_target"), null);
});
test("Actions reruns and object key reordering preserve event bytes and delivery ID", () => {
  const input = issueEvent("edited");
  const first = formatEvent(input, "issues");
  /** @param {unknown} value @returns {unknown} */
  const reverseKeys = (value) =>
    Array.isArray(value)
      ? value.map(reverseKeys)
      : value && typeof value === "object"
      ? Object.fromEntries(
        Object.entries(value).reverse().map((
          [key, child],
        ) => [key, reverseKeys(child)]),
      )
      : value;
  const second = formatEvent(reverseKeys(input), "issues");
  assert.equal(JSON.stringify(first), JSON.stringify(second));
  assert.equal(deliveryIdFor(first), deliveryIdFor(second));
  assert.match(deliveryIdFor(first), /^github\.v2\.[a-f0-9]{64}$/);
});
test("body edits in the same timestamp, new heads and rerun attempts get new IDs", () => {
  const input = issueEvent("edited");
  const before = deliveryIdFor(formatEvent(input, "issues"));
  input.issue.body += "new steps";
  assert.notEqual(before, deliveryIdFor(formatEvent(input, "issues")));
  const p = prEvent("synchronize");
  const oldHead = deliveryIdFor(formatEvent(p, "pull_request_target"));
  p.pull_request.head.sha = "b".repeat(40);
  assert.notEqual(
    oldHead,
    deliveryIdFor(formatEvent(p, "pull_request_target")),
  );
  const run = runEvent("success");
  const attempt1 = deliveryIdFor(formatEvent(run, "workflow_run"));
  run.workflow_run.run_attempt++;
  assert.notEqual(attempt1, deliveryIdFor(formatEvent(run, "workflow_run")));
});

test("editing a review body produces a new stable revision without changing its head or timestamps", () => {
  const input = reviewEvent("edited");
  const before = formatEvent(input, "pull_request_review");
  input.review.body = "An additional reproduction shows a regression";
  const after = formatEvent(input, "pull_request_review");
  assert.equal(after.data.action, "edited");
  assert.equal(after.data.head_sha, before.data.head_sha);
  assert.equal(after.occurredAt, before.occurredAt);
  assert.notEqual(deliveryIdFor(after), deliveryIdFor(before));
  assert.equal(
    deliveryIdFor(after),
    deliveryIdFor(formatEvent(input, "pull_request_review")),
  );
});
test("untrusted shell content stays bounded JSON data", () => {
  const input = issueEvent("opened");
  input.issue.title = "$(touch secret); `evil`\n" + "x".repeat(5000);
  const event = formatEvent(input, "issues");
  assert.equal(event.data.title.length, 1024);
  assert.ok(event.data.title.startsWith("$(touch secret); `evil`\n"));
  assert.ok(Buffer.byteLength(JSON.stringify(event)) < 65536);
});
test("HTTP retry keeps immutable bytes and ID and recomputes the correct signature", async () => {
  const event = formatEvent(issueEvent("opened"), "issues");
  /** @type {TransportOptions[]} */
  const calls = [];
  /** @type {number[]} */
  const waits = [];
  let clock = 1000;
  /** @type {(delay:number) => Promise<void>} */
  const wait = (delay) => {
    waits.push(delay);
    return Promise.resolve();
  };
  /** @type {(url:string, options:TransportOptions) => Promise<{status:number}>} */
  const fetchImpl = (_url, options) => {
    calls.push(options);
    if (calls.length === 1) {
      return Promise.reject(new Error("synthetic timeout"));
    }
    return Promise.resolve({ status: calls.length === 2 ? 503 : 202 });
  };
  const status = await sendEvent(event, {
    url: "https://example.invalid/hook",
    secret: "synthetic",
    now: () => clock++,
    wait,
    fetchImpl,
  });
  assert.equal(status, 202);
  assert.equal(calls.length, 3);
  assert.deepEqual(waits, [2000, 4000]);
  assert.equal(new Set(calls.map((x) => x.body)).size, 1);
  assert.equal(
    new Set(calls.map((x) => x.headers["X-OpenBot-Delivery-Id"])).size,
    1,
  );
  for (const call of calls) {
    assert.equal(call.redirect, "manual");
    const h = call.headers;
    assert.equal(
      h["X-OpenBot-Signature"],
      "sha256=" + createHmac("sha256", "synthetic")
        .update(
          `${h["X-OpenBot-Timestamp"]}.${
            h["X-OpenBot-Delivery-Id"]
          }.${call.body}`,
        ).digest("hex"),
    );
  }
});
test("duplicates finish without retry; permanent rejects never retry", async () => {
  const event = formatEvent(issueEvent("opened"), "issues");
  for (const status of [200, 400, 401, 403, 404, 409, 413, 302]) {
    let calls = 0;
    const pending = sendEvent(event, {
      url: "https://example.invalid/hook",
      secret: "synthetic",
      wait: () => assert.fail("must not wait"),
      fetchImpl: () => {
        calls++;
        return Promise.resolve({ status });
      },
    });
    if (status === 200) assert.equal(await pending, 200);
    else await assert.rejects(pending, /rejected/);
    assert.equal(calls, 1);
  }
});
test("transient errors exhaust bounded retries; invalid URLs never reach transport", async () => {
  const event = formatEvent(issueEvent("opened"), "issues");
  let calls = 0;
  await assert.rejects(
    sendEvent(event, {
      url: "https://example.invalid/hook",
      secret: "synthetic",
      attempts: 2,
      wait: () => Promise.resolve(),
      fetchImpl: () => {
        calls++;
        return Promise.resolve({ status: 429 });
      },
    }),
    /after retries/,
  );
  assert.equal(calls, 2);
  for (
    const url of [
      "http://example.invalid",
      "https://user:pass@example.invalid",
      "https://example.invalid/#x",
    ]
  ) {
    await assert.rejects(
      sendEvent(event, {
        url,
        secret: "synthetic",
        fetchImpl: () => assert.fail("invalid URL"),
      }),
      /Invalid/,
    );
  }
});
/** @param {unknown} input @param {string} eventName @param {Record<string, string>} [configuration] */
async function runMainFixture(input, eventName, configuration = {}) {
  const folder = mkdtempSync(join(tmpdir(), "floorp-openbot-event-"));
  const file = join(folder, "event.json");
  try {
    writeFileSync(file, JSON.stringify(input));
    await main({
      ...configuration,
      GITHUB_EVENT_PATH: file,
      GITHUB_EVENT_NAME: eventName,
    });
  } finally {
    rmSync(folder, { recursive: true, force: true });
  }
}

test("verified fork review with both secrets withheld is the only allowed configuration skip", async () => {
  const input = reviewEvent("edited");
  input.pull_request.head.repo = {
    id: 123,
    full_name: "contributor/Floorp",
    fork: true,
  };
  await runMainFixture(input, "pull_request_review");
  for (
    const configuration of [
      {
        OPENBOT_WEBHOOK_URL: "https://example.invalid/hook",
        OPENBOT_WEBHOOK_SECRET: "",
      },
      { OPENBOT_WEBHOOK_URL: "", OPENBOT_WEBHOOK_SECRET: "synthetic" },
    ]
  ) {
    await assert.rejects(
      runMainFixture(input, "pull_request_review", configuration),
      /OpenBot signing configuration is required/,
    );
  }
});

test("missing configuration fails loudly for ordinary issue, PR, CI and same-repository review events", async () => {
  /** @type {Array<[string, unknown]>} */
  const cases = [
    ["issues", issueEvent("opened")],
    ["pull_request_target", prEvent("opened")],
    ["workflow_run", runEvent("success")],
    ["pull_request_review", reviewEvent("edited")],
  ];
  for (const [eventName, input] of cases) {
    await assert.rejects(
      runMainFixture(input, eventName),
      /OpenBot signing configuration is required/,
    );
  }
});

test("a fork flag without a distinct valid head repository cannot hide missing configuration", async () => {
  const input = reviewEvent("edited");
  input.pull_request.head.repo.fork = true;
  await assert.rejects(
    runMainFixture(input, "pull_request_review"),
    /OpenBot signing configuration is required/,
  );
});
