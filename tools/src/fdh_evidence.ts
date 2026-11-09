// SPDX-License-Identifier: MPL-2.0

/**
 * EvidenceManifest v0 builder for the `fdh` local loop (FDH Gate 3, KAN-113).
 *
 * The builder maps one `fdh` task-state file onto the seven-entity Envelope and
 * the five core relationships required by `tools/qa3/evidence/`, then validates
 * the result with the shared `validateManifest(value, { role: "runner" })`
 * before the CLI writes it. It never invents a pass: assertions come from the
 * commands the CLI actually ran, and the manifest always carries honest
 * limitations (including `visualJudge: null` and an UNVERIFIED DSH pin).
 */

import * as path from "@std/path";
import { encodeHex } from "@std/encoding/hex";
import type {
  Assertion,
  Attempt,
  Cleanup,
  Entity,
  EvidenceManifest,
  InputFidelityInput,
  Operation,
  Relationship,
  Reproduction,
  Runtime,
  State,
  ValidationError,
} from "../qa3/evidence/types.ts";
import { UNVERIFIED, validateManifest } from "../qa3/evidence/validate.ts";
import { type FdhTaskState, taskStatePath } from "./fdh_task.ts";
import { isRefusedBuildEvent } from "./fdh_verify.ts";

export const FDH_OWNER = "fdh-local-runner";
export const FDH_PRINCIPAL_ID = "fdh-cli";
export const FDH_PRINCIPAL_GRANTED_BY = "host:fdh-cli-host";

export interface FdhEvidenceIds {
  task: string;
  job: string;
  run: string;
  attempt: string;
  session: string;
  workspace: string;
  approval: string;
}

/** Stable IDs: the same task always yields the same issuer namespaces. */
export function evidenceIds(taskId: string): FdhEvidenceIds {
  return {
    task: `scheduler:task:${taskId}`,
    job: `scheduler:job:${taskId}`,
    run: `executor:run:${taskId}:1`,
    attempt: `executor:attempt:${taskId}:1:1`,
    session: `host:session:${taskId}`,
    workspace: `host:workspace:${taskId}`,
    approval: `host-authorization:approval:${taskId}:local`,
  };
}

export interface BuildEvidenceOptions {
  /** Override the detected OS block (used by deterministic tests). */
  os?: { name: string; version?: string; arch: string };
  owner?: string;
  principalId?: string;
}

export class EvidenceValidationError extends Error {
  readonly errors: ValidationError[];

  constructor(errors: ValidationError[]) {
    super(
      `evidence manifest failed validation: ${
        errors.map((entry) => `${entry.code} at ${entry.path}`).join(", ")
      }`,
    );
    this.name = "EvidenceValidationError";
    this.errors = errors;
  }
}

export function validateEvidenceManifest(
  value: unknown,
): ValidationError[] {
  return validateManifest(value, { role: "runner" });
}

async function detectOs(): Promise<
  { name: string; version?: string; arch: string }
> {
  const os: { name: string; version?: string; arch: string } = {
    name: Deno.build.os,
    arch: Deno.build.arch,
  };
  try {
    const version = await Deno.osRelease();
    if (version !== "") os.version = version;
  } catch {
    // The version is optional; leaving it out is honest.
  }
  return os;
}

function tail(text: string, max: number): string {
  return text.length <= max ? text : text.slice(text.length - max);
}

function lastTimestamp(state: FdhTaskState): string {
  const last = state.events[state.events.length - 1];
  return last?.timestamp ?? state.createdAt;
}

function externalObservation(
  operation: FdhTaskState["operations"][number],
): string {
  const external = operation.external!;
  const head =
    `external (not run by fdh); exit ${operation.exitCode}; cwd=${operation.cwd}; log=${external.logRef} sha256=${external.logSha256} bytes=${external.bytes}; operator=${external.operator}; attestedAt=${external.attestedAt}; verification=${
      external.verification ?? "none"
    }`;
  const output = operation.output.trim();
  return output === ""
    ? `${head}; no output`
    : `${head}; ${tail(output, 2000)}`;
}

