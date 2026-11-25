// SPDX-License-Identifier: MPL-2.0
import { executeQaDispatch, planQaDispatch } from "./fdh_qa_dispatch.ts";
import type {
  QaDispatchOutcome,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
import type { QaDispatchCliTestDependencies } from "./fdh_qa_dispatch_cli_types.ts";
import type { QaCliResult } from "./fdh_qa_cli_types.ts";
const HELP =
  "Usage: fdh qa plan|dispatch|reconcile --task-id ID --agent NAME\n  --source-run-id N --source-attempt N --source-head SHA --qa-head SHA\n  --mode unsigned-validation|published-release|signed-pre-publication [--execute]\n\nAll seven value options are required. Default is a zero-I/O plan.\nOnly dispatch/reconcile accept explicit --execute; fixed Floorp repository,\nexisting QA workflow and main ref. No host/URL/command/token overrides.\nAssignment is NOT QA success, native qualification or publication approval.\nAmbiguous POST outcomes are never blindly redispatched.\n";
const KEYS = [
  "task-id",
  "agent",
  "source-run-id",
  "source-attempt",
  "source-head",
  "qa-head",
  "mode",
];
const FALSE = {
  qaPassed: false,
  nativeQaVerified: false,
  publicationAuthorized: false,
} as const;
function invalid(): QaCliResult {
  return {
    exitCode: 2,
    stdout: JSON.stringify({
      status: "invalid",
      code: "invalidArguments",
      ...FALSE,
    }),
  };
}
function integer(value: string | undefined): number {
  if (!value || !/^[1-9][0-9]{0,15}$/.test(value)) {
    throw new Error("Invalid identity");
  }
  const parsed = Number(value);
  if (!Number.isSafeInteger(parsed) || String(parsed) !== value) {
    throw new Error("Invalid identity");
  }
  return parsed;
}
/** Only explicit execute imports runtime or resolves a worktree. */
export async function runQaDispatchCli(
  raw: readonly string[],
  test?: QaDispatchCliTestDependencies,
): Promise<QaCliResult> {
  if (raw[0] !== "qa" || !["plan", "dispatch", "reconcile"].includes(raw[1])) {
    return invalid();
  }
  if (raw.length === 3 && ["--help", "-h"].includes(raw[2])) {
    return { exitCode: 0, stdout: HELP };
  }
  if (raw.length > 18) return invalid();
  const values = new Map<string, string>();
  let execute = false;
  for (let index = 2; index < raw.length; index++) {
    const token = raw[index];
    if (
      typeof token !== "string" || token.length > 256 || !token.startsWith("--")
    ) return invalid();
    if (token === "--execute") {
      if (execute || raw[1] === "plan") return invalid();
      execute = true;
      continue;
    }
    const equal = token.indexOf("=");
    const key = token.slice(2, equal === -1 ? undefined : equal);
    if (!KEYS.includes(key) || values.has(key)) return invalid();
    const value = equal === -1 ? raw[++index] : token.slice(equal + 1);
    if (
      typeof value !== "string" || !value || value.length > 256 ||
      value.startsWith("--")
    ) return invalid();
    values.set(key, value);
  }
  if (values.size !== KEYS.length) return invalid();
  let request: QaDispatchRequest;
  try {
    const mode = values.get("mode");
    if (
      mode !== "unsigned-validation" && mode !== "published-release" &&
      mode !== "signed-pre-publication"
    ) return invalid();
    request = {
      taskId: values.get("task-id")!,
      agent: values.get("agent")!,
      sourceRunId: integer(values.get("source-run-id")),
      sourceAttempt: integer(values.get("source-attempt")),
      sourceHead: values.get("source-head")!,
      qaHead: values.get("qa-head")!,
      mode,
    };
    const plan = await planQaDispatch(request);
    if (!execute) {
      return {
        exitCode: 0,
        stdout: JSON.stringify({
          status: "planned",
          key: plan.key,
          plan,
          ...FALSE,
        }),
      };
    }
  } catch {
    return invalid();
  }
  try {
    const action = raw[1] === "reconcile" ? "reconcile" : "execute";
    let outcome: QaDispatchOutcome;
    if (test) outcome = await test.execute(request, action);
    else {
      const [{ resolveCurrentWorktree }, { createQaDispatchRuntime }] =
        await Promise.all([
          import("./fdh_task.ts"),
          import("./fdh_qa_dispatch_runtime.ts"),
        ]);
      const runtime = await createQaDispatchRuntime(
        await resolveCurrentWorktree(),
        request,
      );
      outcome = await executeQaDispatch(request, runtime, action);
    }
    return {
      exitCode: outcome.status === "assigned" ? 0 : 1,
      stdout: JSON.stringify({ ...outcome, ...FALSE }),
    };
  } catch {
    return {
      exitCode: 1,
      stdout: JSON.stringify({
        status: "failed",
        code: "assignmentFailed",
        ...FALSE,
      }),
    };
  }
}
