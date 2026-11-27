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
import { isBlockedBuildEvent } from "./fdh_verify.ts";
import {
  MAX_PLATFORM_REPORT_BYTES,
  MAX_PLATFORM_SCREENSHOT_BYTES,
  platformAssertionName,
  platformReportRef,
  platformScreenshotRef,
  QA_PLATFORMS,
  readPlatformFile,
} from "./fdh_platform_qa.ts";
import type { PlatformQaRecord, QaPlatform } from "./fdh_platform_qa_types.ts";

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
  /**
   * Commit the manifest is built for (the worktree HEAD). A platform QA
   * result counts only when it was recorded at this commit on a clean
   * worktree (KAN-146); without it, no platform result counts.
   */
  head?: string;
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
function manifestStateOf(
  state: FdhTaskState,
  extra: readonly Assertion[] = [],
): State {
  if (state.status === "cancelled") return "cancelled";
  if (state.status === "failed") return "failed";
  const latest = new Map<string, boolean>();
  for (const assertion of [...state.assertions, ...extra]) {
    latest.set(assertion.name, assertion.passed);
  }
  if ([...latest.values()].some((passed) => !passed)) return "failed";
  if (state.status === "succeeded") return "succeeded";
  if (latest.size > 0) return "succeeded";
  return "running";
}

