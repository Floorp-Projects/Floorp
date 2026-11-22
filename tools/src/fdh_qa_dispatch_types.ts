// SPDX-License-Identifier: MPL-2.0
export type QaDispatchMode =
  | "unsigned-validation"
  | "published-release"
  | "signed-pre-publication";
export interface QaDispatchRequest {
  readonly taskId: string;
  readonly agent: string;
  readonly sourceRunId: number;
  readonly sourceAttempt: number;
  readonly sourceHead: string;
  readonly qaHead: string;
  readonly mode: QaDispatchMode;
}
export interface QaDispatchPlan {
  readonly key: string;
  readonly request: QaDispatchRequest;
  readonly ref: "main";
  readonly repository: "Floorp-Projects/Floorp";
  readonly workflow: ".github/workflows/validate-final-package-qa.yml";
  readonly inputs: Readonly<
    {
      package_run_id: string;
      floorp_head_sha: string;
      validation_only: boolean;
      pre_publication: boolean;
    }
  >;
}
export interface QaDispatchIntent {
  readonly jobId: string;
  readonly epoch: number;
  readonly agent: string;
}
export interface QaDispatchRecord {
  readonly schemaVersion: 1;
  readonly key: string;
  readonly request: QaDispatchRequest;
  readonly workflowId: number;
  readonly intent: QaDispatchIntent;
  readonly phase: "intent" | "dispatched" | "assigned" | "unknown" | "mismatch";
  readonly runId: number | null;
  readonly runAttempt: number | null;
}
export interface QaDispatchHttp {
  readonly status: number;
  readonly body: unknown;
}
export interface QaDispatchDependencies {
  withLock<T>(key: string, work: () => Promise<T>): Promise<T>;
  load(key: string): Promise<QaDispatchRecord | null>;
  save(record: QaDispatchRecord): Promise<void>;
  preflight(
    plan: QaDispatchPlan,
  ): Promise<{ readonly workflowId: number } | null>;
  begin(plan: QaDispatchPlan): Promise<QaDispatchIntent | null>;
  dispatch(plan: QaDispatchPlan): Promise<QaDispatchHttp>;
  readRun(id: number): Promise<unknown>;
  finish(plan: QaDispatchPlan, intent: QaDispatchIntent): Promise<void>;
}
export interface QaDispatchOutcome {
  readonly status:
    | "invalid"
    | "planned"
    | "assigned"
    | "pending"
    | "unknown"
    | "mismatch"
    | "failed";
  readonly key: string | null;
  readonly runId: number | null;
  readonly runAttempt: number | null;
  readonly qaPassed: false;
  readonly nativeQaVerified: false;
  readonly publicationAuthorized: false;
}