function operationObservation(
  operation: FdhTaskState["operations"][number],
): string {
  if (operation.external !== undefined) return externalObservation(operation);
  const output = operation.output.trim();
  return output === ""
    ? `exit ${operation.exitCode}; no output`
    : `exit ${operation.exitCode}; ${tail(output, 2000)}`;
}

function isBrowserEval(operation: FdhTaskState["operations"][number]): boolean {
  const line = operation.displayCommand || operation.command;
  return line.includes("tools/dev-tool.ts eval");
}

/**
 * Final state from the LATEST assertion per name, so a deliberately broken
 * build/test that is fixed later (Gate 3 T-01 error-then-fix loop) ends as a
 * pass while the failed intermediate assertion stays visible in assertions[].
 */
function manifestStateOf(state: FdhTaskState): State {
  if (state.status === "cancelled") return "cancelled";
  if (state.status === "failed") return "failed";
  const latest = new Map<string, boolean>();
  for (const assertion of state.assertions) {
    latest.set(assertion.name, assertion.passed);
  }
  if ([...latest.values()].some((passed) => !passed)) return "failed";
  if (state.status === "succeeded") return "succeeded";
  if (latest.size > 0) return "succeeded";
  return "running";
}

function attemptResultOf(
  state: FdhTaskState,
): Attempt["result"] {
  const state_ = manifestStateOf(state);
  if (state_ === "failed") return "failed";
  if (state_ === "cancelled") return "cancelled";
  if (state_ === "succeeded") return "passed";
  return "inconclusive";
}

function buildLimitations(state: FdhTaskState, manifestState: State): string[] {
  const limitations: string[] = [];
  const add = (text: string) => {
    if (!limitations.includes(text)) limitations.push(text);
  };

  add(
    `This evidence covers only the local fdh loop for task '${state.taskId}'; it is not a Gate 3 qualification or release approval.`,
  );
  add(
    "No visual judgment was performed (visualJudge is null); visual/look-and-feel behavior is not covered.",
  );
  add(
    "The DSH toolchain identity is not available to fdh and is recorded as UNVERIFIED.",
  );
  add(
    "Assertions reflect the recorded command exit codes and verbatim outputs only; they do not establish behavior beyond those commands.",
  );
  add(
    "The local development worktree is not the qualification target environment (matchesTarget is false).",
  );
  add(
    "No GitHub or other shared-service behavior is exercised; the local loop does not need shared services.",
  );

  const externalOperations = state.operations.filter((operation) =>
    operation.external !== undefined
  );
  if (externalOperations.length > 0) {
    add(
      "At least one operation was run outside fdh and is operator-attested: the exit code is a CLI declaration, the observation file is operator-provided, and the operator identity is self-declared; fdh only re-checks the observation file SHA-256 when the manifest is built.",
    );
    add(
      "External operation timestamps are attestation times, not the command's start/end times; fdh did not observe the command execution or its cwd.",
    );
  }

  // Use the latest browser-connected assertion, matching manifestStateOf's
  // latest-per-name outcome rule (a pass followed by a fail is a failure).
  let latestBrowserConnected: boolean | undefined;
  for (const assertion of state.assertions) {
    if (assertion.name === "browser-connected") {
      latestBrowserConnected = assertion.passed;
    }
  }
  if (latestBrowserConnected !== true) {
    add(
      "Browser confirmation was not verified; browser behavior remains UNVERIFIED.",
    );
  }
  const hasSourceBuild = state.assertions.some((assertion) =>
    assertion.name === "build-exit-code" ||
    assertion.name === "external-build-exit-code"
  );
  const hasInjectBuild = state.assertions.some((assertion) =>
    assertion.name === "build-inject-exit-code"
  );
  if (!hasSourceBuild && !hasInjectBuild) {
    add("No build verification was recorded; the build state is UNVERIFIED.");
  } else if (!hasSourceBuild) {
    add(
      "Source compilation was not verified; the recorded build assertion is after-mach XHTML injection only.",
    );
  } else if (!hasInjectBuild) {
    add(
      "XHTML injection was not verified; the recorded build assertion is before-mach source compilation only.",
    );
  }
  // A source build refused after the last recorded source compilation means
  // the latest source edits were never compiled, even if an earlier build or
  // a later after-mach inject passed (W-04 review note).
  const lastSourceBuildAt = Math.max(
    -Infinity,
    ...state.assertions
      .filter((assertion) =>
        assertion.name === "build-exit-code" ||
        assertion.name === "external-build-exit-code"
      )
      .map((assertion) => Date.parse(assertion.at)),
  );
  const refusedAfterLastSource = state.events.some((event) =>
    isRefusedBuildEvent(event.command) &&
    Date.parse(event.timestamp) > lastSourceBuildAt
  );
  if (refusedAfterLastSource) {
    add(
      "A source build was refused after the last recorded source compilation (the dev server was running); the latest source edits were not compiled.",
    );
  }
  const hasTest = state.assertions.some((assertion) =>
    assertion.name === "host-test-exit-code" ||
    assertion.name === "external-host-test-exit-code"
  );
  if (!hasTest) {
    add(
      "No host test verification was recorded; the host test state is UNVERIFIED.",
    );
  }
  if (state.operations.some(isBrowserEval)) {
    add(
      "Browser eval output is recorded verbatim; only --expect turns it into a contains-check assertion, which does not prove behavior beyond the checked text.",
    );
  }
  if (
    state.cleanup.result !== "completed" &&
    state.cleanup.result !== "not-required"
  ) {
    add(
      `Cleanup did not complete (${state.cleanup.result}); ${state.cleanup.notes}`,
    );
  }
  if (manifestState === "failed") {
    add(
      "At least one recorded assertion failed; see assertions[] for the exact expected/actual values.",
    );
  }
  add(
    "No approval was requested or recorded; the approval entity is marked not-required.",
  );
  add(
    "The build/toolchain commit is the task's start base; an uncommitted working-tree diff is not hashed in this manifest.",
  );
  add(
    "No defect was reproduced; reproduction is inconclusive by design rather than being inferred from command output.",
  );
  return limitations;
}

