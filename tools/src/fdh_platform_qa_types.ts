// SPDX-License-Identifier: MPL-2.0

/** Platforms with a QA target (KAN-144). Windows comes later (KAN-147). */
export type QaPlatform = "macos" | "linux";

/** passed: every step ran and cleanup left the target clean. */
export type PlatformQaOutcome = "passed" | "failed" | "blocked";

/** Common view of MacosQaTarget and LinuxQaTarget used by the runner. */
export interface PlatformQaTarget {
  readonly platform: QaPlatform;
  preflight(): Promise<PlatformQaPreflight>;
  /** Builds what the target needs once (the Linux image); no-op elsewhere. */
  prepare(): Promise<Record<string, unknown>>;
  acquire(taskId: string): Promise<Record<string, unknown>>;
  stage(
    taskId: string,
    payload: ReadableStream<Uint8Array>,
    sha256: string,
  ): Promise<{ bytes: number; version: string }>;
  launch(taskId: string, startUrl?: string): Promise<{ version: string }>;
  evaluate(taskId: string, script: string): Promise<unknown>;
  /** Base64 PNG. */
  screenshot(taskId: string): Promise<string>;
  cleanup(
    taskId: string,
  ): Promise<{ clean: boolean } & Record<string, unknown>>;
}

export interface PlatformQaPreflight {
  ready: boolean;
  blockers: string[];
  /** Task holding the target, "-foreign" for one fdh did not make, or null. */
  holder: string | null;
  /** Needs prepare() first (the Linux image is missing). */
  needsPrepare: boolean;
  details: Record<string, unknown>;
}

export interface PlatformQaRunOptions {
  taskId: string;
  payloadPath: string;
  /** Expected SHA-256 of the payload, fixed when the job was started. */
  payloadSha256: string;
  /** Directory for the report and screenshot. */
  outDir: string;
  /** Extra chrome-context script; its value is recorded verbatim. */
  evalScript?: string;
  /** Text the eval value (as JSON) must contain. Needs evalScript. */
  expect?: string;
  startUrl?: string;
}

export interface PlatformQaStepRecord {
  name: string;
  ok: boolean;
  startedAt: string;
  finishedAt: string;
  result: unknown;
}

export interface PlatformQaReport {
  schema: "fdh-platform-qa-report/v1";
  taskId: string;
  platform: QaPlatform;
  outcome: PlatformQaOutcome;
  /** Why the run did not pass, e.g. "blocked: lock-held" or "eval-expect". */
  reason: string | null;
  payload: { name: string; sha256: string; version: string | null };
  probe: Record<string, unknown> | null;
  eval: { script: string; value: unknown; expect: string | null } | null;
  screenshot: { file: string; sha256: string } | null;
  cleanupClean: boolean | null;
  steps: PlatformQaStepRecord[];
  startedAt: string;
  finishedAt: string;
}
