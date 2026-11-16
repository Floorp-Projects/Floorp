// SPDX-License-Identifier: MPL-2.0

/**
 * Static fixture types for tools/integration/fdh-trusted-review-workflow.cjs.
 *
 * The harness extracts the trusted-receipt workflow script as text and runs it
 * against mocked GitHub clients. These declarations describe that mock surface
 * so the .cjs harness can be type-checked without `any`, `ts-nocheck` or
 * exclusions; they are type-only and never loaded at runtime.
 */

/** One pull-request review comment returned by the comments API mock. */
export interface ReviewCommentFixture {
  body: string;
  user: { login: string };
  author_association: string;
}

/** Fields a scenario may override inside a generated receipt comment. */
export interface ReviewReceiptFields {
  schemaVersion: number;
  reviewer: string;
  model: string;
  commit: string;
  verdict: string;
  blocking: boolean;
  scope: string;
  tier: string;
}

/** Per-scenario receipt overrides. */
export type ReviewReceiptChanges = Partial<ReviewReceiptFields>;

/** Conclusion the mocked check publication must report. */
export type ScenarioConclusion = "success" | "failure";

/** Fixture switches that steer one workflow scenario. */
export interface ScenarioOptions {
  eventHead?: string;
  headRace?: boolean;
  apiFailure?: boolean;
  childFailure?: boolean;
  invalidOutput?: boolean;
  stale?: boolean;
}

/** Ordered workflow side effect observed by the harness. */
export type ScenarioOperation = "custom-check-completed" | "workflow-failed";

/** Input accepted by the mocked checks.create call. */
export interface CheckCreateInput {
  head_sha: string;
  name: string;
  status: string;
}

/** Input captured from the mocked checks.update call. */
export interface CheckUpdateInput {
  check_run_id: number;
  status: string;
  conclusion: ScenarioConclusion;
}

/** Result of the mocked checks.create call. */
export interface CheckCreateResult {
  data: { id: number };
}

/** Response of the mocked pulls.get call. */
export interface PullRequestFixture {
  data: { number: number; head: { sha: string } };
}

/** Mocked `github.rest.pulls` surface. */
export interface PullsApiFixture {
  get: () => PullRequestFixture;
}

/** Mocked `github.rest.checks` surface. */
export interface ChecksApiFixture {
  create: (input: CheckCreateInput) => CheckCreateResult;
  update: (input: CheckUpdateInput) => Promise<void>;
}

/** Mocked `github.rest.issues` surface. */
export interface IssuesApiFixture {
  listComments: () => void;
}

/** Mocked `github` client consumed by the workflow script. */
export interface GitHubFixture {
  rest: {
    pulls: PullsApiFixture;
    checks: ChecksApiFixture;
    issues: IssuesApiFixture;
  };
  paginate: () => ReviewCommentFixture[];
}

/** Mocked `core` client consumed by the workflow script. */
export interface CoreFixture {
  setFailed: (message: string) => void;
}

/** Child-process shape the harness fabricates for fake children. */
export interface MockChildProcess {
  stdout: { emit(event: string, data: unknown): boolean };
  stderr: { emit(event: string, data: unknown): boolean };
  stdin: { end(input?: string): void };
  emit(event: string, ...args: unknown[]): boolean;
}
