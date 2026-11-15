// SPDX-License-Identifier: MPL-2.0
/** Opt-in assignment control only; never QA qualification or publication. */
import type {
  QaDispatchDependencies,
  QaDispatchIntent,
  QaDispatchOutcome,
  QaDispatchPlan,
  QaDispatchRecord,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
const REPOSITORY = "Floorp-Projects/Floorp" as const;
const WORKFLOW = ".github/workflows/validate-final-package-qa.yml" as const;
const MODES = [
  "unsigned-validation",
  "published-release",
  "signed-pre-publication",
];
function data(value: unknown): Record<string, unknown> | undefined {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    return;
  }
  const result: Record<string, unknown> = Object.create(null);
  for (const key of Reflect.ownKeys(value)) {
    if (typeof key !== "string") return;
    const field = Object.getOwnPropertyDescriptor(value, key);
    if (!field || !("value" in field)) return;
    result[key] = field.value;
  }
  return result;
}
function positive(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}
function snapshot(input: unknown): QaDispatchRequest {
  const v = data(input);
  if (
    !v ||
    Object.keys(v).sort().join(",") !==
      "agent,mode,qaHead,sourceAttempt,sourceHead,sourceRunId,taskId" ||
    typeof v.taskId !== "string" ||
    !(/^[a-z0-9][a-z0-9-]{0,127}$/).test(v.taskId) ||
    typeof v.agent !== "string" ||
    !(/^[a-z0-9][a-z0-9/_-]{0,63}$/i).test(v.agent) ||
    !positive(v.sourceRunId) || !positive(v.sourceAttempt) ||
    typeof v.sourceHead !== "string" ||
    !(/^[a-f0-9]{40}$/).test(v.sourceHead) ||
    typeof v.qaHead !== "string" || !(/^[a-f0-9]{40}$/).test(v.qaHead) ||
    typeof v.mode !== "string" || !MODES.includes(v.mode)
  ) throw new Error("Invalid QA assignment request");
  return Object.freeze({
    taskId: v.taskId,
    agent: v.agent,
    sourceRunId: v.sourceRunId,
    sourceAttempt: v.sourceAttempt,
    sourceHead: v.sourceHead,
    qaHead: v.qaHead,
    mode: v.mode as QaDispatchRequest["mode"],
  });
}
function identity(request: QaDispatchRequest): string {
  return JSON.stringify([
    request.taskId,
    request.sourceRunId,
    request.sourceAttempt,
    request.sourceHead,
    request.qaHead,
    request.mode,
  ]);
}
export async function planQaDispatch(input: unknown): Promise<QaDispatchPlan> {
  // Snapshot primitives before the first await: caller mutation cannot redirect effects.
  const request = snapshot(input);
  const hash = await crypto.subtle.digest(
    "SHA-256",
    new TextEncoder().encode(identity(request)),
  );
  const digest = Array.from(
    new Uint8Array(hash),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
  return Object.freeze({
    key: "github:Floorp:qa-assignment:" + digest,
    request,
    ref: "main",
    repository: REPOSITORY,
    workflow: WORKFLOW,
    inputs: Object.freeze({
      package_run_id: String(request.sourceRunId),
      floorp_head_sha: request.sourceHead,
      validation_only: request.mode === "unsigned-validation",
      pre_publication: request.mode === "signed-pre-publication",
    }),
  });
}
function outcome(
  status: QaDispatchOutcome["status"],
  key: string | null,
  record?: QaDispatchRecord,
): QaDispatchOutcome {
  return {
    status,
    key,
    runId: record?.runId ?? null,
    runAttempt: record?.runAttempt ?? null,
    qaPassed: false,
    nativeQaVerified: false,
    publicationAuthorized: false,
  };
}
function validIntent(intent: QaDispatchIntent, agent: string): boolean {
  return typeof intent?.jobId === "string" &&
    (/^[a-z0-9][a-z0-9-]{0,127}$/i).test(intent.jobId) &&
    positive(intent.epoch) && intent.agent === agent;
}
function validateRecord(record: QaDispatchRecord, plan: QaDispatchPlan): void {
  if (
    record.schemaVersion !== 1 || record.key !== plan.key ||
    identity(snapshot(record.request)) !== identity(plan.request) ||
    !positive(record.workflowId) ||
    !validIntent(record.intent, record.request.agent) ||
    !["intent", "dispatched", "assigned", "unknown", "mismatch"].includes(
      record.phase,
    ) ||
    !(record.runId === null || positive(record.runId)) ||
    !(record.runAttempt === null || positive(record.runAttempt)) ||
    ((record.phase === "intent" || record.phase === "unknown") &&
      (record.runId !== null || record.runAttempt !== null)) ||
    (["dispatched", "mismatch", "assigned"].includes(record.phase) &&
      record.runId === null) ||
    (record.phase === "assigned" && record.runAttempt === null)
  ) throw new Error("Invalid QA assignment record");
}
function responseRunId(status: number, body: unknown): number | null {
  const v = data(body);
  if (status !== 200 || !v || !positive(v.workflow_run_id)) return null;
  const id = v.workflow_run_id;
  return v.run_url ===
        "https://api.github.com/repos/" + REPOSITORY + "/actions/runs/" + id &&
      v.html_url === "https://github.com/" + REPOSITORY + "/actions/runs/" + id
    ? id
    : null;
}
function matchedAttempt(
  value: unknown,
  plan: QaDispatchPlan,
  record: QaDispatchRecord,
): number | null {
  const run = data(value);
  if (!run) return null;
  const repo = data(run.repository), headRepo = data(run.head_repository);
  if (
    run.id !== record.runId || run.workflow_id !== record.workflowId ||
    !positive(run.run_attempt) ||
    run.head_sha !== plan.request.qaHead || run.event !== "workflow_dispatch" ||
    run.path !== WORKFLOW ||
    repo?.full_name !== REPOSITORY || headRepo?.full_name !== REPOSITORY ||
    (record.runAttempt !== null && record.runAttempt !== run.run_attempt) ||
    typeof run.status !== "string" ||
    !["queued", "in_progress", "completed", "waiting", "pending", "requested"]
      .includes(run.status) ||
    !(run.conclusion === null || typeof run.conclusion === "string")
  ) return null;
  return run.run_attempt;
}
async function reconcile(
  plan: QaDispatchPlan,
  record: QaDispatchRecord,
  deps: QaDispatchDependencies,
): Promise<QaDispatchOutcome> {
  if (record.runId === null) return outcome("pending", plan.key, record);
  let remote: unknown;
  try {
    remote = await deps.readRun(record.runId);
  } catch {
    return outcome("pending", plan.key, record);
  }
  const attempt = matchedAttempt(remote, plan, record);
  if (attempt === null) {
    await deps.save({ ...record, phase: "mismatch" });
    return outcome("mismatch", plan.key, record);
  }
  if (record.phase === "assigned") return outcome("assigned", plan.key, record);
  if (plan.request.agent !== record.intent.agent) {
    return outcome("pending", plan.key, record);
  }
  // Fence the observed attempt durably before confirming the external intent.
  const observed: QaDispatchRecord = {
    ...record,
    phase: "dispatched",
    runAttempt: attempt,
  };
  await deps.save(observed);
  try {
    await deps.finish(plan, record.intent);
  } catch {
    return outcome("pending", plan.key, observed);
  }
  const assigned: QaDispatchRecord = { ...observed, phase: "assigned" };
  await deps.save(assigned);
  return outcome("assigned", plan.key, assigned);
}
export async function executeQaDispatch(
  input: unknown,
  deps: QaDispatchDependencies,
  action: "plan" | "execute" | "reconcile" = "plan",
): Promise<QaDispatchOutcome> {
  let plan: QaDispatchPlan;
  try {
    if (!["plan", "execute", "reconcile"].includes(action)) {
      throw new Error("Invalid action");
    }
    plan = await planQaDispatch(input);
  } catch {
    return outcome("invalid", null);
  }
  if (action === "plan") return outcome("planned", plan.key);
  try {
    return await deps.withLock(plan.key, async () => {
      const previous = await deps.load(plan.key);
      if (previous) {
        validateRecord(previous, plan);
        return await reconcile(plan, previous, deps);
      }
      if (action === "reconcile") return outcome("pending", plan.key);
      let preflight: Awaited<ReturnType<QaDispatchDependencies["preflight"]>>;
      try {
        preflight = await deps.preflight(plan);
      } catch {
        return outcome("failed", plan.key);
      }
      if (!preflight || !positive(preflight.workflowId)) {
        return outcome("mismatch", plan.key);
      }
      const intent = await deps.begin(plan);
      if (!intent) return outcome("pending", plan.key);
      if (!validIntent(intent, plan.request.agent)) {
        throw new Error("Invalid intent ownership");
      }
      let record: QaDispatchRecord = {
        schemaVersion: 1,
        key: plan.key,
        request: plan.request,
        workflowId: preflight.workflowId,
        intent,
        phase: "intent",
        runId: null,
        runAttempt: null,
      };
      // Both the existing FDH intent and this assignment record precede the POST.
      await deps.save(record);
      let id: number | null = null;
      try {
        const response = await deps.dispatch(plan);
        id = responseRunId(response.status, response.body);
      } catch { /* Ambiguous acceptance is never a retry instruction. */ }
      if (id === null) {
        record = { ...record, phase: "unknown" };
        await deps.save(record);
        return outcome("unknown", plan.key, record);
      }
      record = { ...record, phase: "dispatched", runId: id };
      try {
        await deps.save(record);
      } catch {
        // Preserve the exact returned identity for diagnosis, without pretending it is durable.
        return outcome("unknown", plan.key, record);
      }
      return await reconcile(plan, record, deps);
    });
  } catch {
    return outcome("unknown", plan.key);
  }
}
