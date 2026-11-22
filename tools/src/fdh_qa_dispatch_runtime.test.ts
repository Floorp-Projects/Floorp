// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  createTaskState,
  readTaskStateFor,
  writeTaskState,
} from "./fdh_task.ts";
import { finishExtOp, readExtLedger, startExtOp } from "./fdh_ext.ts";
import { jobJournalPath, readJob } from "./fdh_executor.ts";
import { executeQaDispatch, planQaDispatch } from "./fdh_qa_dispatch.ts";
import { createQaDispatchRuntimeForTest } from "./fdh_qa_dispatch_runtime.ts";
import type { QaDispatchRequest } from "./fdh_qa_dispatch_types.ts";
import type { QaDispatchRuntimeTestDependencies } from "./fdh_qa_dispatch_runtime_types.ts";
const REPO = "Floorp-Projects/Floorp";
const SOURCE = ".github/workflows/package_and_publish_release.yml",
  QA = ".github/workflows/validate-final-package-qa.yml";
const request: QaDispatchRequest = {
  taskId: "qa-owned",
  agent: "lead",
  sourceRunId: 17,
  sourceAttempt: 2,
  sourceHead: "a".repeat(40),
  qaHead: "b".repeat(40),
  mode: "unsigned-validation",
};
async function temporary(work: (dir: string) => Promise<void>) {
  const dir = Deno.realPathSync(
    await Deno.makeTempDir({ prefix: "fdh-qa-runtime-" }),
  );
  try {
    writeTaskState(
      createTaskState({
        taskId: request.taskId,
        worktree: dir,
        baseCommit: request.sourceHead,
        portOffset: 0,
      }),
    );
    await work(dir);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
}
function network(root: string) {
  let posts = 0;
  let gets = 0;
  let refHead = request.qaHead;
  const deps: QaDispatchRuntimeTestDependencies = {
    github: {
      request(operation) {
        gets++;
        let value: unknown;
        if (operation.kind === "repository") {
          value = { full_name: REPO, default_branch: "main" };
        } else if (operation.kind === "workflow") {
          value = {
            id: operation.workflow === "source" ? 90 : 99,
            path: operation.workflow === "source" ? SOURCE : QA,
            state: "active",
          };
        } else if (operation.kind === "currentRun") {
          value = {
            id: operation.runId,
            workflow_id: operation.runId === 17 ? 90 : 99,
            run_attempt: operation.runId === 17 ? 2 : 1,
            head_sha: operation.runId === 17
              ? request.sourceHead
              : request.qaHead,
            event: "workflow_dispatch",
            path: operation.runId === 17 ? SOURCE : QA,
            repository: { full_name: REPO },
            head_repository: { full_name: REPO },
            status: operation.runId === 17 ? "completed" : "queued",
            conclusion: operation.runId === 17 ? "success" : null,
          };
        } else return Promise.reject(new Error("Unexpected capability"));
        return Promise.resolve({
          ok: true,
          kind: "metadata",
          jsonText: JSON.stringify(value),
          publicationAuthorized: false,
        });
      },
    },
    wire: {
      mainRef() {
        return Promise.resolve({
          status: 200,
          body: {
            ref: "refs/heads/main",
            object: { type: "commit", sha: refHead },
          },
        });
      },
      dispatch() {
        posts++;
        const ledger = readExtLedger(root, request.taskId);
        assertEquals(Object.keys(ledger.ops).length, 1);
        const op = Object.values(ledger.ops)[0];
        const job = readJob(root, request.taskId, op.jobId);
        assertEquals(job.state, "running");
        assertEquals(job.lease?.holder, "lead");
        const files = [
          ...Deno.readDirSync(path.join(root, "_dist/fdh/qa-assignments")),
        ].filter((item) => item.name.endsWith(".json"));
        assertEquals(files.length, 1);
        const record = JSON.parse(
          Deno.readTextFileSync(
            path.join(root, "_dist/fdh/qa-assignments", files[0].name),
          ),
        );
        assertEquals(record.phase, "intent");
        assertEquals(record.intent.jobId, job.jobId);
        return Promise.resolve({
          status: 200,
          body: {
            workflow_run_id: 23,
            run_url: "https://api.github.com/repos/" + REPO +
              "/actions/runs/23",
            html_url: "https://github.com/" + REPO + "/actions/runs/23",
          },
        });
      },
    },
  };
  return {
    deps,
    posts: () => posts,
    gets: () => gets,
    wrongMain: () => {
      refHead = "c".repeat(40);
    },
  };
}
Deno.test("QA runtime persists actual FDH intent before POST and restores assignment without redispatch", () =>
  temporary(async (root) => {
    const f = network(root);
    const first = await executeQaDispatch(
      request,
      await createQaDispatchRuntimeForTest(root, request, f.deps),
      "execute",
    );
    assertEquals(first.status, "assigned");
    assertEquals(first.qaPassed, false);
    const second = await executeQaDispatch(
      request,
      await createQaDispatchRuntimeForTest(root, request, f.deps),
      "execute",
    );
    assertEquals(second.status, "assigned");
    assertEquals(f.posts(), 1);
    const plan = await planQaDispatch(request);
    assertEquals(
      readExtLedger(root, request.taskId).ops[plan.key].outcome,
      "succeeded",
    );
  }));
Deno.test("QA runtime main head mismatch creates no external intent or POST", () =>
  temporary(async (root) => {
    const f = network(root);
    f.wrongMain();
    const result = await executeQaDispatch(
      request,
      await createQaDispatchRuntimeForTest(root, request, f.deps),
      "execute",
    );
    assertEquals(result.status, "mismatch");
    assertEquals(f.posts(), 0);
    assertEquals(
      Object.keys(readExtLedger(root, request.taskId).ops).length,
      0,
    );
  }));
Deno.test("QA runtime lost response remains unknown across process recreation", () =>
  temporary(async (root) => {
    const f = network(root);
    let lost = 0;
    f.deps.wire.dispatch = () => {
      lost++;
      return Promise.reject(new Error("SENSITIVE-STDERR"));
    };
    const first = await executeQaDispatch(
      request,
      await createQaDispatchRuntimeForTest(root, request, f.deps),
      "execute",
    );
    assertEquals(first.status, "unknown");
    const second = await executeQaDispatch(
      request,
      await createQaDispatchRuntimeForTest(root, request, f.deps),
      "execute",
    );
    assertEquals(second.status, "pending");
    assertEquals(lost, 1);
    assert(!JSON.stringify(first).includes("SENSITIVE"));
  }));
Deno.test("QA runtime expired real intent lease refuses POST", () =>
  temporary(async (root) => {
    const f = network(root);
    const runtime = await createQaDispatchRuntimeForTest(root, request, f.deps);
    const begin = runtime.begin;
    runtime.begin = async (plan) => {
      const intent = await begin(plan);
      assert(intent);
      const job = readJob(root, request.taskId, intent.jobId);
      assert(job.lease);
      job.lease.heartbeatAt = "2000-01-01T00:00:00.000Z";
      Deno.writeTextFileSync(
        jobJournalPath(root, request.taskId, intent.jobId),
        JSON.stringify(job),
      );
      return intent;
    };
    assertEquals(
      (await executeQaDispatch(request, runtime, "execute")).status,
      "unknown",
    );
    assertEquals(f.posts(), 0);
  }));
Deno.test("QA runtime refuses a missing task before network", () =>
  temporary(async (root) => {
    const f = network(root);
    await assertRejects(() =>
      createQaDispatchRuntimeForTest(
        root,
        { ...request, taskId: "absent" },
        f.deps,
      )
    );
    assertEquals(f.gets(), 0);
    assertEquals(f.posts(), 0);
  }));
Deno.test("QA real lease expiring after POST never confirms or redispatches", () =>
  temporary(async (root) => {
    const f = network(root);
    const send = f.deps.wire.dispatch;
    f.deps.wire.dispatch = async (request) => {
      const response = await send(request);
      const op = Object.values(readExtLedger(root, request.taskId).ops)[0];
      const job = readJob(root, request.taskId, op.jobId);
      assert(job.lease);
      job.lease.heartbeatAt = "2000-01-01T00:00:00.000Z";
      Deno.writeTextFileSync(
        jobJournalPath(root, request.taskId, op.jobId),
        JSON.stringify(job),
      );
      return response;
    };
    assertEquals(
      (await executeQaDispatch(
        request,
        await createQaDispatchRuntimeForTest(root, request, f.deps),
        "execute",
      )).status,
      "pending",
    );
    assertEquals(
      (await executeQaDispatch(
        request,
        await createQaDispatchRuntimeForTest(root, request, f.deps),
        "reconcile",
      )).status,
      "pending",
    );
    assertEquals(f.posts(), 1);
    assert(
      Object.values(readExtLedger(root, request.taskId).ops)[0].outcome !==
        "succeeded",
    );
  }));
Deno.test("QA actual assignment file lock serializes independent runtime instances", () =>
  temporary(async (root) => {
    const f = network(root);
    const a = await createQaDispatchRuntimeForTest(root, request, f.deps),
      b = await createQaDispatchRuntimeForTest(root, request, f.deps);
    const results = await Promise.all([
      executeQaDispatch(request, a, "execute"),
      executeQaDispatch(request, b, "execute"),
    ]);
    assertEquals(f.posts(), 1);
    assert(results.some((value) => value.status === "assigned"));
    assert(
      results.every((value) =>
        value.qaPassed === false && value.publicationAuthorized === false
      ),
    );
  }));
Deno.test("QA runtime refuses a parent alias before creating an outside assignment directory", () =>
  temporary(async (root) => {
    const f = network(root),
      runtime = await createQaDispatchRuntimeForTest(root, request, f.deps);
    const data = path.join(root, "_dist", "fdh"),
      backup = data + "-owned-backup";
    const external = Deno.realPathSync(
      Deno.makeTempDirSync({ prefix: "fdh-qa-external-" }),
    );
    let linked = false;
    Deno.renameSync(data, backup);
    try {
      Deno.symlinkSync(external, data, {
        type: Deno.build.os === "windows" ? "junction" : "dir",
      });
      linked = true;
      assertEquals(
        (await executeQaDispatch(request, runtime, "execute")).status,
        "unknown",
      );
      assertEquals(f.posts(), 0);
      assertEquals(f.gets(), 0);
      assertEquals([...Deno.readDirSync(external)].length, 0);
    } finally {
      if (linked) Deno.removeSync(data);
      Deno.renameSync(backup, data);
      Deno.removeSync(external, { recursive: true });
    }
  }));
Deno.test("QA runtime refuses an assignment that grows beyond its byte cap after lstat", () =>
  temporary(async (root) => {
    const f = network(root);
    assertEquals(
      (await executeQaDispatch(
        request,
        await createQaDispatchRuntimeForTest(root, request, f.deps),
        "execute",
      )).status,
      "assigned",
    );
    const directory = path.join(root, "_dist/fdh/qa-assignments"),
      name = [...Deno.readDirSync(directory)].find((file) =>
        file.name.endsWith(".json")
      )!.name,
      file = path.join(directory, name),
      text = Deno.readTextFileSync(file);
    const original = Deno.lstatSync;
    let grew = false;
    const before = f.gets();
    Deno.lstatSync = (target) => {
      const stat = original(target);
      if (target === file && !grew) {
        grew = true;
        Deno.writeTextFileSync(file, text + " ".repeat(100000));
      }
      return stat;
    };
    try {
      assertEquals(
        (await executeQaDispatch(
          request,
          await createQaDispatchRuntimeForTest(root, request, f.deps),
          "reconcile",
        )).status,
        "unknown",
      );
      assertEquals(grew, true);
      assertEquals(f.gets(), before);
      assertEquals(f.posts(), 1);
    } finally {
      Deno.lstatSync = original;
    }
  }));
Deno.test("QA runtime cannot widen the existing thirty-minute intent lifetime", () =>
  temporary(async (root) => {
    const f = network(root), send = f.deps.wire.dispatch;
    f.deps.wire.dispatch = async (input) => {
      const response = await send(input),
        op = Object.values(readExtLedger(root, request.taskId).ops)[0],
        job = readJob(root, request.taskId, op.jobId);
      assert(job.lease);
      job.lease.ttlMs = 86400000;
      job.lease.heartbeatAt = new Date(Date.now() - 3600000).toISOString();
      Deno.writeTextFileSync(
        jobJournalPath(root, request.taskId, op.jobId),
        JSON.stringify(job),
      );
      return response;
    };
    assertEquals(
      (await executeQaDispatch(
        request,
        await createQaDispatchRuntimeForTest(root, request, f.deps),
        "execute",
      )).status,
      "pending",
    );
    assertEquals(f.posts(), 1);
    assert(
      Object.values(readExtLedger(root, request.taskId).ops)[0].outcome !==
        "succeeded",
    );
  }));
Deno.test("QA foreign live intent cannot replace the assignment key's expired intent", () =>
  temporary(async (root) => {
    const f = network(root),
      state = readTaskStateFor(root, request.taskId),
      foreign = await startExtOp(
        state,
        "github:Floorp:unrelated-live",
        "lead",
        "external",
        "unrelated operation",
      );
    assertEquals(foreign.status, "started");
    if (foreign.status !== "started") {
      throw new Error("Expected actual foreign fixture intent");
    }
    const runtime = await createQaDispatchRuntimeForTest(root, request, f.deps),
      save = runtime.save;
    runtime.save = async (record) => {
      if (record.phase === "intent") {
        const job = readJob(root, request.taskId, record.intent.jobId);
        assert(job.lease);
        job.lease.heartbeatAt = "2000-01-01T00:00:00.000Z";
        Deno.writeTextFileSync(
          jobJournalPath(root, request.taskId, record.intent.jobId),
          JSON.stringify(job),
        );
        await save({
          ...record,
          intent: { jobId: foreign.jobId, epoch: foreign.epoch, agent: "lead" },
        });
      } else await save(record);
    };
    assertEquals(
      (await executeQaDispatch(request, runtime, "execute")).status,
      "unknown",
    );
    assertEquals(f.posts(), 0);
  }));
Deno.test("QA missing assignment never restarts a prior resolved failed external intent", () =>
  temporary(async (root) => {
    const f = network(root),
      state = readTaskStateFor(root, request.taskId),
      runtime = await createQaDispatchRuntimeForTest(root, request, f.deps);
    runtime.save = () => Promise.reject(new Error("initial storage failed"));
    assertEquals(
      (await executeQaDispatch(request, runtime, "execute")).status,
      "unknown",
    );
    const plan = await planQaDispatch(request),
      op = readExtLedger(root, request.taskId).ops[plan.key];
    await finishExtOp(state, {
      key: plan.key,
      epoch: op.epoch,
      agent: "lead",
      outcome: "failed",
    });
    assertEquals(
      (await executeQaDispatch(
        request,
        await createQaDispatchRuntimeForTest(root, request, f.deps),
        "execute",
      )).status,
      "pending",
    );
    assertEquals(f.posts(), 0);
    assertEquals(
      readExtLedger(root, request.taskId).ops[plan.key].jobId,
      op.jobId,
    );
  }));
