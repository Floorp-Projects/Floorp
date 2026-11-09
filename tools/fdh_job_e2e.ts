// SPDX-License-Identifier: MPL-2.0

/**
 * Live B-02 test cases E-1..E-9 for `fdh job` (frozen ADR 8716289, D-23).
 *
 * Runs real fdh processes in this worktree: controllers (`job wait`,
 * `task resume`), detached executors (`job exec`) and the real step
 * (`deno task test:host`). Faults are injected by killing processes and,
 * where a crash window cannot be hit by timing, by `FDH_JOB_FAULT` on the
 * executor. Each case asserts the ADR's expectation and writes its journal
 * and a summary under `--out`.
 *
 *   deno run -A tools/fdh_job_e2e.ts --out docs/qa/fdh-executor/<run> [--only E-2,E-3]
 *
 * Not part of `test:host` (it needs --allow-run and about five minutes).
 */

import * as path from "@std/path";

const FDH = path.fromFileUrl(new URL("./fdh.ts", import.meta.url));
const ROOT = path.dirname(path.dirname(FDH));
const TTL = 3_000;

interface Args {
  out: string;
  only: Set<string> | null;
}

function parseArgs(argv: string[]): Args {
  let out = "";
  let only: Set<string> | null = null;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--out") out = argv[++i] ?? "";
    else if (argv[i] === "--only") only = new Set((argv[++i] ?? "").split(","));
    else throw new Error(`unknown argument ${argv[i]}`);
  }
  if (out === "") throw new Error("--out <dir> is required");
  return { out: path.resolve(out), only };
}

interface Run {
  code: number;
  stdout: string;
  stderr: string;
}

async function fdh(
  args: string[],
  env: Record<string, string> = {},
): Promise<Run> {
  const output = await new Deno.Command(Deno.execPath(), {
    args: ["run", "-A", FDH, ...args],
    cwd: ROOT,
    env: { FDH_JOB_FAULT: "", FDH_JOB_FAULT_ENABLE: "", ...env },
    stdout: "piped",
    stderr: "piped",
  }).output();
  return {
    code: output.code,
    stdout: new TextDecoder().decode(output.stdout).trim(),
    stderr: new TextDecoder().decode(output.stderr).trim(),
  };
}

function lastJson(text: string): Record<string, unknown> {
  const line = text.split(/\r?\n/).filter((l) => l.startsWith("{")).pop();
  if (line === undefined) throw new Error(`no JSON in output: ${text}`);
  return JSON.parse(line);
}

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

function journalPath(taskId: string, jobId: string): string {
  return path.join(ROOT, "_dist", "fdh", "jobs", taskId, `${jobId}.json`);
}

// deno-lint-ignore no-explicit-any
type Journal = any;

function journal(taskId: string, jobId: string): Journal {
  return JSON.parse(Deno.readTextFileSync(journalPath(taskId, jobId)));
}

async function until(
  what: string,
  check: () => boolean,
  timeoutMs = 60_000,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (!check()) {
    if (Date.now() > deadline) throw new Error(`timed out waiting for ${what}`);
    await sleep(200);
  }
}

function executorPid(holder: string | null): number {
  const match = /^executor-(\d+)-/.exec(holder ?? "");
  if (!match) throw new Error(`not an executor holder: ${holder}`);
  return Number(match[1]);
}

function kill(pid: number): void {
  try {
    Deno.kill(pid, "SIGKILL");
  } catch (error) {
    if (!(error instanceof Deno.errors.NotFound)) throw error;
  }
}

async function alive(pid: number): Promise<boolean> {
  if (Deno.build.os === "windows") {
    const out = await new Deno.Command("powershell", {
      args: [
        "-NoProfile",
        "-Command",
        `if (Get-Process -Id ${pid} -ErrorAction SilentlyContinue) { 'yes' } else { 'no' }`,
      ],
      stdout: "piped",
    }).output();
    return new TextDecoder().decode(out.stdout).trim() === "yes";
  }
  try {
    Deno.kill(pid, "SIGCONT");
    return true;
  } catch {
    return false;
  }
}

interface Check {
  name: string;
  passed: boolean;
  detail: string;
}

class Case {
  checks: Check[] = [];
  notes: string[] = [];
  constructor(readonly id: string, readonly title: string) {}
  expect(name: string, passed: boolean, detail: unknown = ""): void {
    this.checks.push({
      name,
      passed,
      detail: typeof detail === "string" ? detail : JSON.stringify(detail),
    });
  }
  get passed(): boolean {
    return this.checks.length > 0 && this.checks.every((c) => c.passed);
  }
}

