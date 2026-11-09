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
  readTaskStateFor,
  recordAssertion,
  recordExternalOperation,
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
  buildVerifyCommands,
  guardFailedBuildEventCommand,
  parseBuildPhase,
  refusedBuildEventCommand,
} from "./src/fdh_verify.ts";
import {
  assertSourceBuildAllowed,
  DevGuardRefusalError,
} from "./src/fdh_dev_guard.ts";
import {
  FDH_REVIEW_RECEIPT_MARKER,
  FDH_REVIEW_RECEIPT_SCHEMA_VERSION,
  receiptFileName,
  REVIEW_TIERS,
  REVIEW_VERDICTS,
  validateReceipt,
} from "./src/fdh_review.ts";
import type {
  ReviewReceipt,
  ReviewTier,
  ReviewVerdict,
} from "./src/fdh_review.ts";
// setup.ts reaches npm:fflate, so it is loaded lazily in `task prepare` after
// prepareDeps; a static import would break every command in a fresh worktree.
import { prepareDeps } from "./src/setup_deps.ts";
import type { SetupRecord } from "./src/setup.ts";
import {
  approveQuota,
  claimQuota,
  pendingQuota,
  quotaLedgerExists,
  quotaLedgerPath,
  readQuotaLedger,
  releaseQuota,
  resumeQuota,
  stopQuota,
  summarizeQuota,
  writeQuotaLedger,
} from "./src/fdh_quota.ts";
import type {
  PendingApproval,
  PendingExternalOp,
  QuotaStopReason,
} from "./src/fdh_quota.ts";
import {
  claimGuiLease,
  endGuiLease,
  guiLeasePathForWorktree,
  readGuiLease,
  summarizeGuiLease,
  writeGuiLease,
} from "./src/fdh_gui.ts";
import type { GuiLeaseEndReason } from "./src/fdh_gui.ts";
import { lockPathFor, withFileLock } from "./src/fdh_lock.ts";
import {
  assertNoSecretReferences,
  checkTaskSecrets,
} from "./src/fdh_secrets.ts";
import type { ExternalVerification, FdhTaskState } from "./src/fdh_task.ts";
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
  task prepare --task-id <id> [--runtime-source <abs-dir>]
                            Idempotent setup: deno install if fflate is missing,
                            copy the Debug runtime into _dist/bin (optional
                            registry source), relink the devdir mounts,
                            wipe the copied sessionstore and node_modules/.vite,
                            then record setup.guardEnv for 'dev up'
  dev up --task-id <id>     Start dev-tool for the recorded worktree/offset
                            (setup.guardEnv is passed to the child process)
  dev down --task-id <id>   Stop dev-tool for the recorded worktree
  verify build --task-id <id> [--phase <all|before-mach|after-mach>]
                            Run the source compile (before-mach) then the XHTML
                            inject (after-mach); default all. --phase after-mach
                            keeps the legacy inject-only fast path, and --phase
                            before-mach skips the Runtime-dependent inject.
                            all/before-mach are refused while this worktree's
                            dev server runs: 'dev down', build, then 'dev up'.
  verify test --task-id <id>
                            Run "deno task test:host"
  verify browser --task-id <id> [--eval <js>] [--expect <text>]
                            Check dev-tool browser status; --eval output is
                            recorded verbatim, and --expect turns it into an
                            honest contains-check assertion
  evidence --task-id <id> --out <path>
                            Build + validate the EvidenceManifest v0
                            (--out must stay under <worktree>/_dist/fdh)
  record --task-id <id> --command <text> --exit-code <n>
         --observation-file <path> --operator <name>
         [--cwd <abs-dir>] [--verification <build|host-test|browser|other>]
         [--attested-at <iso>]
                            Record a command run outside fdh: pin the observation
                            file's sha256/bytes and, with --verification, add an
                            external-<kind>-exit-code assertion
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
  gui claim --task-id <id> --agent <name>
                            Acquire the shared GUI-desktop lease (one holder)
  gui end --task-id <id> --agent <name> --reason <ended|cancelled> [--note <text>]
                            End or cancel the lease; only the holder may release it
  gui status --task-id <id>
                            Print the shared GUI-desktop lease
  secrets check --task-id <id>
                            Deny evidence records that reference repository secrets
  secrets status
                            Print the secret-reference denial policy
  review record --task-id <id> --reviewer <id> --model <id> --commit <sha>
                --verdict <approve|approve_with_notes|request_changes>
                --scope <text> --tier <full_read|evidence_only|contract_spot>
                [--blocking <true|false>]
                            Write a review receipt under
                            <worktree>/_dist/fdh/reviews/ and print the PR
                            comment marker; post it with 'gh pr comment'.
  help, --help              Show this help

