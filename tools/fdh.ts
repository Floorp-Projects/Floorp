#!/usr/bin/env -S deno run -A
// SPDX-License-Identifier: MPL-2.0

/**
 * `fdh` — thin CLI for the FDH Gate 3 local loop (KAN-113).
 *
 * This is not an agent loop: it has no interactive conversation, no LLM calls
 * and no GitHub/network operations. It only records task state and runs the
 * existing Floorp tools (dev-tool, feles-build, host tests) so an already
 * running agent (DSH/Codex/Claude) can drive: task -> edit -> build -> test ->
 * browser confirmation -> evidence.
 */

import * as path from "@std/path";
import { isTcpPortInUse, parsePortOffsetSpec } from "./src/port_offset.ts";
import {
  appendEvent,
  createTaskState,
  fdhDir,
  nowIso,
  readTaskState,
  recordAssertion,
  requireWorktree,
  resolveBaseCommit,
  resolveCurrentWorktree,
  resolvePortOffset,
  resumeTaskState,
  runRecordedCommand,
  runStreamingCommand,
  taskStateExists,
  taskStatePath,
  validateTaskId,
  writeTaskState,
} from "./src/fdh_task.ts";
import {
  buildEvidenceManifest,
  EvidenceValidationError,
  validateEvidenceManifest,
  writeEvidenceManifest,
} from "./src/fdh_evidence.ts";
import {
  approveQuota,
  claimQuota,
  pendingQuota,
  quotaLedgerExists,
  readQuotaLedger,
  releaseQuota,
  resumeQuota,
  stopQuota,
  summarizeQuota,
  writeQuotaLedger,
} from "./src/fdh_quota.ts";
import type { PendingExternalOp, QuotaStopReason } from "./src/fdh_quota.ts";
import type { FdhTaskState } from "./src/fdh_task.ts";
import type { ValidationError } from "./qa3/evidence/types.ts";

const HELP = `
Usage: deno run -A tools/fdh.ts <command> [options]

Commands:
  task start --task-id <id> --worktree <abs-dir> [--port-offset <n|auto>] [--base <sha>]
                            Create the local task state under
                            <worktree>/_dist/fdh/tasks/<id>.json
  task resume --task-id <id> [--worktree <abs-dir>]
                            Re-open the state and verify worktree, base
                            commit, port offset and recorded ports (local only)
  dev up --task-id <id>     Start dev-tool for the recorded worktree/offset
  dev down --task-id <id>   Stop dev-tool for the recorded worktree
  verify build --task-id <id>
                            Run "deno task feles-build build --phase after-mach"
  verify test --task-id <id>
                            Run "deno task test:host"
  verify browser --task-id <id> [--eval <js>] [--expect <text>]
                            Check dev-tool browser status; --eval output is
                            recorded verbatim, and --expect turns it into an
                            honest contains-check assertion
  evidence --task-id <id> --out <path>
                            Build + validate the EvidenceManifest v0
                            (--out must stay under <worktree>/_dist/fdh)
  quota claim --task-id <id> --agent <name>
                            Assign the task to an agent; a second active holder
                            is rejected (no double assignment)
  quota stop --task-id <id> --agent <name> --reason <quota|infra|manual> [--note <text>]
                            Stop a task for a quota/infra/manual reason
  quota resume --task-id <id> --agent <name> [--note <text>]
                            Resume a stopped task and re-assign it
  quota pending --task-id <id> --op <push|pr|release|external> [--note <text>]
                            Record a non-idempotent external op awaiting approval
  quota approve --task-id <id> [--note <text>]
                            Approve and clear pending external operations
  quota release --task-id <id> --agent <name>
                            Release the active holder without stopping
  quota status --task-id <id>
                            Print the local quota/assignment ledger
  help, --help              Show this help

State: <worktree>/_dist/fdh/tasks/<id>.json (git-ignored via _dist/).
Quota ledger: <worktree>/_dist/fdh/quota/<id>.json (local-only).
Errors go to stderr and exit 1. The CLI never pushes, opens a PR, or calls a
shared service; pending push/PR/release ops wait for 'quota approve'.

State: <worktree>/_dist/fdh/tasks/<id>.json (git-ignored via _dist/).
Errors go to stderr and exit 1. The CLI never pushes, opens a PR, or calls a
shared service.
`.trim();