function buildEntities(
  state: FdhTaskState,
  ids: FdhEvidenceIds,
  owner: string,
): Entity[] {
  return [
    {
      kind: "task",
      id: ids.task,
      issuer: "scheduler",
      owner,
      ownerRole: "task-owner",
      label: `fdh local loop task ${state.taskId}`,
    },
    {
      kind: "job",
      id: ids.job,
      issuer: "scheduler",
      owner,
      ownerRole: "job-owner",
      label: `local gate loop job for ${state.taskId}`,
    },
    {
      kind: "run",
      id: ids.run,
      issuer: "executor",
      owner,
      ownerRole: "run-owner",
      label: `fdh command run for ${state.taskId}`,
    },
    {
      kind: "attempt",
      id: ids.attempt,
      issuer: "executor",
      owner,
      ownerRole: "run-owner",
      label: `first attempt for ${state.taskId}`,
    },
    {
      kind: "session",
      id: ids.session,
      issuer: "host",
      owner,
      ownerRole: "session-owner",
      context: "existing agent session driving the fdh CLI",
    },
    {
      kind: "workspace",
      id: ids.workspace,
      issuer: "host",
      owner,
      ownerRole: "workspace-owner",
      path: state.worktree,
    },
    {
      kind: "approval",
      id: ids.approval,
      issuer: "host-authorization",
      owner,
      ownerRole: "approver",
      decision: "not-required",
      label: "no approval is requested by the fdh local loop",
    },
  ];
}

function buildRelationships(ids: FdhEvidenceIds): Relationship[] {
  return [
    { from: ids.task, type: "task-has-job", to: ids.job },
    { from: ids.job, type: "job-has-run", to: ids.run },
    { from: ids.run, type: "run-has-attempt", to: ids.attempt },
    { from: ids.run, type: "run-uses-session", to: ids.session },
    { from: ids.run, type: "run-in-workspace", to: ids.workspace },
    { from: ids.approval, type: "approval-for-job", to: ids.job },
  ];
}