State: <worktree>/_dist/fdh/tasks/<id>.json (git-ignored via _dist/).
Quota ledger: <worktree>/_dist/fdh/quota/<id>.json (local-only).
Errors go to stderr and exit 1. The CLI never pushes, opens a PR, or calls a
shared service; pending push/PR/release ops wait for 'quota approve'.
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

  // Validate the quota ledger before the resume event is recorded, so a broken
  // ledger cannot leave a successful resume in the task state.
  let approvalPending: PendingApproval[] = [];
  const { report } = await resumeTaskState({
    taskId,
    worktree,
    probePort: isTcpPortInUse,
    validate: (resumedState) => {
      approvalPending = quotaLedgerExists(resumedState.worktree, taskId)
        ? readQuotaLedger(resumedState.worktree, taskId).approvalPending
        : [];
    },
  });
  // Resume never reruns push/PR/release; it reports the pending approvals so the
  // agent waits for an explicit 'fdh quota approve'.
  console.log(JSON.stringify({ ...report, approvalPending }));
}

/** Set on the child `task prepare` that runs after the parent's deno install. */
const DEPS_INSTALLED_ENV = "FDH_PREPARE_DEPS_INSTALLED";

/**
 * Idempotent setup preparation for one worktree (IMP-5).
 *
 * Runs deps/runtime/profile/vite steps in order, maps the SetupRecord onto the
 * task-state `setup` block, and records the event. The record keeps the
 * previous registry source and vite-clear time when a later replay is a no-op,
 * so a resume does not lose how the runtime was prepared.
 */
async function cmdTaskPrepare(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "runtime-source"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  const runtimeSource = hasOption(parsed, "runtime-source")
    ? requireOption(parsed, "runtime-source")
    : undefined;
  if (runtimeSource !== undefined && !path.isAbsolute(runtimeSource)) {
    throw new Error(
      `--runtime-source must be an absolute path, got ${runtimeSource}`,
    );
  }

  // Install npm deps before loading setup.ts: its import graph needs fflate.
  // Deno fixes the npm resolution of a process at startup, so a process that
  // has just run `deno install` still cannot load setup.ts. In that case the
  // rest of the prepare runs in a fresh child with the same arguments (W-05).
  // Read and clear the marker so nothing this process spawns inherits it.
  const parentInstalled = Deno.env.get(DEPS_INSTALLED_ENV) === "1";
  Deno.env.delete(DEPS_INSTALLED_ENV);
  let deps = prepareDeps({ projectRoot: state.worktree });
  if (deps.installed && parentInstalled) {
    // The parent's install did not make fflate visible to this child either;
    // never re-exec again.
    throw new Error(
      "task prepare: deno install ran again in the child process; refusing to re-exec",
    );
  }
  if (deps.installed) {
    appendEvent(state, {
      timestamp: nowIso(),
      command: "task prepare (deps)",
      note: "deno install ran; continuing in a new process",
      exitCode: 0,
    });
    writeTaskState(state);
    const child = new Deno.Command(Deno.execPath(), {
      args: ["run", "-A", path.fromFileUrl(import.meta.url), ...Deno.args],
      env: { [DEPS_INSTALLED_ENV]: "1" },
      stdin: "inherit",
      stdout: "inherit",
      stderr: "inherit",
    });
    const status = await child.spawn().status;
    if (!status.success) {
      throw new Error(
        `task prepare after deno install failed with exit ${status.code}`,
      );
    }
    return;
  }
  if (parentInstalled && deps.status === "already") {
    // The parent installed the deps for this run; keep that fact. Only an
    // "already" result is upgraded, never a CI skip.
    deps = { ...deps, status: "prepared", installed: true };
  }
  const { prepareSetup } = await import("./src/setup.ts");
  const record: SetupRecord = await prepareSetup({
    projectRoot: state.worktree,
    depsResult: deps,
    runtime: { sourcePath: runtimeSource },
  });

  const previous = state.setup;
  state.setup = {
    guardEnv: record.guardEnv,
    runtimeSource: record.runtime.sourcePath ??
      previous?.runtimeSource ??
      record.runtime.binRootDir,
    profilePath: record.profile.profileDir,
    viteClearedAt: record.vite.removed
      ? record.preparedAt
      : previous?.viteClearedAt ?? null,
    commands: [
      `deps:${record.deps.status}`,
      `runtime:${record.runtime.status}`,
      `profile:${record.profile.status}`,
      `vite:${record.vite.status}`,
    ],
  };
  appendEvent(state, {
    timestamp: nowIso(),
    command: "task prepare",
    note: JSON.stringify({
      deps: record.deps.status,
      runtime: record.runtime.status,
      profile: record.profile.status,
      vite: record.vite.status,
      runtimeSource: state.setup.runtimeSource,
    }),
    exitCode: 0,
  });
  writeTaskState(state);

  console.log(JSON.stringify({
    taskId: state.taskId,
    setup: state.setup,
    steps: record,
  }));
}