async function newTask(id: string): Promise<string> {
  const taskId = `e2e-${id.toLowerCase()}-${Date.now().toString(36)}`;
  const run = await fdh([
    "task",
    "start",
    "--task-id",
    taskId,
    "--worktree",
    ROOT,
  ]);
  if (run.code !== 0) throw new Error(`task start failed: ${run.stderr}`);
  return taskId;
}

async function startTest(
  taskId: string,
  extra: string[] = [],
  env: Record<string, string> = {},
): Promise<string> {
  const run = await fdh([
    "job",
    "start",
    "--task-id",
    taskId,
    "--step",
    "test",
    "--lease-ttl-ms",
    String(TTL),
    ...extra,
  ], env);
  if (run.code !== 0) throw new Error(`job start failed: ${run.stderr}`);
  return String(lastJson(run.stdout).jobId);
}

async function wait(taskId: string, jobId: string): Promise<Run> {
  return await fdh([
    "job",
    "wait",
    "--task-id",
    taskId,
    "--job-id",
    jobId,
    "--timeout-ms",
    "180000",
  ]);
}

function runningWithProcess(taskId: string, jobId: string, epoch = 1) {
  return () => {
    const j = journal(taskId, jobId);
    const a = j.attempts.find((x: Journal) => x.epoch === epoch);
    return a?.result === "running" && a.process != null;
  };
}