function buildOperations(state: FdhTaskState): Operation[] {
  if (state.operations.length === 0) {
    return [{
      input: "fdh task start",
      target: state.worktree,
      order: 1,
      observation:
        `task state created at ${state.createdAt}; no command was recorded yet`,
    }];
  }
  return state.operations.map((operation, index) => ({
    input: operation.displayCommand ||
      [operation.command, ...operation.args].join(" "),
    target: operation.cwd,
    order: index + 1,
    observation: operationObservation(operation),
  }));
}

function buildAssertions(state: FdhTaskState): Assertion[] {
  if (state.assertions.length > 0) {
    return state.assertions.map((assertion) => ({
      name: assertion.name,
      expected: assertion.expected,
      actual: assertion.actual,
      passed: assertion.passed,
    }));
  }
  return [{
    name: "task-state-recorded",
    expected: "an fdh task state exists for this run",
    actual:
      `${state.operations.length} operation(s), 0 verification assertions`,
    passed: true,
  }];
}

function buildAttempts(state: FdhTaskState, ids: FdhEvidenceIds): Attempt[] {
  return [{
    run: ids.run,
    attempt: 1,
    startedAt: state.createdAt,
    endedAt: lastTimestamp(state),
    result: attemptResultOf(state),
    exitCode: null,
    notes:
      `fdh local loop; ${state.operations.length} command(s), ${state.assertions.length} assertion(s)`,
  }];
}

function buildRuntime(
  state: FdhTaskState,
  os: { name: string; version?: string; arch: string },
): Runtime {
  return {
    os: {
      name: os.name,
      ...(os.version !== undefined ? { version: os.version } : {}),
      arch: os.arch,
    },
    build: {
      commit: state.baseCommit,
      id: state.taskId,
      repository: "Floorp-Projects/Floorp",
    },
    packageDigest: null,
    toolchain: {
      dsh: { version: UNVERIFIED, pin: UNVERIFIED },
      floorp: {
        commit: state.baseCommit,
        pin: `floorp@${state.baseCommit}`,
      },
      deno: { version: Deno.version.deno },
    },
  };
}

async function sha256Hex(bytes: Uint8Array): Promise<string> {
  // Copy into a definitely ArrayBuffer-backed view for the WebCrypto typings.
  const input = new Uint8Array(bytes.length);
  input.set(bytes);
  const digest = await crypto.subtle.digest("SHA-256", input);
  return encodeHex(new Uint8Array(digest));
}

/**
 * Re-hash each recorded external observation file and pin it as an input.
 * A mismatch means the observation changed after attestation: the manifest
 * must not silently carry the original SHA-256 claim.
 */
async function externalObservationInputs(
  state: FdhTaskState,
): Promise<InputFidelityInput[]> {
  const inputs: InputFidelityInput[] = [];
  const seen = new Set<string>();
  for (const operation of state.operations) {
    const external = operation.external;
    if (external === undefined || seen.has(external.logRef)) continue;
    seen.add(external.logRef);
    const file = path.resolve(state.worktree, external.logRef);
    let bytes: Uint8Array;
    try {
      bytes = Deno.readFileSync(file);
    } catch (error) {
      throw new EvidenceValidationError([{
        code: "schema-violation",
        path: `/inputFidelity/inputs/${inputs.length + 1}`,
        message:
          `external observation file '${external.logRef}' is not readable: ${
            error instanceof Error ? error.message : String(error)
          }`,
      }]);
    }
    const sha256 = await sha256Hex(bytes);
    if (sha256 !== external.logSha256) {
      throw new EvidenceValidationError([{
        code: "schema-violation",
        path: `/inputFidelity/inputs/${inputs.length + 1}`,
        message:
          `external observation file '${external.logRef}' changed since attestation: sha256 ${sha256} != recorded ${external.logSha256}`,
      }]);
    }
    inputs.push({ path: file, sha256, bytes: bytes.length });
  }
  return inputs;
}