async function cmdGui(
  parsed: ParsedArgs,
  sub: "claim" | "end" | "status",
): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id", "agent", "reason", "note"]);

  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);
  const file = await guiLeasePathForWorktree(state.worktree);

  if (sub === "status") {
    const lease = readGuiLease(file);
    console.log(
      JSON.stringify({ ...summarizeGuiLease(lease), leasePath: file }),
    );
    return;
  }

  const agent = requireOption(parsed, "agent");
  const note = hasOption(parsed, "note") ? requireOption(parsed, "note") : "";
  const reason = sub === "end"
    ? requireOption(parsed, "reason") as GuiLeaseEndReason
    : undefined;
  // The lease is shared by every worktree of the repository: read, change and
  // write it under one lock so two concurrent claims cannot both succeed.
  const lease = await withFileLock(lockPathFor(file), () => {
    const current = readGuiLease(file);
    if (reason === undefined) {
      claimGuiLease(current, agent, taskId, state.worktree);
    } else {
      endGuiLease(current, agent, reason, note);
    }
    writeGuiLease(file, current);
    return current;
  });
  appendTaskEvent(state.worktree, taskId, {
    timestamp: nowIso(),
    command: reason === undefined ? "gui claim" : "gui end",
    note: reason === undefined
      ? `GUI desktop lease claimed by ${agent}`
      : `GUI desktop lease ${reason} by ${agent}`,
    exitCode: 0,
  });
  console.log(JSON.stringify({ ...summarizeGuiLease(lease), leasePath: file }));
}

/** Same file path after resolving; case-insensitive on Windows. */
function samePath(a: string, b: string): boolean {
  const norm = (p: string) => {
    const resolved = path.resolve(p);
    return Deno.build.os === "windows" ? resolved.toLowerCase() : resolved;
  };
  return norm(a) === norm(b);
}

/**
 * Append one event to the task state. `writeTaskState` takes the task-state
 * lock and merges with the file's current content, so concurrent `quota` /
 * `gui` / build commands on one task keep each other's events. (It must not
 * be wrapped in another lock on the same file: a second handle in the same
 * process would wait for itself.)
 */
function appendTaskEvent(
  worktree: string,
  taskId: string,
  event: Parameters<typeof appendEvent>[1],
): void {
  const fresh = readTaskState(worktree, taskId);
  appendEvent(fresh, event);
  writeTaskState(fresh);
}

async function cmdSecrets(
  parsed: ParsedArgs,
  sub: "check" | "status",
): Promise<void> {
  if (sub === "status") {
    assertNoPositional(parsed, 2);
    assertAllowedOptions(parsed, []);
    console.log(JSON.stringify({
      policy: "evidence/task records must not reference repository secrets",
      secretsFile: ".secrets",
      secretsFileRead: false,
      enforcedBy: "fdh evidence + fdh secrets check",
    }));
    return;
  }
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, ["task-id"]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);
  const result = checkTaskSecrets(state.worktree, taskId);
  console.log(JSON.stringify(result));
  if (result.denied) {
    throw new Error(
      `secrets reference denied: task ${taskId} references ${result.references.length} secret marker(s)`,
    );
  }
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
  const note = hasOption(parsed, "note") ? requireOption(parsed, "note") : "";

  if (sub === "status") {
    const ledger = readQuotaLedger(state.worktree, taskId, true);
    console.log(JSON.stringify(summarizeQuota(ledger)));
    return;
  }

  // Read, change and write the ledger under one lock so two agents cannot
  // both claim the task, or lose each other's stop/approve.
  const ledgerFile = quotaLedgerPath(state.worktree, taskId);
  const ledger = await withFileLock(lockPathFor(ledgerFile), () => {
    const current = readQuotaLedger(state.worktree, taskId, true);
    // writeQuotaLedger writes to the path recorded in the ledger itself; it
    // must be the locked file, or the lock would not protect the update.
    if (
      !samePath(quotaLedgerPath(current.worktree, current.taskId), ledgerFile)
    ) {
      throw new Error(
        `quota ledger ${ledgerFile} records another task or worktree (${current.taskId} in ${current.worktree}); refusing to update it`,
      );
    }
    switch (sub) {
      case "claim":
        claimQuota(current, requireOption(parsed, "agent"));
        break;
      case "stop": {
        const reason = requireOption(parsed, "reason") as QuotaStopReason;
        stopQuota(current, requireOption(parsed, "agent"), reason, note);
        break;
      }
      case "resume":
        resumeQuota(current, requireOption(parsed, "agent"), note);
        break;
      case "pending":
        pendingQuota(
          current,
          requireOption(parsed, "op") as PendingExternalOp,
          note,
        );
        break;
      case "approve":
        approveQuota(current, note);
        break;
      case "release":
        releaseQuota(current, requireOption(parsed, "agent"));
        break;
    }
    writeQuotaLedger(current);
    return current;
  });
  const summary = summarizeQuota(ledger);
  appendTaskEvent(state.worktree, taskId, {
    timestamp: nowIso(),
    command: `quota ${sub}`,
    note: JSON.stringify(summary),
    exitCode: 0,
  });
  console.log(JSON.stringify(summary));
}

