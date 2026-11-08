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