interface ParsedArgs {
  positional: string[];
  options: Map<string, string>;
  flags: Set<string>;
}

function parseArgv(argv: string[]): ParsedArgs {
  const positional: string[] = [];
  const options = new Map<string, string>();
  const flags = new Set<string>();

  for (let index = 0; index < argv.length; index++) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") {
      flags.add("help");
      continue;
    }
    if (arg.startsWith("--")) {
      const body = arg.slice(2);
      if (body === "") {
        throw new Error("unexpected argument '--'");
      }
      const equals = body.indexOf("=");
      if (equals >= 0) {
        options.set(body.slice(0, equals), body.slice(equals + 1));
        continue;
      }
      const value = argv[index + 1];
      if (value === undefined || value.startsWith("--")) {
        throw new Error(`--${body} requires a value`);
      }
      options.set(body, value);
      index++;
      continue;
    }
    if (arg.startsWith("-") && arg !== "-") {
      throw new Error(`unknown option '${arg}'`);
    }
    positional.push(arg);
  }

  return { positional, options, flags };
}

function assertAllowedOptions(
  parsed: ParsedArgs,
  allowed: readonly string[],
): void {
  const allowedSet = new Set(allowed);
  for (const name of parsed.options.keys()) {
    if (!allowedSet.has(name)) {
      throw new Error(`unknown option --${name}`);
    }
  }
}

function requireOption(parsed: ParsedArgs, name: string): string {
  const value = parsed.options.get(name);
  if (value === undefined) {
    throw new Error(`--${name} is required`);
  }
  if (value === "") {
    throw new Error(`--${name} must not be empty`);
  }
  return value;
}

function hasOption(parsed: ParsedArgs, name: string): boolean {
  return parsed.options.has(name);
}

function assertNoPositional(parsed: ParsedArgs, expected: number): void {
  if (parsed.positional.length !== expected) {
    throw new Error(
      `unexpected arguments: ${parsed.positional.slice(expected).join(" ")}`,
    );
  }
}

function printValidationErrors(errors: ValidationError[]): void {
  for (const error of errors) {
    console.error(
      `[fdh] evidence-invalid ${error.code} ${error.path}: ${error.message}`,
    );
  }
}

function streamToConsole(): {
  onStdout: (chunk: string) => void;
  onStderr: (chunk: string) => void;
} {
  const encoder = new TextEncoder();
  return {
    onStdout: (chunk) => Deno.stdout.writeSync(encoder.encode(chunk)),
    onStderr: (chunk) => Deno.stderr.writeSync(encoder.encode(chunk)),
  };
}

function summaryOf(state: FdhTaskState) {
  return {
    taskId: state.taskId,
    worktree: state.worktree,
    baseCommit: state.baseCommit,
    portOffset: state.portOffset,
    status: state.status,
    statePath: taskStatePath(state.worktree, state.taskId),
  };
}

async function cmdTaskStart(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "worktree", "port-offset", "base"]);

  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const worktreeArg = requireOption(parsed, "worktree");
  if (!path.isAbsolute(worktreeArg)) {
    throw new Error(`--worktree must be an absolute path, got ${worktreeArg}`);
  }
  if (
    hasOption(parsed, "base") && requireOption(parsed, "base").trim() === ""
  ) {
    throw new Error("--base must not be empty");
  }

  // Parse/validate the offset before any git or child process runs.
  const portOffset = await resolvePortOffset(
    hasOption(parsed, "port-offset")
      ? requireOption(parsed, "port-offset")
      : undefined,
  );

  const worktree = await requireWorktree(worktreeArg);
  const baseCommit = await resolveBaseCommit(
    worktree,
    hasOption(parsed, "base") ? requireOption(parsed, "base") : undefined,
  );

  if (taskStateExists(worktree, taskId)) {
    throw new Error(
      `task '${taskId}' already exists at ${
        taskStatePath(worktree, taskId)
      }; use 'fdh task resume --task-id ${taskId}'`,
    );
  }

  const state = createTaskState({ taskId, worktree, baseCommit, portOffset });
  writeTaskState(state);
  console.log(JSON.stringify(summaryOf(state)));
}