async function loadStateForCwd(taskId: string): Promise<FdhTaskState> {
  const worktree = await resolveCurrentWorktree();
  return readTaskStateFor(worktree, taskId);
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

  // `fdh task prepare` records the local Patcher guard. Set it on this CLI
  // process so the spawned dev-tool inherits it; no manual export is needed.
  if (action === "up" && state.setup?.guardEnv !== undefined) {
    for (const [name, value] of Object.entries(state.setup.guardEnv)) {
      Deno.env.set(name, value);
    }
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
  assertAllowedOptions(
    parsed,
    kind === "build" ? ["task-id", "phase"] : ["task-id"],
  );
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  const phase = kind === "build"
    ? parseBuildPhase(
      hasOption(parsed, "phase") ? requireOption(parsed, "phase") : undefined,
    )
    : undefined;

  // W-04: a source build under this worktree's running dev browser wedges
  // Marionette sessions. Refuse before spawning, and keep the refusal visible.
  // A guard that cannot determine liveness also stops (fail closed), but is
  // labelled as a guard failure, not as a refusal.
  if (phase !== undefined) {
    try {
      await assertSourceBuildAllowed(state.worktree, phase);
    } catch (error) {
      appendEvent(state, {
        timestamp: nowIso(),
        command: error instanceof DevGuardRefusalError
          ? refusedBuildEventCommand(phase)
          : guardFailedBuildEventCommand(phase),
        note: error instanceof Error ? error.message : String(error),
        exitCode: 1,
      });
      writeTaskState(state);
      throw error;
    }
  }

  const specs = phase !== undefined ? buildVerifyCommands(phase) : [{
    displayCommand: "deno task test:host",
    args: ["task", "test:host"],
    assertionName: "host-test-exit-code",
    expected: "exit code 0",
  }];

  // Run the phases in order and stop at the first failure: a failed source
  // compile must leave the injector unrun, and the failing assertion is
  // already persisted by runRecordedCommand before the throw.
  for (const spec of specs) {
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

/**
 * Record a build/test run that happened outside the fdh CLI (IMP-2).
 *
 * The operator supplies the observation file and exit code; fdh pins the file
 * bytes by SHA-256, stores them under `_dist/fdh/observations/`, and adds an
 * `external-<kind>-exit-code` assertion when `--verification` is set. The
 * evidence manifest re-hashes the file and discloses the attestation limits.
 */
async function cmdRecord(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 1);
  assertAllowedOptions(parsed, [
    "task-id",
    "command",
    "exit-code",
    "observation-file",
    "cwd",
    "operator",
    "verification",
    "attested-at",
  ]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  const exitCodeRaw = requireOption(parsed, "exit-code");
  const exitCode = Number(exitCodeRaw);
  if (!Number.isSafeInteger(exitCode)) {
    throw new Error(`--exit-code must be an integer, got ${exitCodeRaw}`);
  }

  const cwd = hasOption(parsed, "cwd")
    ? requireOption(parsed, "cwd")
    : state.worktree;
  if (!path.isAbsolute(cwd)) {
    throw new Error(`--cwd must be an absolute path, got ${cwd}`);
  }

  const verification = hasOption(parsed, "verification")
    ? requireOption(parsed, "verification") as ExternalVerification
    : undefined;

  const result = await recordExternalOperation(state, {
    command: requireOption(parsed, "command"),
    cwd,
    exitCode,
    observationFile: requireOption(parsed, "observation-file"),
    operator: requireOption(parsed, "operator"),
    verification,
    attestedAt: hasOption(parsed, "attested-at")
      ? requireOption(parsed, "attested-at")
      : undefined,
  });

  console.log(JSON.stringify({
    taskId: state.taskId,
    operation: result.operation,
    assertion: result.assertion ?? null,
  }));
}

/**
 * Record one independent-review receipt for the current PR head commit.
 *
 * The CLI is still local-only: it writes the receipt JSON under
 * `<worktree>/_dist/fdh/reviews/` and prints the exact comment body. The agent
 * (or human) posts that body to the PR; the `fdh-review-gate` workflow parses
 * the marker and checks distinct reviewers at the PR head.
 */
async function cmdReviewRecord(parsed: ParsedArgs): Promise<void> {
  assertNoPositional(parsed, 2);
  assertAllowedOptions(parsed, [
    "task-id",
    "reviewer",
    "model",
    "commit",
    "verdict",
    "scope",
    "tier",
    "blocking",
  ]);
  const taskId = validateTaskId(requireOption(parsed, "task-id"));
  const state = await loadStateForCwd(taskId);

  const verdict = requireOption(parsed, "verdict") as ReviewVerdict;
  if (!(REVIEW_VERDICTS as readonly string[]).includes(verdict)) {
    throw new Error(
      `--verdict must be one of ${REVIEW_VERDICTS.join("|")}, got ${
        JSON.stringify(verdict)
      }`,
    );
  }
  const tier = requireOption(parsed, "tier") as ReviewTier;
  if (!(REVIEW_TIERS as readonly string[]).includes(tier)) {
    throw new Error(
      `--tier must be one of ${REVIEW_TIERS.join("|")}, got ${
        JSON.stringify(tier)
      }`,
    );
  }

  let blocking = false;
  if (hasOption(parsed, "blocking")) {
    const raw = requireOption(parsed, "blocking");
    if (raw !== "true" && raw !== "false") {
      throw new Error(`--blocking must be true or false, got ${raw}`);
    }
    blocking = raw === "true";
  }

  const receipt: ReviewReceipt = {
    schemaVersion: FDH_REVIEW_RECEIPT_SCHEMA_VERSION,
    reviewer: requireOption(parsed, "reviewer"),
    model: requireOption(parsed, "model"),
    commit: requireOption(parsed, "commit").toLowerCase(),
    verdict,
    blocking,
    scope: requireOption(parsed, "scope"),
    tier,
  };
  // Validate against the same commit the receipt claims: the workflow later
  // checks it against the actual PR head SHA.
  const validated = validateReceipt(receipt, receipt.commit, {
    requireMergeable: false,
  });

  const reviewsDir = path.join(fdhDir(state.worktree), "reviews");
  await Deno.mkdir(reviewsDir, { recursive: true });
  // One file per reviewer and head: the gate needs two distinct reviewers at
  // the same commit, so their local receipts must not overwrite each other.
  const receiptPath = path.join(
    reviewsDir,
    receiptFileName(taskId, validated.commit, validated.reviewer),
  );
  await Deno.writeTextFile(
    receiptPath,
    `${JSON.stringify(validated, null, 2)}\n`,
  );

  const commentBody = `<!-- ${FDH_REVIEW_RECEIPT_MARKER} ${
    JSON.stringify(validated)
  } -->`;
  console.log(JSON.stringify({
    taskId: state.taskId,
    receiptPath,
    receipt: validated,
    commentBody,
  }));
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

  assertNoSecretReferences(state);

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
      if (sub === "prepare") return await cmdTaskPrepare(parsed);
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
    case "record":
      return await cmdRecord(parsed);
    case "quota":
      if (
        sub === "claim" || sub === "stop" || sub === "resume" ||
        sub === "pending" || sub === "approve" || sub === "release" ||
        sub === "status"
      ) {
        return await cmdQuota(parsed, sub);
      }
      throw new Error(`unknown quota command ${JSON.stringify(sub ?? "")}`);
    case "gui":
      if (sub === "claim" || sub === "end" || sub === "status") {
        return await cmdGui(parsed, sub);
      }
      throw new Error(`unknown gui command ${JSON.stringify(sub ?? "")}`);
    case "secrets":
      if (sub === "check" || sub === "status") {
        return await cmdSecrets(parsed, sub);
      }
      throw new Error(`unknown secrets command ${JSON.stringify(sub ?? "")}`);
    case "review":
      if (sub === "record") return await cmdReviewRecord(parsed);
      throw new Error(`unknown review command ${JSON.stringify(sub ?? "")}`);
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