function attemptResultOf(
  state: FdhTaskState,
  extra: readonly Assertion[],
): Attempt["result"] {
  const state_ = manifestStateOf(state, extra);
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
  const hasFdhSourceBuild = state.assertions.some((assertion) =>
    assertion.name === "build-exit-code"
  );
  const hasSourceBuild = hasFdhSourceBuild ||
    state.assertions.some((assertion) =>
      assertion.name === "external-build-exit-code"
    );
  if (hasSourceBuild && !hasFdhSourceBuild) {
    // An external build record names a command fdh never checked; it may not
    // have compiled the source at all (for example an after-mach-only run).
    add(
      "Source compilation is operator-attested only: no source build was run by fdh, and the recorded external build command was not checked to be a source build.",
    );
  }
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
  // A source build refused (or stopped by a failing guard) after the last
  // recorded source compilation means the latest source edits were never
  // compiled, even if an earlier build or a later after-mach inject passed.
  // Only a source build fdh ran itself clears it: an external record's time is
  // an operator attestation, not the build time, and its command is not
  // checked to be a source build. Unparseable timestamps never clear it: a
  // source build with a bad `at` does not count, and a blocked event with a
  // bad timestamp counts.
  const lastSourceBuildAt = Math.max(
    -Infinity,
    ...state.assertions
      .filter((assertion) => assertion.name === "build-exit-code")
      .map((assertion) => Date.parse(assertion.at))
      .filter((at) => Number.isFinite(at)),
  );
  const blockedAfterLastSource = state.events.some((event) => {
    if (!isBlockedBuildEvent(event.command)) return false;
    const at = Date.parse(event.timestamp);
    return !Number.isFinite(at) || at > lastSourceBuildAt;
  });
  if (blockedAfterLastSource) {
    add(
      "A source build was refused or blocked after the last source build fdh ran (dev server running or dev guard failure); the latest source edits were not compiled.",
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
    state.assertions.some((assertion) =>
      assertion.name.startsWith("platform-qa-")
    )
  ) {
    add(
      "Platform QA (fdh platform-qa, KAN-145) ran a packaged build on one QA target per platform (a macOS VM, a Linux container); it is an early development check, not QA3, release qualification or a visual judgment. A result counts only when its job ran at this HEAD on a clean worktree (platform-qa-<platform>-at-head, KAN-146), but fdh did not verify that the payload itself was built from this HEAD; the payload is identified by its SHA-256 only.",
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

function buildAssertions(
  state: FdhTaskState,
  extra: readonly Assertion[],
): Assertion[] {
  if (state.assertions.length > 0) {
    return [
      ...state.assertions.map((assertion) => ({
        name: assertion.name,
        expected: assertion.expected,
        actual: assertion.actual,
        passed: assertion.passed,
      })),
      ...extra,
    ];
  }
  return [{
    name: "task-state-recorded",
    expected: "an fdh task state exists for this run",
    actual:
      `${state.operations.length} operation(s), 0 verification assertions`,
    passed: true,
  }];
}

function buildAttempts(
  state: FdhTaskState,
  ids: FdhEvidenceIds,
  extra: readonly Assertion[],
): Attempt[] {
  return [{
    run: ids.run,
    attempt: 1,
    startedAt: state.createdAt,
    endedAt: lastTimestamp(state),
    result: attemptResultOf(state, extra),
    exitCode: null,
    notes: `fdh local loop; ${state.operations.length} command(s), ${
      state.assertions.length + extra.length
    } assertion(s)`,
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

/** Name of the assertion that binds a platform result to the manifest's commit. */
export function platformAtHeadName(platform: string): string {
  return `platform-qa-${platform}-at-head`;
}

/**
 * `expected` of a `platform-qa-<p>-at-head` assertion. `evidence trace` reads
 * the commit back with PLATFORM_AT_HEAD_RE, so the two must change together.
 */
export function platformAtHeadExpected(
  platform: string,
  head: string | undefined,
): string {
  return `the ${platform} result was recorded at HEAD ${
    head ?? "(not given)"
  } on a clean worktree`;
}

export const PLATFORM_AT_HEAD_RE =
  /^the [a-z]+ result was recorded at HEAD ([0-9a-f]{40}(?:[0-9a-f]{24})?) on a clean worktree$/;

/** The record of a task-state assertion is well formed for this task. */
function isRecordFor(
  value: unknown,
  taskId: string,
  platform: QaPlatform,
): value is PlatformQaRecord {
  if (value === null || typeof value !== "object") return false;
  const r = value as Record<string, unknown>;
  const allowed = [
    platformReportRef(taskId, platform),
    platformScreenshotRef(taskId, platform),
  ];
  return r.platform === platform && typeof r.jobId === "string" &&
    typeof r.head === "string" && typeof r.cleanWorktree === "boolean" &&
    Array.isArray(r.attachments) &&
    r.attachments.every((a: unknown) => {
      if (a === null || typeof a !== "object") return false;
      const x = a as Record<string, unknown>;
      return typeof x.ref === "string" && allowed.includes(x.ref) &&
        typeof x.sha256 === "string" && /^[0-9a-f]{64}$/.test(x.sha256) &&
        Number.isSafeInteger(x.bytes);
    });
}

interface PlatformEvidence {
  assertions: Assertion[];
  inputs: InputFidelityInput[];
  deviations: string[];
}

/**
 * The latest result of each platform (KAN-146): re-hash the report and
 * screenshot its job pinned (a changed file stops the build, like an
 * external observation), and add a `platform-qa-<p>-at-head` assertion that
 * passes only when the result was recorded at `head` on a clean worktree.
 * An older result, or one from before results were bound, does not count.
 */
async function platformEvidence(
  state: FdhTaskState,
  head: string | undefined,
  /** Inputs that come before these in inputFidelity.inputs. */
  inputsBefore: number,
): Promise<PlatformEvidence> {
  const out: PlatformEvidence = { assertions: [], inputs: [], deviations: [] };
  const want = head?.trim().toLowerCase();
  for (const platform of QA_PLATFORMS) {
    const name = platformAssertionName(platform);
    const latest = state.assertions.filter((a) => a.name === name).at(-1);
    if (latest === undefined) continue;
    const record = latest.platform;
    const expected = platformAtHeadExpected(platform, want);
    if (record !== undefined && !isRecordFor(record, state.taskId, platform)) {
      out.assertions.push({
        name: platformAtHeadName(platform),
        expected,
        actual: "the recorded commit binding is malformed",
        passed: false,
      });
      continue;
    }
    if (record === undefined) {
      out.assertions.push({
        name: platformAtHeadName(platform),
        expected,
        actual:
          "the result carries no commit binding: no report was pinned (the job did not finish, its report was not read, or it was recorded before KAN-146)",
        passed: false,
      });
      continue;
    }
    for (const attachment of record.attachments) {
      const file = path.join(state.worktree, ...attachment.ref.split("/"));
      const at = `/inputFidelity/inputs/${inputsBefore + out.inputs.length}`;
      const bytes = readPlatformFile(
        state.worktree,
        attachment.ref,
        attachment.ref.endsWith(".png")
          ? MAX_PLATFORM_SCREENSHOT_BYTES
          : MAX_PLATFORM_REPORT_BYTES,
      );
      if (typeof bytes === "string") {
        throw new EvidenceValidationError([{
          code: "schema-violation",
          path: at,
          message: `platform QA file ${bytes}`,
        }]);
      }
      const sha256 = await sha256Hex(bytes);
      if (sha256 !== attachment.sha256) {
        throw new EvidenceValidationError([{
          code: "schema-violation",
          path: at,
          message:
            `platform QA file '${attachment.ref}' changed since job ${record.jobId} settled: sha256 ${sha256} != recorded ${attachment.sha256}; run the platform step again`,
        }]);
      }
      out.inputs.push({ path: file, sha256, bytes: bytes.length });
    }
    const bound = want !== undefined && record.head === want &&
      record.cleanWorktree;
    out.assertions.push({
      name: platformAtHeadName(platform),
      expected,
      actual: `recorded by job ${record.jobId} at ${record.head} (${
        record.cleanWorktree ? "clean worktree" : "uncommitted changes"
      })`,
      passed: bound,
    });
    out.deviations.push(
      `${platform} QA ran on an FDH QA target (${
        record.target ?? "target not recorded"
      }), not on this host`,
    );
  }
  return out;
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
  // inputs[0] is the task state, then the external observation files.
  const platform = await platformEvidence(
    state,
    options.head,
    1 + externalInputs.length,
  );
  const manifestState = manifestStateOf(state, platform.assertions);

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
    attempts: buildAttempts(state, ids, platform.assertions),
    operations: buildOperations(state),
    assertions: buildAssertions(state, platform.assertions),
    environmentFidelity: {
      host: `${os.name}${os.version ? ` ${os.version}` : ""} (${os.arch})`,
      network: "enabled",
      matchesTarget: false,
      deviations: [
        "local development worktree rather than a packaged qualification environment",
        "no external network services are used; only the local dev server may be running",
        ...platform.deviations,
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
        ...platform.inputs,
      ],
      unchangedDuringRun: "unknown",
      deviations: [
        "the task-state file is the evidence source and can change after this manifest is written",
        ...(externalInputs.length > 0
          ? [
            "external observation files were supplied by the operator and are re-hashed when this manifest is built",
          ]
          : []),
        ...(platform.inputs.length > 0
          ? [
            "platform QA reports and screenshots were pinned when their jobs settled and are re-hashed when this manifest is built",
          ]
          : []),
      ],
      notes: externalInputs.length + platform.inputs.length > 0
        ? `Pinned inputs are the fdh task-state snapshot, ${externalInputs.length} external observation file(s) and ${platform.inputs.length} platform QA file(s), re-hashed while building this manifest.`
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