async function cmdTaskResume(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "worktree"]);

  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const worktree = hasOption(parsed, "worktree")
    ? requireOption(parsed, "worktree")
    : undefined;

  const { state, report } = await resumeTaskState({
    taskId,
    worktree,
    probePort: isTcpPortInUse,
  });
  // Resume never reruns push/PR/release; it reports the pending approvals so the
  // agent waits for an explicit 'fdh quota approve'.
  const approvalPending = quotaLedgerExists(state.worktree, taskId)
    ? readQuotaLedger(state.worktree, taskId).approvalPending
    : [];
  console.log(JSON.stringify({ ...report, approvalPending }));
}

async function cmdQuota(
  parsed: ParsedArgs,
  sub:
    | "claim"
    | "stop"
    | "resume"
    | "pending"
    | "approve"
    | "release"
    | "status",
): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "agent", "reason", "op", "note"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);
  const ledger = readQuotaLedger(state.worktree, taskId, true);
  const note = hasOption(parsed, "note") ? requireOption(parsed, "note") : "";

  switch (sub) {
    case "claim":
      claimQuota(ledger, requireOption(parsed, "agent"));
      break;
    case "stop": {
      const reason = requireOption(parsed, "reason") as QuotaStopReason;
      stopQuota(ledger, requireOption(parsed, "agent"), reason, note);
      break;
    }
    case "resume":
      resumeQuota(ledger, requireOption(parsed, "agent"), note);
      break;
    case "pending":
      pendingQuota(
        ledger,
        requireOption(parsed, "op") as PendingExternalOp,
        note,
      );
      break;
    case "approve":
      approveQuota(ledger, note);
      break;
    case "release":
      releaseQuota(ledger, requireOption(parsed, "agent"));
      break;
    case "status":
      console.log(JSON.stringify(summarizeQuota(ledger)));
      return;
  }

  writeQuotaLedger(ledger);
  const summary = summarizeQuota(ledger);
  appendEvent(state, {
    timestamp: nowIso(),
    command: `quota ${sub}`,
    note: JSON.stringify(summary),
    exitCode: 0,
  });
  writeTaskState(state);
  console.log(JSON.stringify(summary));
}

async function loadStateForCwd(taskId: string): Promise<FdhTaskState> {
  const worktree = await resolveCurrentWorktree();
  return readTaskState(worktree, taskId);
}

async function runDevCommand(
  parsed: ParsedArgs,
  action: "up" | "down",
): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  // dev up must reuse the recorded integer offset; never re-resolve "auto".
  const offset = parsePortOffsetSpec(state.portOffset);
  if (offset === "auto") {
    throw new Error("recorded port offset must be an integer, not 'auto'");
  }

  const displayCommand = action === "up"
    ? `deno run -A tools/dev-tool.ts start --port-offset ${offset}`
    : "deno run -A tools/dev-tool.ts stop";
  const args = action === "up"
    ? [
      "run",
      "-A",
      "tools/dev-tool.ts",
      "start",
      "--port-offset",
      String(offset),
    ]
    : ["run", "-A", "tools/dev-tool.ts", "stop"];

  console.log(`[fdh] ${displayCommand}`);
  const result = await runRecordedCommand(
    state,
    {
      command: Deno.execPath(),
      args,
      cwd: state.worktree,
      displayCommand,
      stream: streamToConsole(),
    },
    runStreamingCommand,
  );

  if (action === "up") {
    state.cleanup = {
      result: result.code === 0 ? "partial" : "failed",
      notes: result.code === 0
        ? "Dev server started; run 'fdh dev down' to stop it."
        : `dev up failed with exit ${result.code}; inspect events[].`,
      details: ["dev-tool start"],
    };
  } else {
    state.cleanup = {
      result: result.code === 0 ? "completed" : "failed",
      notes: result.code === 0
        ? "Dev server stopped by 'fdh dev down'."
        : `dev down failed with exit ${result.code}; the dev server may still run.`,
      details: ["dev-tool stop"],
    };
  }
  appendEvent(state, {
    timestamp: nowIso(),
    command: "cleanup",
    note: `cleanup ${action}: ${state.cleanup.result}`,
    exitCode: result.code,
  });
  writeTaskState(state);

  if (result.code !== 0) {
    throw new Error(`${displayCommand} failed with exit ${result.code}`);
  }
}

