// SPDX-License-Identifier: MPL-2.0
import * as path from "@std/path";
import { fdhDir, readTaskStateFor } from "./fdh_task.ts";
import { lockPathFor, withFileLock } from "./fdh_lock.ts";
import { finishExtOp, readExtLedger, startExtOp } from "./fdh_ext.ts";
import { DEFAULT_INTENT_TTL_MS, readJob } from "./fdh_executor.ts";
import { createQaGhTransport } from "./fdh_qa_gh_transport.ts";
import { planQaDispatch } from "./fdh_qa_dispatch.ts";
import type { QaGhOperation } from "./fdh_qa_gh_transport_types.ts";
import type {
  QaDispatchDependencies,
  QaDispatchIntent,
  QaDispatchRecord,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
import type { QaDispatchRuntimeTestDependencies } from "./fdh_qa_dispatch_runtime_types.ts";
const REPO = "Floorp-Projects/Floorp";
const SOURCE = ".github/workflows/package_and_publish_release.yml";
const QA = ".github/workflows/validate-final-package-qa.yml";
function object(value: unknown): Record<string, unknown> | null {
  return typeof value === "object" && value !== null && !Array.isArray(value)
    ? value as Record<string, unknown>
    : null;
}
function positive(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}
function jsonFile(file: string): unknown {
  const stat = Deno.lstatSync(file);
  if (!stat.isFile || stat.isSymlink || stat.size > 65536) {
    throw new Error("Invalid assignment record");
  }
  const fd = Deno.openSync(file, { read: true });
  try {
    const opened = fd.statSync();
    if (!opened.isFile || opened.size > 65536) {
      throw new Error("Invalid assignment record");
    }
    const bytes = new Uint8Array(65537);
    let used = 0;
    while (used < bytes.length) {
      const count = fd.readSync(bytes.subarray(used));
      if (count === null) break;
      if (!positive(count)) throw new Error("Assignment read made no progress");
      used += count;
    }
    if (used > 65536) throw new Error("Assignment record exceeds byte cap");
    return JSON.parse(
      new TextDecoder("utf-8", { fatal: true }).decode(bytes.subarray(0, used)),
    );
  } finally {
    fd.close();
  }
}
/** Validate each existing parent before creating its next child. Not same-SID isolation. */
function ensureAssignmentDirectory(root: string): string {
  let current = root;
  for (const part of ["_dist", "fdh", "qa-assignments"]) {
    current = path.join(current, part);
    let stat: Deno.FileInfo;
    try {
      stat = Deno.lstatSync(current);
    } catch (error) {
      if (!(error instanceof Deno.errors.NotFound)) throw error;
      Deno.mkdirSync(current, { mode: 0o700 });
      stat = Deno.lstatSync(current);
    }
    if (
      !stat.isDirectory || stat.isSymlink ||
      Deno.realPathSync(current) !== current
    ) throw new Error("Assignment directory aliases are refused");
  }
  return current;
}
function save(file: string, value: QaDispatchRecord): void {
  const temporary = file + "." + Deno.pid + "." + crypto.randomUUID() + ".tmp";
  let created = false;
  try {
    const fd = Deno.openSync(temporary, {
      createNew: true,
      write: true,
      mode: 0o600,
    });
    created = true;
    try {
      const bytes = new TextEncoder().encode(
        JSON.stringify(value, null, 2) + "\n",
      );
      let offset = 0;
      while (offset < bytes.length) {
        const written = fd.writeSync(bytes.subarray(offset));
        if (!positive(written)) {
          throw new Error("Assignment write made no progress");
        }
        offset += written;
      }
      fd.syncSync();
    } finally {
      fd.close();
    }
    Deno.renameSync(temporary, file);
    created = false;
  } finally {
    if (created) Deno.removeSync(temporary);
  }
}
/** Production capabilities are fixed; CLI cannot supply transports or paths. */
export async function createQaDispatchRuntime(
  worktree: string,
  request: QaDispatchRequest,
): Promise<QaDispatchDependencies> {
  const { createQaDispatchWireTransport } = await import(
    "./fdh_qa_dispatch_gh.ts"
  );
  return await createQaDispatchRuntimeForTest(worktree, request, {
    github: createQaGhTransport(),
    wire: createQaDispatchWireTransport(),
  });
}
/** Network test seam only. State, OS lock, intent and epoch fencing stay real. */
export async function createQaDispatchRuntimeForTest(
  worktree: string,
  input: QaDispatchRequest,
  network: QaDispatchRuntimeTestDependencies,
): Promise<QaDispatchDependencies> {
  const plan = await planQaDispatch(input);
  const root = Deno.realPathSync(worktree);
  const state = readTaskStateFor(root, plan.request.taskId);
  const directory = path.join(fdhDir(root), "qa-assignments");
  const file = path.join(
    directory,
    plan.request.taskId + "-" + plan.key.slice(plan.key.lastIndexOf(":") + 1) +
      ".json",
  );
  let locked = false;
  let preflightWorkflowId: number | null = null;
  function same(key: string): void {
    if (key !== plan.key || !locked) {
      throw new Error("Assignment scope is not locked");
    }
  }
  function load(): QaDispatchRecord | null {
    try {
      return jsonFile(file) as QaDispatchRecord;
    } catch (error) {
      if (error instanceof Deno.errors.NotFound) return null;
      throw error;
    }
  }
  async function scopedRecord(record: QaDispatchRecord): Promise<void> {
    if (
      record.schemaVersion !== 1 || record.key !== plan.key ||
      !positive(record.workflowId) ||
      record.request.agent !== record.intent.agent ||
      (await planQaDispatch(record.request)).key !== plan.key
    ) {
      throw new Error("Assignment record scope mismatch");
    }
  }
  function fenced(intent: QaDispatchIntent, allowSucceeded = false): void {
    const op = readExtLedger(root, state.taskId).ops[plan.key];
    if (
      !op || op.key !== plan.key || op.jobId !== intent.jobId ||
      op.epoch !== intent.epoch ||
      (op.outcome !== null && !(allowSucceeded && op.outcome === "succeeded"))
    ) {
      throw new Error("Assignment ledger identity fenced");
    }
    const job = readJob(root, state.taskId, intent.jobId);
    if (job.epoch !== intent.epoch || intent.agent !== plan.request.agent) {
      throw new Error("Assignment intent fenced");
    }
    if (allowSucceeded && job.state === "succeeded") return;
    const lease = job.lease;
    const heartbeat = lease ? Date.parse(lease.heartbeatAt) : NaN;
    const age = Date.now() - heartbeat;
    if (
      job.state !== "running" || !lease || lease.epoch !== intent.epoch ||
      lease.holder !== intent.agent || !positive(lease.ttlMs) ||
      lease.ttlMs > DEFAULT_INTENT_TTL_MS || !Number.isFinite(age) || age < 0 ||
      age >= lease.ttlMs
    ) throw new Error("Assignment intent is not live");
  }
  async function metadata(
    operation: QaGhOperation,
  ): Promise<Record<string, unknown> | null> {
    const result = await network.github.request(operation);
    if (!result.ok || result.kind !== "metadata") return null;
    return object(JSON.parse(result.jsonText));
  }
  return {
    async withLock(key, work) {
      if (key !== plan.key || locked) {
        throw new Error("Invalid assignment lock scope");
      }
      if (ensureAssignmentDirectory(root) !== directory) {
        throw new Error("Assignment directory scope changed");
      }
      return await withFileLock(lockPathFor(file), async () => {
        locked = true;
        try {
          return await work();
        } finally {
          locked = false;
        }
      });
    },
    load(key) {
      same(key);
      return Promise.resolve(load());
    },
    save(record) {
      same(record.key);
      save(file, record);
      return Promise.resolve();
    },
    async preflight(current) {
      same(current.key);
      preflightWorkflowId = null;
      const repository = await metadata({ kind: "repository" });
      const source = await metadata({ kind: "workflow", workflow: "source" });
      const qa = await metadata({ kind: "workflow", workflow: "qa" });
      const run = await metadata({
        kind: "currentRun",
        runId: plan.request.sourceRunId,
      });
      const ref = await network.wire.mainRef();
      const refBody = object(ref.body), commit = object(refBody?.object);
      const successful = run?.status === "completed" &&
        run.conclusion === "success";
      const signedPending = plan.request.mode === "signed-pre-publication" &&
        run?.conclusion === null &&
        ["in_progress", "waiting"].includes(String(run.status));
      if (
        repository?.full_name !== REPO ||
        repository.default_branch !== "main" || source?.path !== SOURCE ||
        source.state !== "active" || !positive(source.id) || qa?.path !== QA ||
        qa.state !== "active" || !positive(qa.id) ||
        run?.id !== plan.request.sourceRunId ||
        run.run_attempt !== plan.request.sourceAttempt ||
        run.workflow_id !== source.id || run.path !== SOURCE ||
        run.head_sha !== plan.request.sourceHead ||
        object(run.repository)?.full_name !== REPO ||
        object(run.head_repository)?.full_name !== REPO ||
        !(successful || signedPending) || ref.status !== 200 ||
        refBody?.ref !== "refs/heads/main" || commit?.type !== "commit" ||
        commit.sha !== plan.request.qaHead
      ) return null;
      preflightWorkflowId = qa.id;
      return { workflowId: qa.id };
    },
    async begin(current) {
      same(current.key);
      // A missing assignment is not evidence that an older intent never POSTed.
      // Preserve its audit identity even after a generic failed resolution.
      if (readExtLedger(root, state.taskId).ops[plan.key]) return null;
      const intent = await startExtOp(
        state,
        current.key,
        plan.request.agent,
        "external",
        JSON.stringify({
          operation: "QA dispatch assignment, not QA pass",
          repository: REPO,
          workflow: QA,
          ref: "main",
          qaHead: plan.request.qaHead,
          sourceAttempt: plan.request.sourceAttempt,
          inputs: current.inputs,
        }),
      );
      return intent.status === "started"
        ? {
          jobId: intent.jobId,
          epoch: intent.epoch,
          agent: plan.request.agent,
        }
        : null;
    },
    async dispatch(current) {
      same(current.key);
      const record = load();
      if (
        !record || record.phase !== "intent" || record.runId !== null ||
        record.runAttempt !== null || record.workflowId !== preflightWorkflowId
      ) {
        throw new Error("Durable intent missing");
      }
      await scopedRecord(record);
      fenced(record.intent);
      return await network.wire.dispatch(plan.request);
    },
    async readRun(id) {
      same(plan.key);
      const record = load();
      if (!record || record.runId !== id) {
        throw new Error("Run is not the dispatch response identity");
      }
      await scopedRecord(record);
      return await metadata({ kind: "currentRun", runId: id });
    },
    async finish(current, intent) {
      same(current.key);
      const record = load();
      if (
        !record || record.phase !== "dispatched" || !positive(record.runId) ||
        !positive(record.runAttempt) ||
        record.intent.jobId !== intent.jobId ||
        record.intent.epoch !== intent.epoch ||
        record.intent.agent !== intent.agent
      ) throw new Error("Assignment confirmation scope mismatch");
      await scopedRecord(record);
      fenced(intent, true);
      await finishExtOp(state, {
        key: plan.key,
        epoch: intent.epoch,
        agent: plan.request.agent,
        outcome: "succeeded",
      });
    },
  };
}