/**
 * Build an EvidenceManifest v0 for `state`, validate it with the shared
 * Runner rules, and return it. Throws `EvidenceValidationError` when the
 * built document would be rejected.
 */
export async function buildEvidenceManifest(
  state: FdhTaskState,
  options: BuildEvidenceOptions = {},
): Promise<EvidenceManifest> {
  const ids = evidenceIds(state.taskId);
  const owner = options.owner ?? FDH_OWNER;
  const os = options.os ?? await detectOs();
  const statePath = taskStatePath(state.worktree, state.taskId);
  const stateBytes = Deno.readFileSync(statePath);
  const externalInputs = await externalObservationInputs(state);
  const manifestState = manifestStateOf(state);

  const manifest: EvidenceManifest = {
    schemaVersion: 1,
    sourceFormat: "work-record",
    envelope: {
      epoch: 1,
      principal: {
        id: options.principalId ?? FDH_PRINCIPAL_ID,
        kind: "agent",
        grantedBy: FDH_PRINCIPAL_GRANTED_BY,
      },
      entities: buildEntities(state, ids, owner),
      relationships: buildRelationships(ids),
      taskStatus: {
        state: manifestState,
        cancelRequested: false,
      },
      cancelRequested: false,
      stateTransition: {
        from: manifestState === "running"
          ? "scheduled"
          : manifestState === "cancelled"
          ? "cancel_requested"
          : "running",
        to: manifestState,
        at: lastTimestamp(state),
        reason: "fdh local loop recorded the commands in operations[]",
      },
    },
    runtime: buildRuntime(state, os),
    attempts: buildAttempts(state, ids),
    operations: buildOperations(state),
    assertions: buildAssertions(state),
    environmentFidelity: {
      host: `${os.name}${os.version ? ` ${os.version}` : ""} (${os.arch})`,
      network: "enabled",
      matchesTarget: false,
      deviations: [
        "local development worktree rather than a packaged qualification environment",
        "no external network services are used; only the local dev server may be running",
      ],
      notes:
        "Local fdh loop on the recorded worktree; this is not a release-environment run.",
    },
    inputFidelity: {
      inputs: [
        {
          path: statePath,
          sha256: await sha256Hex(stateBytes),
          bytes: stateBytes.length,
        },
        ...externalInputs,
      ],
      unchangedDuringRun: "unknown",
      deviations: [
        "the task-state file is the evidence source and can change after this manifest is written",
        ...(externalInputs.length > 0
          ? [
            "external observation files were supplied by the operator and are re-hashed when this manifest is built",
          ]
          : []),
      ],
      notes: externalInputs.length > 0
        ? `Pinned inputs are the fdh task-state snapshot and ${externalInputs.length} external observation file(s) re-hashed while building this manifest.`
        : "Pinned input is the fdh task-state snapshot read while building this manifest.",
    },
    visualJudge: null,
    cleanup: {
      result: state.cleanup.result,
      notes: state.cleanup.notes,
      ...(state.cleanup.details !== undefined
        ? { details: state.cleanup.details }
        : {}),
    } satisfies Cleanup,
    limitations: buildLimitations(state, manifestState),
    reproduction: {
      result: state.reproduction.result,
      notes: state.reproduction.notes,
      ...(state.reproduction.defectRef !== undefined
        ? { defectRef: state.reproduction.defectRef }
        : {}),
    } satisfies Reproduction,
  };

  const errors = validateEvidenceManifest(manifest);
  if (errors.length > 0) {
    throw new EvidenceValidationError(errors);
  }
  return manifest;
}

export interface WrittenEvidence {
  path: string;
  bytes: number;
}

/** Write an already-validated manifest as pretty JSON. */
export async function writeEvidenceManifest(
  outPath: string,
  manifest: EvidenceManifest,
): Promise<WrittenEvidence> {
  const text = `${JSON.stringify(manifest, null, 2)}\n`;
  await Deno.mkdir(path.dirname(outPath), { recursive: true });
  await Deno.writeTextFile(outPath, text);
  return { path: outPath, bytes: new TextEncoder().encode(text).length };
}