const CASES: Record<string, (c: Case) => Promise<void>> = {
  async "E-1"(c) {
    const taskId = await newTask(c.id);
    const jobId = await startTest(taskId);
    const controller = new Deno.Command(Deno.execPath(), {
      args: [
        "run",
        "-A",
        FDH,
        "job",
        "wait",
        "--task-id",
        taskId,
        "--job-id",
        jobId,
      ],
      cwd: ROOT,
      stdout: "null",
      stderr: "null",
    }).spawn();
    await until("the step to run", runningWithProcess(taskId, jobId));
    controller.kill("SIGKILL");
    await controller.status;
    const mid = journal(taskId, jobId);
    c.expect(
      "the executor keeps running after its controller was killed",
      mid.state === "running" && mid.epoch === 1,
      { state: mid.state, epoch: mid.epoch },
    );
    const done = await wait(taskId, jobId);
    const j = journal(taskId, jobId);
    c.expect(
      "a new controller reconnects and the job succeeds",
      done.code === 0 && j.state === "succeeded",
      j.state,
    );
    c.expect(
      "one attempt at epoch 1",
      j.attempts.length === 1 && j.epoch === 1,
      j.attempts.map((a: Journal) => a.result),
    );
    save(c, taskId, jobId);
  },

  async "E-2"(c) {
    const taskId = await newTask(c.id);
    const jobId = await startTest(taskId);
    await until("the step to run", runningWithProcess(taskId, jobId));
    const first = journal(taskId, jobId).attempts[0];
    kill(executorPid(first.holder));
    c.notes.push(
      `killed executor ${first.holder}; step root pid ${first.process.pid}`,
    );
    const done = await wait(taskId, jobId);
    const j = journal(taskId, jobId);
    c.expect(
      "the job succeeds after the executor was killed",
      done.code === 0 && j.state === "succeeded",
      j.state,
    );
    c.expect(
      "attempt 1 is lost and attempt 2 (epoch 2) succeeded",
      j.attempts.length === 2 && j.attempts[0].result === "lost" &&
        j.attempts[1].result === "succeeded" && j.epoch === 2,
      j.attempts.map((a: Journal) => `${a.epoch}:${a.result}`),
    );
    c.expect(
      "the lost attempt's step process is not running any more",
      !(await alive(first.process.pid)),
      `pid ${first.process.pid}`,
    );
    save(c, taskId, jobId);
  },

  async "E-3"(c) {
    const taskId = await newTask(c.id);
    const jobId = await startTest(taskId, [], {
      FDH_JOB_FAULT: "exit-after-outbox",
      FDH_JOB_FAULT_ENABLE: "1",
    });
    await until("the executor to exit after its outbox", () => {
      const log = path.join(
        path.dirname(journalPath(taskId, jobId)),
        `${jobId}.outbox-1.json`,
      );
      try {
        Deno.statSync(log);
        return true;
      } catch {
        return false;
      }
    }, 90_000);
    const before = journal(taskId, jobId);
    c.expect(
      "the journal was not updated by the ACK",
      before.state === "running",
      before.state,
    );
    const done = await wait(taskId, jobId);
    const j = journal(taskId, jobId);
    c.expect(
      "reconcile applies the outbox and the job succeeds",
      done.code === 0 && j.state === "succeeded",
      j.state,
    );
    c.expect(
      "not rerun: one attempt",
      j.attempts.length === 1,
      j.attempts.length,
    );
    c.expect(
      "the recovery is recorded",
      j.events.some((e: Journal) => e.kind === "outbox-applied"),
      j.events.map((e: Journal) => e.kind),
    );
    save(c, taskId, jobId);
  },

  async "E-4/E-5"(c) {
    const taskId = await newTask("e4");
    const jobId = await startTest(taskId, [], {
      FDH_JOB_FAULT: `stall:${4 * TTL}`,
      FDH_JOB_FAULT_ENABLE: "1",
    });
    const done = await wait(taskId, jobId);
    // The stalled epoch-1 executor reports after its stall; wait for it.
    await until(
      "the late report of epoch 1",
      () =>
        journal(taskId, jobId).events.some((e: Journal) =>
          e.kind === "stale-epoch" && e.epoch === 1
        ),
      60_000,
    );
    const j = journal(taskId, jobId);
    c.expect(
      "E-4: epoch 2 took over and the job succeeded",
      done.code === 0 && j.state === "succeeded" && j.epoch === 2,
      { state: j.state, epoch: j.epoch },
    );
    c.expect(
      "E-5: the epoch-1 report was rejected as stale and recorded",
      j.events.some((e: Journal) => e.kind === "stale-epoch" && e.epoch === 1),
      j.events.filter((e: Journal) => e.kind === "stale-epoch"),
    );
    c.expect(
      "E-5: attempt 1 stays lost, with the late report in its notes",
      j.attempts[0].result === "lost" &&
        /late report/.test(j.attempts[0].notes),
      j.attempts[0].notes,
    );
    c.expect(
      "the job's result is epoch 2's",
      j.attempts[1].result === "succeeded",
      j.attempts[1].result,
    );
    save(c, taskId, jobId);
  },

  async "E-6"(c) {
    const taskId = await newTask(c.id);
    const intent = await fdh([
      "job",
      "intent",
      "--task-id",
      taskId,
      "--op",
      "push",
      "--target",
      "origin e2e-branch",
      "--agent",
      "agent-a",
    ]);
    const jobId = String(lastJson(intent.stdout).jobId);
    const resume = await fdh(["task", "resume", "--task-id", taskId]);
    const out = lastJson(resume.stdout) as Journal;
    const j = journal(taskId, jobId);
    c.expect(
      "task resume puts the unconfirmed intent into awaiting-approval",
      resume.code === 0 && j.state === "awaiting-approval",
      j.state,
    );
    c.expect(
      "the quota ledger lists it as pending",
      out.approvalPending.some((p: Journal) =>
        p.op === "push" && p.note.includes(jobId)
      ),
      out.approvalPending,
    );
    const again = await fdh(["task", "resume", "--task-id", taskId]);
    c.expect(
      "a second resume does not rerun or re-queue it",
      journal(taskId, jobId).attempts.length === 1 &&
        (lastJson(again.stdout) as Journal).approvalPending.length === 1,
      (lastJson(again.stdout) as Journal).approvalPending.length,
    );
    const resolve = await fdh([
      "job",
      "resolve",
      "--task-id",
      taskId,
      "--job-id",
      jobId,
      "--resolution",
      "succeeded",
      "--note",
      "branch exists on origin",
    ]);
    const r = journal(taskId, jobId);
    const ledger = await fdh(["quota", "status", "--task-id", taskId]);
    c.expect(
      "the approver's resolution closes it",
      resolve.code === 0 && r.state === "succeeded",
      r.state,
    );
    c.expect(
      "and approves its pending entry",
      (lastJson(ledger.stdout) as Journal).approvalPending.length === 0,
      lastJson(ledger.stdout),
    );
    save(c, taskId, jobId);
  },

  async "E-7"(c) {
    const taskId = await newTask(c.id);
    const jobId = await startTest(taskId);
    await until("the step to run", runningWithProcess(taskId, jobId));
    const first = journal(taskId, jobId).attempts[0];
    kill(executorPid(first.holder));
    const marker = path.join(ROOT, `fdh-e2e-e7-${Date.now()}.txt`);
    Deno.writeTextFileSync(marker, "an untracked file changes the inputs\n");
    try {
      const done = await wait(taskId, jobId);
      const j = journal(taskId, jobId);
      c.expect(
        "the job fails instead of rerunning",
        done.code !== 0 && j.state === "failed",
        j.state,
      );
      c.expect("no second attempt", j.attempts.length === 1, j.attempts.length);
      c.expect(
        "the reason is recorded",
        j.events.some((e: Journal) => e.kind === "input-changed"),
        j.events.map((e: Journal) => e.kind),
      );
      save(c, taskId, jobId);
    } finally {
      Deno.removeSync(marker);
    }
  },

  async "E-8"(c) {
    const taskId = await newTask(c.id);
    const jobId = await startTest(taskId, ["--max-attempts", "2"]);
    await until("epoch 1 to run", runningWithProcess(taskId, jobId, 1));
    kill(executorPid(journal(taskId, jobId).attempts[0].holder));
    // A controller respawns epoch 2; kill that executor as well.
    const controller = fdh([
      "job",
      "wait",
      "--task-id",
      taskId,
      "--job-id",
      jobId,
      "--timeout-ms",
      "180000",
    ]);
    await until("epoch 2 to run", runningWithProcess(taskId, jobId, 2), 90_000);
    kill(executorPid(journal(taskId, jobId).attempts[1].holder));
    const done = await controller;
    const j = journal(taskId, jobId);
    c.expect(
      "the job fails after maxAttempts lost attempts",
      done.code !== 0 && j.state === "failed",
      j.state,
    );
    c.expect(
      "exactly two attempts, both lost",
      j.attempts.length === 2 &&
        j.attempts.every((a: Journal) => a.result === "lost"),
      j.attempts.map((a: Journal) => a.result),
    );
    c.expect(
      "gave-up is recorded",
      j.events.some((e: Journal) => e.kind === "gave-up"),
      j.events.map((e: Journal) => e.kind),
    );
    save(c, taskId, jobId);
  },

  async "E-9"(c) {
    const taskId = await newTask(c.id);
    const intent = await fdh([
      "job",
      "intent",
      "--task-id",
      taskId,
      "--op",
      "pr",
      "--target",
      "draft PR",
      "--agent",
      "agent-a",
    ]);
    const jobId = String(lastJson(intent.stdout).jobId);
    await fdh(["task", "resume", "--task-id", taskId]);
    const late = await fdh([
      "job",
      "confirm",
      "--task-id",
      taskId,
      "--job-id",
      jobId,
      "--epoch",
      "1",
      "--agent",
      "agent-a",
      "--outcome",
      "succeeded",
    ]);
    const j = journal(taskId, jobId);
    c.expect("the late confirm is refused", late.code !== 0, late.stderr);
    c.expect(
      "the intent still awaits approval",
      j.state === "awaiting-approval",
      j.state,
    );
    c.expect(
      "the late report is recorded",
      j.events.some((e: Journal) => e.kind === "late-report"),
      j.events.map((e: Journal) => e.kind),
    );
    await fdh([
      "job",
      "resolve",
      "--task-id",
      taskId,
      "--job-id",
      jobId,
      "--resolution",
      "failed",
      "--note",
      "e2e cleanup",
    ]);
    save(c, taskId, jobId);
  },
};

