// SPDX-License-Identifier: MPL-2.0

/**
 * fdh task-state store and command helpers (FDH Gate 3 local loop, KAN-113).
 *
 * `fdh` is a thin CLI that lets an already-running agent (DSH/Codex/Claude)
 * drive the local gate loop: task start -> edit -> dev up -> verify build/test/
 * browser -> evidence -> dev down. This module owns the durable state file
 * (`<worktree>/_dist/fdh/tasks/<id>.json`) and the helpers the CLI composes;
 * it deliberately contains no agent conversation loop, no LLM calls and no
 * network/GitHub operations.
 *
 * The module is dependency-injected where it touches the outside world
 * (`git`, the TCP port probe, child commands) so the fast tests can exercise
 * the contract without long browser runs.
 */

import * as path from "@std/path";
import { encodeHex } from "@std/encoding/hex";
import {
  type DevPortProbe,
  isTcpPortInUse,
  launchPortsForOffset,
  parsePortOffsetSpec,
  resolveAutoDevPortOffset,
} from "./port_offset.ts";

/** Task IDs accepted by the CLI (`--task-id`). */
export const TASK_ID_PATTERN = /^[a-z0-9][a-z0-9-]*$/;

/** Relative location of the fdh state tree inside a worktree. */
export const FDH_DIR = path.join("_dist", "fdh");
export const FDH_TASKS_DIR = path.join(FDH_DIR, "tasks");
export const FDH_OBSERVATIONS_DIR = path.join(FDH_DIR, "observations");

/** Upper bound of an external observation file kept in the task state (256 KiB). */
export const MAX_OBSERVATION_BYTES = 256 * 1024;

/** `--attested-at` shape: date, time and an explicit zone (Z or ±hh:mm). */
const ISO_INSTANT_RE =
  /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}(?::\d{2}(?:\.\d{1,9})?)?(?:Z|[+-]\d{2}:\d{2})$/;

/** Clock skew tolerated between the operator and this host (5 minutes). */
export const MAX_ATTESTED_FUTURE_SKEW_MS = 5 * 60 * 1000;

export type FdhTaskStatus =
  | "running"
  | "succeeded"
  | "failed"
  | "cancelled";

export interface FdhEvent {
  /** ISO 8601 timestamp with offset. */
  timestamp: string;
  /** Human-readable command that produced this event. */
  command: string;
  note: string;
  exitCode: number | null;
}

/** External verification kinds that may produce an assertion (IMP-2 A案). */
export type ExternalVerification = "build" | "host-test" | "browser" | "other";

/**
 * Attestation block for a command that was run outside fdh.
 *
 * fdh did not execute the command; the operator supplies the observation file
 * and the exit code. `logSha256` pins the observation content at record time
 * and is re-checked when the EvidenceManifest is built.
 */
export interface FdhExternalRecord {
  /** Self-declared operator that ran the command and wrote the observation. */
  operator: string;
  /** Observation path relative to the worktree (`_dist/fdh/observations/...`). */
  logRef: string;
  /** SHA-256 (hex) of the observation bytes at record time. */
  logSha256: string;
  /** Byte length of the observation file at record time. */
  bytes: number;
  /** ISO 8601 attestation time; not the command's start/end time. */
  attestedAt: string;
  /** When set, `recordExternalOperation` also records an exit-code assertion. */
  verification?: ExternalVerification;
}

export interface FdhOperation {
  command: string;
  args: string[];
  /** Command line as a human would type it (e.g. `deno task test:host`). */
  displayCommand: string;
  cwd: string;
  startedAt: string;
  endedAt: string;
  exitCode: number;
  /** Captured stdout+stderr, possibly tail-limited for state-file size. */
  output: string;
  /** Set only for commands recorded from outside the fdh CLI. */
  external?: FdhExternalRecord;
}

export interface FdhAssertion {
  name: string;
  expected: string;
  actual: string;
  passed: boolean;
  at: string;
}

export interface FdhCleanup {
  result: "completed" | "partial" | "failed" | "not-required" | "unknown";
  notes: string;
  details?: string[];
}

export interface FdhReproduction {
  result: "reproduced" | "not-reproduced" | "inconclusive";
  notes: string;
  defectRef?: string;
}

/**
 * Setup facts recorded by `fdh task prepare` (IMP-5). Optional so old task
 * states written before the setup step stay readable.
 */