async function runVerify(
  parsed: ParsedArgs,
  kind: "build" | "test",
): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  const spec = kind === "build"
    ? {
      displayCommand: "deno task feles-build build --phase after-mach",
      args: ["task", "feles-build", "build", "--phase", "after-mach"],
      assertionName: "build-exit-code",
      expected: "exit code 0",
    }
    : {
      displayCommand: "deno task test:host",
      args: ["task", "test:host"],
      assertionName: "host-test-exit-code",
      expected: "exit code 0",
    };

  console.log(`[fdh] ${spec.displayCommand}`);
  const result = await runRecordedCommand(
    state,
    {
      command: Deno.execPath(),
      args: spec.args,
      cwd: state.worktree,
      displayCommand: spec.displayCommand,
      assertionName: spec.assertionName,
      expected: spec.expected,
      stream: streamToConsole(),
    },
    runStreamingCommand,
  );

  if (result.code !== 0) {
    throw new Error(`${spec.displayCommand} failed with exit ${result.code}`);
  }
}

function firstLine(text: string, max = 500): string {
  const line = text.trim().split(/\r?\n/)[0] ?? "";
  return line.length <= max ? line : `${line.slice(0, max)}…`;
}

async function cmdVerifyBrowser(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "eval", "expect"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const evalScript = hasOption(parsed, "eval")
    ? requireOption(parsed, "eval")
    : undefined;
  const expectText = hasOption(parsed, "expect")
    ? requireOption(parsed, "expect")
    : undefined;
  if (expectText !== undefined && evalScript === undefined) {
    throw new Error(
      "--expect requires --eval (the expected text is compared to eval output)",
    );
  }
  const state = await loadStateForCwd(taskId);

  const statusDisplay = "deno run -A tools/dev-tool.ts status";
  const status = await runRecordedCommand(
    state,
    {
      command: Deno.execPath(),
      args: ["run", "-A", "tools/dev-tool.ts", "status"],
      cwd: state.worktree,
      displayCommand: statusDisplay,
      stream: streamToConsole(),
    },
    runStreamingCommand,
  );

  const connected = status.code === 0 &&
    /Connected to browser\./.test(status.output);
  recordAssertion(state, {
    name: "browser-connected",
    expected: "Connected to browser.",
    actual: connected
      ? "Connected to browser."
      : status.output.trim() === ""
      ? `exit ${status.code}; no output`
      : firstLine(status.output),
    passed: connected,
  });
  writeTaskState(state);

  if (!connected) {
    throw new Error(
      "browser is not connected; record UNVERIFIED in the evidence limitations and retry after 'fdh dev up' succeeds",
    );
  }

  if (evalScript === undefined) return;
  if (evalScript.trim() === "") {
    throw new Error("--eval must not be empty");
  }

  const evalDisplay = `deno run -A tools/dev-tool.ts eval ${evalScript}`;
  const evaluated = await runRecordedCommand(
    state,
    {
      command: Deno.execPath(),
      args: ["run", "-A", "tools/dev-tool.ts", "eval", evalScript],
      cwd: state.worktree,
      displayCommand: evalDisplay,
      maxOutputChars: Number.POSITIVE_INFINITY,
      stream: streamToConsole(),
    },
    runStreamingCommand,
  );
  // The eval output is always recorded verbatim. Without --expect it is never
  // turned into a passing assertion; with --expect, the recorded assertion is
  // an honest contains-check of the actual output.
  appendEvent(state, {
    timestamp: nowIso(),
    command: "browser eval output (verbatim)",
    note: evaluated.output,
    exitCode: evaluated.code,
  });
  if (expectText !== undefined) {
    const passed = evaluated.code === 0 &&
      evaluated.output.includes(expectText);
    recordAssertion(state, {
      name: "browser-eval-contains",
      expected: `eval output contains ${JSON.stringify(expectText)}`,
      actual: firstLine(evaluated.output),
      passed,
    });
  }
  writeTaskState(state);

  if (evaluated.code !== 0) {
    throw new Error(`browser eval failed with exit ${evaluated.code}`);
  }
  if (expectText !== undefined && !evaluated.output.includes(expectText)) {
    throw new Error(
      `browser eval output did not contain ${JSON.stringify(expectText)}`,
    );
  }
}