let outDir = "";

function save(c: Case, taskId: string, jobId: string): void {
  const dir = path.join(outDir, c.id.replace("/", "-"));
  Deno.mkdirSync(dir, { recursive: true });
  Deno.copyFileSync(journalPath(taskId, jobId), path.join(dir, "journal.json"));
  const task = path.join(ROOT, "_dist", "fdh", "tasks", `${taskId}.json`);
  const state = JSON.parse(Deno.readTextFileSync(task));
  Deno.writeTextFileSync(
    path.join(dir, "task-events.json"),
    `${JSON.stringify(state.events, null, 2)}\n`,
  );
  c.notes.push(`task ${taskId}, job ${jobId}`);
}

const args = parseArgs(Deno.args);
outDir = args.out;
Deno.mkdirSync(outDir, { recursive: true });
const results: Case[] = [];
for (const [id, run] of Object.entries(CASES)) {
  if (args.only !== null && !args.only.has(id)) continue;
  const c = new Case(id, id);
  const started = Date.now();
  console.log(`[e2e] ${id} ...`);
  try {
    await run(c);
  } catch (error) {
    c.expect(
      "ran to completion",
      false,
      error instanceof Error ? error.message : String(error),
    );
  }
  console.log(
    `[e2e] ${id} ${c.passed ? "PASS" : "FAIL"} (${
      ((Date.now() - started) / 1000).toFixed(1)
    } s)`,
  );
  for (const check of c.checks) {
    console.log(
      `  ${check.passed ? "ok  " : "FAIL"} ${check.name}${
        check.passed ? "" : ` — ${check.detail}`
      }`,
    );
  }
  results.push(c);
}
const summary = {
  ranAt: new Date().toISOString(),
  os: `${Deno.build.os} ${Deno.build.arch}`,
  deno: Deno.version.deno,
  leaseTtlMs: TTL,
  cases: results.map((c) => ({
    id: c.id,
    passed: c.passed,
    checks: c.checks,
    notes: c.notes,
  })),
};
Deno.writeTextFileSync(
  path.join(outDir, "summary.json"),
  `${JSON.stringify(summary, null, 2)}\n`,
);
const failed = results.filter((c) => !c.passed).map((c) => c.id);
console.log(
  failed.length === 0
    ? "[e2e] all cases passed"
    : `[e2e] failed: ${failed.join(", ")}`,
);
Deno.exit(failed.length === 0 ? 0 : 1);