export interface FdhSetup {
  /** Environment variables passed to dev up (e.g. FLOORP_SKIP_PATCHER). */
  guardEnv: Record<string, string>;
  /** Where the runtime `_dist/bin` was copied/unpacked from. */
  runtimeSource: string;
  /** Profile path prepared for this task. */
  profilePath: string;
  /** ISO 8601 time when node_modules/.vite was cleared, or null. */
  viteClearedAt: string | null;
  /** Commands the prepare step ran (for an idempotent replay record). */
  commands: string[];
}

export interface FdhTaskState {
  taskId: string;
  /** Absolute, canonical path of the worktree. */
  worktree: string;
  baseCommit: string;
  /** Resolved integer port offset (never "auto"). */
  portOffset: number;
  createdAt: string;
  status: FdhTaskStatus;
  events: FdhEvent[];
  operations: FdhOperation[];
  assertions: FdhAssertion[];
  cleanup: FdhCleanup;
  reproduction: FdhReproduction;
  /** IMP-5 setup facts; absent on states written before `fdh task prepare`. */
  setup?: FdhSetup;
}

export interface CreateTaskStateInput {
  taskId: string;
  worktree: string;
  baseCommit: string;
  portOffset: number;
  now?: () => string;
}

export function nowIso(): string {
  return new Date().toISOString();
}

/** Validate `--task-id` and return it unchanged, or throw a user-facing error. */
export function validateTaskId(taskId: string): string {
  if (!TASK_ID_PATTERN.test(taskId)) {
    throw new Error(
      `--task-id must match ${TASK_ID_PATTERN}, got ${JSON.stringify(taskId)}`,
    );
  }
  return taskId;
}

export function fdhDir(worktree: string): string {
  return path.join(worktree, FDH_DIR);
}

export function taskStatePath(worktree: string, taskId: string): string {
  validateTaskId(taskId);
  return path.join(worktree, FDH_TASKS_DIR, `${taskId}.json`);
}