function resolveEvidenceOut(state: FdhTaskState, raw: string): string {
  const root = path.resolve(fdhDir(state.worktree));
  const resolved = path.isAbsolute(raw)
    ? path.resolve(raw)
    : path.resolve(state.worktree, raw);
  const relative = path.relative(root, resolved);
  if (
    relative === "" || relative.startsWith("..") || path.isAbsolute(relative)
  ) {
    throw new Error(
      `--out must be inside ${root} (the fdh evidence root), got ${resolved}`,
    );
  }
  if (relative.startsWith(path.join("tasks"))) {
    throw new Error(
      `--out must not overwrite the task state under ${
        path.join(root, "tasks")
      }, got ${resolved}`,
    );
  }
  return resolved;
}

async function cmdEvidence(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 1);
  assertAllowedOptions(parsed, ["task-id", "out"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);
  const outPath = resolveEvidenceOut(state, requireOption(parsed, "out"));

  appendEvent(state, {
    timestamp: nowIso(),
    command: "evidence",
    note: `building manifest at ${outPath}`,
    exitCode: 0,
  });
  writeTaskState(state);

  let manifest;
  try {
    manifest = await buildEvidenceManifest(state);
  } catch (error) {
    if (error instanceof EvidenceValidationError) {
      printValidationErrors(error.errors);
      Deno.exit(1);
    }
    throw error;
  }

  const errors = validateEvidenceManifest(manifest);
  if (errors.length > 0) {
    printValidationErrors(errors);
    Deno.exit(1);
  }

  const written = await writeEvidenceManifest(outPath, manifest);
  console.log(JSON.stringify({
    taskId: state.taskId,
    out: written.path,
    bytes: written.bytes,
    state: manifest.envelope.taskStatus.state,
    assertions: manifest.assertions.length,
    limitations: manifest.limitations.length,
  }));
}

async function main(argv: string[]): Promise<void> {
  const parsed = parseArgv(argv);
  if (
    parsed.flags.has("help") || parsed.positional.length === 0 ||
    parsed.positional[0] === "help"
  ) {
    console.log(HELP);
    return;
  }

  const [group, sub] = parsed.positional;
  switch (group) {
    case "task":
      if (sub === "start") return await cmdTaskStart(parsed);
      if (sub === "resume") return await cmdTaskResume(parsed);
      throw new Error(`unknown task command ${JSON.stringify(sub ?? "")}`);
    case "dev":
      if (sub === "up") return await runDevCommand(parsed, "up");
      if (sub === "down") return await runDevCommand(parsed, "down");
      throw new Error(`unknown dev command ${JSON.stringify(sub ?? "")}`);
    case "verify":
      if (sub === "build") return await runVerify(parsed, "build");
      if (sub === "test") return await runVerify(parsed, "test");
      if (sub === "browser") return await cmdVerifyBrowser(parsed);
      throw new Error(`unknown verify command ${JSON.stringify(sub ?? "")}`);
    case "evidence":
      assertNoPositional(parsed, 1);
      return await cmdEvidence(parsed);
    case "quota":
      if (
        sub === "claim" || sub === "stop" || sub === "resume" ||
        sub === "pending" || sub === "approve" || sub === "release" ||
        sub === "status"
      ) {
        return await cmdQuota(parsed, sub);
      }
      throw new Error(`unknown quota command ${JSON.stringify(sub ?? "")}`);
    default:
      throw new Error(`unknown command ${JSON.stringify(group)}`);
  }
}

main(Deno.args)
  .then(() => Deno.exit(0))
  .catch((error: unknown) => {
    console.error(
      `[fdh] ${error instanceof Error ? error.message : String(error)}`,
    );
    Deno.exit(1);
  });
