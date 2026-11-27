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

/** File and clock access of a run, replaceable in tests. */
export interface PlatformQaIo {
  sha256File(file: string): Promise<string>;
  openFile(file: string): Promise<ReadableStream<Uint8Array>>;
  writeFile(file: string, data: Uint8Array): Promise<void>;
  mkdir(dir: string): Promise<void>;
  now(): string;
}

export interface PlatformQaCliResult {
  exitCode: number;
  stdout: string;
}

export interface PlatformQaCliDeps {
  io?: PlatformQaIo;
  readFile?: (file: string) => Promise<Uint8Array>;
  taskStateExists?: (taskId: string) => boolean;
  createTarget?: (
    platform: QaPlatform,
    config: unknown,
    payloadFile?: string,
  ) => Promise<{ target: PlatformQaTarget; redactions: string[] }>;
  activeJobs?: (
    worktree: string,
    taskId: string,
    platform: QaPlatform,
  ) => string[];
}

/**
 * Inputs of a platform QA step job (KAN-145). The hashes are taken when the
 * job starts and are part of its command, so a rerun after a lost attempt
 * checks that the payload and target config are still the same files.
 */
export interface PlatformStepOptions {
  platform: QaPlatform;
  targetFile: string;
  targetSha256: string;
  payloadFile: string;
  payloadSha256: string;
  evalScript?: string;
  expect?: string;
  startUrl?: string;
}

/** A file a platform QA result rests on, pinned when its job settled. */
export interface PlatformQaAttachment {
  /** Path relative to the worktree, with `/` separators. */
  ref: string;
  sha256: string;
  bytes: number;
}

/**
 * What a settled platform QA job left (KAN-146): the commit and worktree it
 * ran on, the payload and target it checked, and the report and screenshot
 * pinned by SHA-256. Kept on the task-state assertion, so the evidence
 * builder can re-hash the files and check the commit binding.
 */
export interface PlatformQaRecord {
  platform: QaPlatform;
  jobId: string;
  /** Worktree HEAD when the job started (from its journal inputs). */
  head: string;
  /** No tracked change and no untracked file when the job started. */
  cleanWorktree: boolean;
  /** The report's outcome, or null when the report could not be read. */
  outcome: PlatformQaOutcome | null;
  payload: { name: string; sha256: string; version: string | null } | null;
  /** The target as the report's preflight saw it (image tag, macOS version). */
  target: string | null;
  attachments: PlatformQaAttachment[];
  /** Why the report does not support a pass (empty when it does). */
  problems: string[];
}