export function createTaskState(input: CreateTaskStateInput): FdhTaskState {
  validateTaskId(input.taskId);
  if (!Number.isSafeInteger(input.portOffset) || input.portOffset < 0) {
    throw new Error(
      `port offset must be a non-negative integer, got ${input.portOffset}`,
    );
  }
  if (!/^[0-9a-f]{7,40}$/.test(input.baseCommit)) {
    throw new Error(`base commit must be a git SHA, got ${input.baseCommit}`);
  }
  const createdAt = (input.now ?? nowIso)();
  return {
    taskId: input.taskId,
    worktree: input.worktree,
    baseCommit: input.baseCommit,
    portOffset: input.portOffset,
    createdAt,
    status: "running",
    events: [
      {
        timestamp: createdAt,
        command: "task start",
        note: `created task state for ${input.taskId}`,
        exitCode: 0,
      },
    ],
    operations: [],
    assertions: [],
    cleanup: {
      result: "not-required",
      notes: "No dev server was started by this task; nothing to tear down.",
    },
    reproduction: {
      result: "inconclusive",
      notes:
        "The fdh local loop records verification; it does not reproduce a defect.",
    },
  };
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function assertArray(value: unknown, name: string): void {
  if (!Array.isArray(value)) {
    throw new Error(`invalid fdh task state: ${name} must be an array`);
  }
}

const EXTERNAL_VERIFICATIONS: ExternalVerification[] = [
  "build",
  "host-test",
  "browser",
  "other",
];

function assertString(value: unknown, name: string): string {
  if (typeof value !== "string" || value === "") {
    throw new Error(
      `invalid fdh task state: ${name} must be a non-empty string`,
    );
  }
  return value;
}

function assertExternal(value: unknown, index: number): void {
  const pointer = `operations[${index}].external`;
  if (!isRecord(value)) {
    throw new Error(`invalid fdh task state: ${pointer} must be an object`);
  }
  assertString(value.operator, `${pointer}.operator`);
  assertString(value.logRef, `${pointer}.logRef`);
  const sha = assertString(value.logSha256, `${pointer}.logSha256`);
  if (!/^[0-9a-f]{64}$/.test(sha)) {
    throw new Error(
      `invalid fdh task state: ${pointer}.logSha256 must be 64 hex`,
    );
  }
  if (!Number.isSafeInteger(value.bytes) || (value.bytes as number) < 0) {
    throw new Error(
      `invalid fdh task state: ${pointer}.bytes must be a non-negative integer`,
    );
  }
  assertString(value.attestedAt, `${pointer}.attestedAt`);
  if (
    value.verification !== undefined &&
    !EXTERNAL_VERIFICATIONS.includes(value.verification as ExternalVerification)
  ) {
    throw new Error(
      `invalid fdh task state: ${pointer}.verification must be one of ${
        EXTERNAL_VERIFICATIONS.join("|")
      }`,
    );
  }
}

function assertSetup(value: unknown): void {
  if (!isRecord(value)) {
    throw new Error("invalid fdh task state: setup must be an object");
  }
  if (!isRecord(value.guardEnv)) {
    throw new Error("invalid fdh task state: setup.guardEnv must be an object");
  }
  for (const [key, env] of Object.entries(value.guardEnv)) {
    if (typeof env !== "string") {
      throw new Error(
        `invalid fdh task state: setup.guardEnv.${key} must be a string`,
      );
    }
  }
  assertString(value.runtimeSource, "setup.runtimeSource");
  assertString(value.profilePath, "setup.profilePath");
  if (value.viteClearedAt !== null && typeof value.viteClearedAt !== "string") {
    throw new Error(
      "invalid fdh task state: setup.viteClearedAt must be a string or null",
    );
  }
  if (!Array.isArray(value.commands)) {
    throw new Error("invalid fdh task state: setup.commands must be an array");
  }
  for (const command of value.commands) {
    if (typeof command !== "string" || command === "") {
      throw new Error(
        "invalid fdh task state: setup.commands entries must be non-empty strings",
      );
    }
  }
}

/** Runtime shape check for state read back from disk. */
export function parseTaskState(value: unknown): FdhTaskState {
  if (!isRecord(value)) {
    throw new Error("invalid fdh task state: expected a JSON object");
  }
  const taskId = value.taskId;
  if (typeof taskId !== "string") {
    throw new Error("invalid fdh task state: taskId must be a string");
  }
  validateTaskId(taskId);
  for (
    const key of ["worktree", "baseCommit", "createdAt", "status"] as const
  ) {
    if (typeof value[key] !== "string" || value[key] === "") {
      throw new Error(`invalid fdh task state: ${key} must be a string`);
    }
  }
  if (
    !Number.isSafeInteger(value.portOffset) ||
    (value.portOffset as number) < 0
  ) {
    throw new Error(
      "invalid fdh task state: portOffset must be a non-negative integer",
    );
  }
  for (const key of ["events", "operations", "assertions"] as const) {
    assertArray(value[key], key);
  }
  for (const [index, operation] of (value.operations as unknown[]).entries()) {
    if (isRecord(operation) && operation.external !== undefined) {
      assertExternal(operation.external, index);
    }
  }
  if (value.setup !== undefined) {
    assertSetup(value.setup);
  }
  if (!isRecord(value.cleanup) || !isRecord(value.reproduction)) {
    throw new Error(
      "invalid fdh task state: cleanup and reproduction must be objects",
    );
  }
  return value as unknown as FdhTaskState;
}

export function taskStateExists(worktree: string, taskId: string): boolean {
  try {
    return Deno.statSync(taskStatePath(worktree, taskId)).isFile;
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return false;
    throw error;
  }
}

export function readTaskState(
  worktree: string,
  taskId: string,
): FdhTaskState {
  const file = taskStatePath(worktree, taskId);
  const text = Deno.readTextFileSync(file);
  try {
    return parseTaskState(JSON.parse(text));
  } catch (error) {
    throw new Error(
      `invalid fdh task state at ${file}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
}

/** Write the state atomically (tmp file + rename) so a crash cannot truncate it. */
export function writeTaskState(state: FdhTaskState): string {
  const file = taskStatePath(state.worktree, state.taskId);
  Deno.mkdirSync(path.dirname(file), { recursive: true });
  const tmp = `${file}.tmp-${Deno.pid}`;
  Deno.writeTextFileSync(tmp, `${JSON.stringify(state, null, 2)}\n`);
  Deno.renameSync(tmp, file);
  return file;
}

export function appendEvent(
  state: FdhTaskState,
  event: FdhEvent,
): FdhTaskState {
  state.events.push(event);
  return state;
}

export function recordOperation(
  state: FdhTaskState,
  operation: FdhOperation,
): FdhTaskState {
  state.operations.push(operation);
  return state;
}

export interface NewAssertion {
  name: string;
  expected: string;
  actual: string;
  passed: boolean;
  at?: string;
}

export function recordAssertion(
  state: FdhTaskState,
  assertion: NewAssertion,
): FdhAssertion {
  const entry: FdhAssertion = {
    name: assertion.name,
    expected: assertion.expected,
    actual: assertion.actual,
    passed: assertion.passed,
    at: assertion.at ?? nowIso(),
  };
  state.assertions.push(entry);
  return entry;
}

// ---------------------------------------------------------------------------
// git / worktree helpers
// ---------------------------------------------------------------------------

/** Runs `git <args>` in `cwd` and returns trimmed stdout, throwing on failure. */
export type GitRunner = (cwd: string, args: string[]) => Promise<string>;

export const defaultGitRunner: GitRunner = async (cwd, args) => {
  const output = await new Deno.Command("git", {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
    env: { LC_ALL: "C" },
  }).output();
  const stdout = new TextDecoder().decode(output.stdout).trim();
  if (!output.success) {
    const stderr = new TextDecoder().decode(output.stderr).trim();
    throw new Error(
      `git ${args.join(" ")} failed (exit ${output.code})` +
        (stderr ? `: ${stderr}` : ""),
    );
  }
  return stdout;
};

/** Resolve the absolute git work-tree root for `cwd`. */
export async function resolveGitRoot(
  cwd: string,
  git: GitRunner = defaultGitRunner,
): Promise<string> {
  const root = await git(cwd, ["rev-parse", "--show-toplevel"]);
  if (!root) {
    throw new Error(`not a git work tree: ${cwd}`);
  }
  return Deno.realPathSync(root);
}

/**
 * Require `dir` to be an existing absolute directory that *is* the root of a
 * git work tree (not a subdirectory of one).
 */
export async function requireWorktree(
  dir: string,
  git: GitRunner = defaultGitRunner,
): Promise<string> {
  if (!path.isAbsolute(dir)) {
    throw new Error(`--worktree must be an absolute path, got ${dir}`);
  }
  const stat = Deno.statSync(dir);
  if (!stat.isDirectory) {
    throw new Error(`--worktree is not a directory: ${dir}`);
  }
  const real = Deno.realPathSync(dir);
  const root = await resolveGitRoot(dir, git);
  if (root !== real) {
    throw new Error(
      `--worktree must be the git work-tree root: git reports ${root} for ${dir}`,
    );
  }
  return real;
}

export async function resolveCurrentWorktree(
  git: GitRunner = defaultGitRunner,
): Promise<string> {
  return await resolveGitRoot(Deno.cwd(), git);
}

/** Resolve `--base` (default `HEAD`) to a full commit SHA inside `worktree`. */
export async function resolveBaseCommit(
  worktree: string,
  base: string | undefined,
  git: GitRunner = defaultGitRunner,
): Promise<string> {
  const raw = base ?? "HEAD";
  let sha: string;
  try {
    sha = await git(worktree, ["rev-parse", "--verify", `${raw}^{commit}`]);
  } catch (error) {
    throw new Error(
      `base commit ${JSON.stringify(raw)} is not reachable in ${worktree}: ${
        error instanceof Error ? error.message : String(error)
      }`,
    );
  }
  if (!/^[0-9a-f]{7,40}$/.test(sha)) {
    throw new Error(`git returned an unexpected commit id: ${sha}`);
  }
  return sha;
}

/**
 * Resolve `--port-offset` (`<n|auto>` / omitted) through the Gate 2 shared
 * parser. Invalid values throw before anything is spawned.
 */
export async function resolvePortOffset(
  raw: string | number | undefined,
): Promise<number> {
  const spec = parsePortOffsetSpec(raw);
  return spec === "auto" ? await resolveAutoDevPortOffset() : spec;
}

// ---------------------------------------------------------------------------
// child-command helpers
// ---------------------------------------------------------------------------

export interface CommandResult {
  code: number;
  stdout: string;
  stderr: string;
  output: string;
}

export interface StreamOptions {
  onStdout?: (chunk: string) => void;
  onStderr?: (chunk: string) => void;
}

/** Runs a child command while streaming stdout/stderr, returning both. */
export type StreamRunner = (
  command: string,
  args: string[],
  cwd: string,
  options?: StreamOptions,
) => Promise<CommandResult>;

export const runStreamingCommand: StreamRunner = async (
  command,
  args,
  cwd,
  options = {},
) => {
  const child = new Deno.Command(command, {
    args,
    cwd,
    stdout: "piped",
    stderr: "piped",
  }).spawn();

  let stdout = "";
  let stderr = "";
  const pump = async (
    stream: ReadableStream<Uint8Array>,
    append: (text: string) => void,
    sink?: (chunk: string) => void,
  ): Promise<void> => {
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      const text = decoder.decode(chunk, { stream: true });
      if (text === "") continue;
      append(text);
      sink?.(text);
    }
    const tail = decoder.decode();
    if (tail !== "") {
      append(tail);
      sink?.(tail);
    }
  };

  await Promise.all([
    pump(child.stdout, (text) => stdout += text, options.onStdout),
    pump(child.stderr, (text) => stderr += text, options.onStderr),
  ]);
  const status = await child.status;
  return { code: status.code, stdout, stderr, output: stdout + stderr };
};

export interface RecordedCommandSpec {
  command: string;
  args: string[];
  cwd: string;
  displayCommand: string;
  /** When set, an exit-code-0 assertion is recorded under this name. */
  assertionName?: string;
  expected?: string;
  /** Max characters of output kept in the state operation (default 4000). */
  maxOutputChars?: number;
  stream?: StreamOptions;
}

export interface RecordedCommandResult extends CommandResult {
  operation: FdhOperation;
}

function tail(text: string, max: number): string {
  if (max === Number.POSITIVE_INFINITY) return text;
  return text.length <= max ? text : text.slice(text.length - max);
}

/**
 * Run a child command, append the event + operation (and optional assertion)
 * to the state, persist the state, and return the result.
 *
 * Non-idempotent external operations (push/PR) are intentionally absent: the
 * CLI only runs local commands.
 */
export async function runRecordedCommand(
  state: FdhTaskState,
  spec: RecordedCommandSpec,
  runner: StreamRunner = runStreamingCommand,
): Promise<RecordedCommandResult> {
  const startedAt = nowIso();
  const result = await runner(spec.command, spec.args, spec.cwd, spec.stream);
  const endedAt = nowIso();
  const operation: FdhOperation = {
    command: spec.command,
    args: spec.args,
    displayCommand: spec.displayCommand,
    cwd: spec.cwd,
    startedAt,
    endedAt,
    exitCode: result.code,
    output: tail(result.output, spec.maxOutputChars ?? 4000),
  };
  recordOperation(state, operation);
  appendEvent(state, {
    timestamp: endedAt,
    command: spec.displayCommand,
    note: `exit ${result.code}${
      result.output.trim() === "" ? "" : `: ${tail(result.output.trim(), 800)}`
    }`,
    exitCode: result.code,
  });
  if (spec.assertionName !== undefined) {
    recordAssertion(state, {
      name: spec.assertionName,
      expected: spec.expected ?? "exit code 0",
      actual: `exit code ${result.code}`,
      passed: result.code === 0,
      at: endedAt,
    });
  }
  writeTaskState(state);
  return { ...result, operation };
}

export interface ExternalOperationInput {
  /** Command line as the operator ran it, e.g. `deno task test:host`. */
  command: string;
  args?: string[];
  /** Absolute cwd; its real path must be inside `state.worktree`. */
  cwd: string;
  exitCode: number;
  /** Observation file path (absolute or relative to the worktree). */
  observationFile: string;
  operator: string;
  verification?: ExternalVerification;
  /** Attestation time; defaults to now. */
  attestedAt?: string;
  /** Max characters of the observation kept in the state operation (default 4000). */
  maxOutputChars?: number;
  now?: () => string;
}

export interface ExternalOperationResult {
  operation: FdhOperation;
  /** Recorded only when `verification` is set. */
  assertion?: FdhAssertion;
}

/** Absolute path of an observation file, constrained to `<worktree>/_dist/fdh/observations`. */
export function resolveObservationPath(
  state: FdhTaskState,
  raw: string,
): string {
  const root = path.resolve(fdhDir(state.worktree), "observations");
  const resolved = path.isAbsolute(raw)
    ? path.resolve(raw)
    : path.resolve(state.worktree, raw);
  const relative = path.relative(root, resolved);
  if (
    relative === "" || relative.startsWith("..") || path.isAbsolute(relative)
  ) {
    throw new Error(
      `observation file must stay under ${root}, got ${resolved}`,
    );
  }
  // Reject a symlink under the observations root that resolves elsewhere.
  let realRoot: string;
  try {
    realRoot = Deno.realPathSync(root);
  } catch {
    realRoot = root;
  }
  let realResolved: string;
  try {
    realResolved = Deno.realPathSync(resolved);
  } catch {
    return resolved;
  }
  const realRelative = path.relative(realRoot, realResolved);
  if (realRelative.startsWith("..") || path.isAbsolute(realRelative)) {
    throw new Error(
      `observation file must resolve under ${root}, got ${realResolved}`,
    );
  }
  return resolved;
}

async function sha256Hex(bytes: Uint8Array): Promise<string> {
  const input = new Uint8Array(bytes.length);
  input.set(bytes);
  const digest = await crypto.subtle.digest("SHA-256", input);
  return encodeHex(new Uint8Array(digest));
}

/**
 * Record a command that was run outside the fdh CLI (IMP-2 A案).
 *
 * The exit code and observation file are operator-attested; fdh pins the
 * observation bytes with SHA-256 and appends the operation/event to the task
 * state. When `verification` is set, an `external-<kind>-exit-code` assertion
 * is also recorded so the EvidenceManifest state reflects a failure.
 */
export async function recordExternalOperation(
  state: FdhTaskState,
  input: ExternalOperationInput,
): Promise<ExternalOperationResult> {
  if (!Number.isSafeInteger(input.exitCode)) {
    throw new Error(
      `--exit-code must be an integer, got ${JSON.stringify(input.exitCode)}`,
    );
  }
  const operator = input.operator.trim();
  if (operator === "") {
    throw new Error("--operator must not be empty");
  }
  if (
    input.verification !== undefined &&
    !EXTERNAL_VERIFICATIONS.includes(input.verification)
  ) {
    throw new Error(
      `--verification must be one of ${EXTERNAL_VERIFICATIONS.join("|")}, got ${
        JSON.stringify(input.verification)
      }`,
    );
  }
  if (!path.isAbsolute(input.cwd)) {
    throw new Error(`--cwd must be an absolute path, got ${input.cwd}`);
  }
  const worktree = Deno.realPathSync(state.worktree);
  const cwd = Deno.realPathSync(input.cwd);
  const cwdRelative = path.relative(worktree, cwd);
  if (cwdRelative.startsWith("..") || path.isAbsolute(cwdRelative)) {
    throw new Error(
      `--cwd must be inside the task worktree ${worktree}, got ${cwd}`,
    );
  }

  const observationPath = resolveObservationPath(state, input.observationFile);
  const stat = Deno.statSync(observationPath);
  if (!stat.isFile) {
    throw new Error(
      `observation file is not a regular file: ${observationPath}`,
    );
  }
  const bytes = Deno.readFileSync(observationPath);
  if (bytes.length > MAX_OBSERVATION_BYTES) {
    throw new Error(
      `observation file is ${bytes.length} bytes; the limit is ${MAX_OBSERVATION_BYTES}`,
    );
  }

  const now = (input.now ?? nowIso)();
  const attestedAt = input.attestedAt ?? now;
  if (attestedAt.trim() === "") {
    throw new Error("--attested-at must not be empty");
  }
  // Attested time orders external records against other evidence, so it must
  // be a real ISO 8601 instant and not in the future.
  const attestedMs = Date.parse(attestedAt);
  if (!ISO_INSTANT_RE.test(attestedAt) || !Number.isFinite(attestedMs)) {
    throw new Error(
      `--attested-at must be an ISO 8601 date-time with a zone, for example 2026-11-09T05:00:00Z or 2026-11-09T14:00:00+09:00; got ${
        JSON.stringify(attestedAt)
      }`,
    );
  }
  if (attestedMs > Date.parse(now) + MAX_ATTESTED_FUTURE_SKEW_MS) {
    throw new Error(
      `--attested-at must not be in the future, got ${attestedAt} (now ${now})`,
    );
  }
  const text = new TextDecoder().decode(bytes);
  const operation: FdhOperation = {
    command: input.command,
    args: input.args ?? [],
    displayCommand: input.command,
    cwd,
    startedAt: attestedAt,
    endedAt: attestedAt,
    exitCode: input.exitCode,
    output: tail(text, input.maxOutputChars ?? 4000),
    external: {
      operator,
      // Always '/'-separated so the recorded evidence reads the same on every
      // OS; path.resolve accepts it on Windows too (W-01).
      logRef: path.relative(worktree, observationPath)
        .split(path.SEPARATOR)
        .join("/"),
      logSha256: await sha256Hex(bytes),
      bytes: bytes.length,
      attestedAt,
      ...(input.verification !== undefined
        ? { verification: input.verification }
        : {}),
    },
  };
  recordOperation(state, operation);
  appendEvent(state, {
    timestamp: attestedAt,
    command: `external ${input.command}`,
    note: `external record by ${operator}; exit ${input.exitCode}${
      text.trim() === "" ? "" : `: ${tail(text.trim(), 800)}`
    }`,
    exitCode: input.exitCode,
  });
  let assertion: FdhAssertion | undefined;
  if (input.verification !== undefined) {
    assertion = recordAssertion(state, {
      name: `external-${input.verification}-exit-code`,
      expected: "exit code 0",
      actual: `exit code ${input.exitCode}`,
      passed: input.exitCode === 0,
      at: attestedAt,
    });
  }
  writeTaskState(state);
  return { operation, assertion };
}

// ---------------------------------------------------------------------------
// resume
// ---------------------------------------------------------------------------

export interface ResumeReport {
  taskId: string;
  worktree: string;
  baseCommit: string;
  portOffset: number;
  status: FdhTaskStatus;
  statePath: string;
  /** What resume re-verified locally. */
  restored: string[];
  /** What resume found stale/unsafe; nothing external is rerun. */
  stale: string[];
}

export interface ResumeOptions {
  taskId: string;
  /** Defaults to the git root of the current working directory. */
  worktree?: string;
  git?: GitRunner;
  probePort?: DevPortProbe;
  now?: () => string;
  /**
   * Called with the reopened state just before the resume event is appended.
   * Throwing here aborts the resume without recording a successful event
   * (used to validate the quota ledger before the state write).
   */
  validate?: (state: FdhTaskState) => void;
}

/**
 * Local-only resume: re-open the state, verify the worktree/base/offset are
 * still valid, and probe the recorded ports. It never reruns push/PR or any
 * other non-idempotent external operation.
 */
export async function resumeTaskState(
  options: ResumeOptions,
): Promise<{ state: FdhTaskState; report: ResumeReport }> {
  validateTaskId(options.taskId);
  const git = options.git ?? defaultGitRunner;
  const probePort = options.probePort ?? isTcpPortInUse;

  const worktree = options.worktree !== undefined
    ? await requireWorktree(options.worktree, git)
    : await resolveCurrentWorktree(git);

  const state = readTaskState(worktree, options.taskId);

  let recordedWorktree: string;
  try {
    recordedWorktree = Deno.realPathSync(state.worktree);
  } catch {
    throw new Error(
      `recorded worktree no longer exists: ${state.worktree}`,
    );
  }
  if (recordedWorktree !== worktree) {
    throw new Error(
      `task ${state.taskId} belongs to ${recordedWorktree}, not ${worktree}`,
    );
  }

  try {
    await git(worktree, ["cat-file", "-e", `${state.baseCommit}^{commit}`]);
  } catch {
    throw new Error(
      `base commit ${state.baseCommit} is no longer reachable in ${worktree}`,
    );
  }

  const parsedOffset = parsePortOffsetSpec(state.portOffset);
  if (parsedOffset !== state.portOffset) {
    throw new Error(
      `recorded port offset ${state.portOffset} is not admissible`,
    );
  }

  const restored = ["state-file", "worktree", "base-commit", "port-offset"];
  const stale: string[] = [];
  for (const port of launchPortsForOffset(state.portOffset)) {
    if (await probePort(port)) {
      stale.push(`port ${port} is already in use`);
    }
  }
  if (stale.length > 0) {
    stale.push(
      "another dev server may still be running; run 'fdh dev down' before 'fdh dev up'",
    );
  }

  options.validate?.(state);

  appendEvent(state, {
    timestamp: (options.now ?? nowIso)(),
    command: "task resume",
    note: JSON.stringify({ restored, stale }),
    exitCode: 0,
  });
  writeTaskState(state);

  return {
    state,
    report: {
      taskId: state.taskId,
      worktree: state.worktree,
      baseCommit: state.baseCommit,
      portOffset: state.portOffset,
      status: state.status,
      statePath: taskStatePath(worktree, state.taskId),
      restored,
      stale,
    },
  };
}
