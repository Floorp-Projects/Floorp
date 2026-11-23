// SPDX-License-Identifier: MPL-2.0
/**
 * EvidenceManifest v0 TypeScript types.
 *
 * These types mirror `evidence-manifest.schema.json` (frozen ADR 8061068
 * sections 1.1-1.5) and are importable by tools:
 *
 * ```ts
 * import type { EvidenceManifest, ValidationError } from "./evidence/types.ts";
 * ```
 *
 * Runtime validation lives in `validate.ts`; the JSON Schema stays the contract
 * so Python (`validate.py`) and TypeScript (`validate.ts`) reject the same
 * fixtures with the same error codes.
 */

/** Version of the Envelope/manifest document. Any other value is rejected. */
export const SCHEMA_VERSION = 1;

export type SchemaVersion = 1;

export type EntityKind =
  | "task"
  | "job"
  | "run"
  | "attempt"
  | "session"
  | "workspace"
  | "approval";

/** ID issuer (ADR 1.1). Each issuer owns a stable ID namespace. */
export type IdIssuer =
  | "scheduler"
  | "executor"
  | "host"
  | "host-authorization";

export type OwnerRole =
  | "task-owner"
  | "job-owner"
  | "executor"
  | "run-owner"
  | "session-owner"
  | "workspace-owner"
  | "approver";

export type State =
  | "pending"
  | "scheduled"
  | "awaiting-approval"
  | "running"
  | "cancel_requested"
  | "succeeded"
  | "failed"
  | "cancelled";

export type RelationshipType =
  | "task-has-job"
  | "job-has-run"
  | "run-has-attempt"
  | "run-uses-session"
  | "run-in-workspace"
  | "approval-for-job";

export type ApprovalDecision =
  | "approved"
  | "denied"
  | "pending"
  | "not-required";

export type AttemptResult =
  | "passed"
  | "failed"
  | "error"
  | "timed-out"
  | "cancelled"
  | "inconclusive";

export type CleanupResult =
  | "completed"
  | "partial"
  | "failed"
  | "not-required"
  | "unknown";

/** ADR 1.3 / 2.1 reproduction axis, reported separately from pass/fail. */
export type ReproductionResult =
  | "reproduced"
  | "not-reproduced"
  | "inconclusive";

/** ADR 1.3 見た目判定者. */
export type VisualJudgeSource = "human" | "agent";

export type SourceFormat = "qa3-receipt" | "final-package-qa" | "work-record";

export type NetworkPosture = "disabled" | "enabled" | "not-used" | "unknown";

/** ISO 8601 timestamp with offset, or null when the source record has none. */
export type IsoDateTime = string | null;

export interface EntityFields {
  id: string;
  owner: string;
  label?: string;
}

export interface TaskEntity extends EntityFields {
  kind: "task";
  issuer: "scheduler";
  ownerRole: "task-owner";
}

export interface JobEntity extends EntityFields {
  kind: "job";
  issuer: "scheduler";
  ownerRole: "job-owner" | "executor";
}

export interface RunEntity extends EntityFields {
  kind: "run";
  issuer: "executor";
  ownerRole: "run-owner";
}

export interface AttemptEntity extends EntityFields {
  kind: "attempt";
  issuer: "executor";
  ownerRole: "run-owner";
}

export interface SessionEntity extends EntityFields {
  kind: "session";
  issuer: "host";
  ownerRole: "session-owner";
  context?: string;
}

export interface WorkspaceEntity extends EntityFields {
  kind: "workspace";
  issuer: "host";
  ownerRole: "workspace-owner";
  path?: string;
}

export interface ApprovalEntity extends EntityFields {
  kind: "approval";
  issuer: "host-authorization";
  ownerRole: "approver";
  decision: ApprovalDecision;
}

export type Entity =
  | TaskEntity
  | JobEntity
  | RunEntity
  | AttemptEntity
  | SessionEntity
  | WorkspaceEntity
  | ApprovalEntity;

export interface Relationship {
  from: string;
  type: RelationshipType;
  to: string;
}

export interface Principal {
  id: string;
  kind: "human" | "agent";
  /** Authenticated Host that granted the principal; model self-declarations are not trusted. */
  grantedBy: string;
}

export interface TaskStatus {
  state: State;
  /** TaskStatus-body side of `cancel_requested`. */
  cancelRequested: boolean;
}

export interface StateTransition {
  from: State;
  to: State;
  at?: IsoDateTime;
  reason?: string;
}

export interface Envelope {
  epoch: number;
  principal: Principal;
  entities: Entity[];
  relationships: Relationship[];
  taskStatus: TaskStatus;
  /** Envelope side of `cancel_requested`; must equal `taskStatus.cancelRequested`. */
  cancelRequested: boolean;
  stateTransition: StateTransition;
}

export interface OsBlock {
  name: string;
  version?: string;
  /** Architecture, or the literal UNVERIFIED when the record does not carry it. */
  arch: string;
  kernel?: string;
}

export interface BuildBlock {
  commit: string | null;
  id?: string;
  repository?: string;
  workflowRun?: string | number;
}

export interface PackageDigest {
  algorithm: "sha256" | "sha512";
  value: string;
  artifact?: string;
}

export interface ToolVersion {
  version: string;
  pin?: string;
}

export interface DshToolchain {
  version: string;
  pin: string;
}

export interface FloorpToolchain {
  commit: string | null;
  pin: string;
  version?: string;
}

export interface Toolchain {
  dsh: DshToolchain;
  floorp: FloorpToolchain;
  runtime?: { commit: string; pin: string };
  python?: ToolVersion;
  deno?: ToolVersion;
  node?: ToolVersion;
}

export interface Runtime {
  os: OsBlock;
  build: BuildBlock;
  packageDigest: PackageDigest | null;
  toolchain: Toolchain;
}

/** ADR 1.3 個別試行: Run/Attempt identity, start/end time and result. */
export interface Attempt {
  run: string;
  attempt: number;
  startedAt: IsoDateTime;
  endedAt: IsoDateTime;
  result: AttemptResult;
  exitCode?: number | null;
  logRef?: string;
  notes?: string;
}

/** ADR 1.3 操作列: input, target, order, observation. */
export interface Operation {
  input: string;
  target: string;
  order: number;
  observation: string;
}

/** ADR 1.3 assertion: expected, actual, pass/fail. */
export interface Assertion {
  name: string;
  expected: string;
  actual: string;
  passed: boolean;
}

export interface EnvironmentFidelity {
  host: string;
  network: NetworkPosture;
  matchesTarget: boolean;
  deviations: string[];
  notes: string;
}

export interface InputFidelityInput {
  path: string;
  sha256: string;
  bytes?: number;
}

export interface InputFidelity {
  inputs: InputFidelityInput[];
  unchangedDuringRun: boolean | "unknown";
  deviations: string[];
  notes: string;
}

/** ADR 1.3 見た目判定者, or null when no visual judgment was performed. */
export interface VisualJudge {
  source: VisualJudgeSource;
  scope: string;
  notes: string;
  artifactRef?: string;
}

/** ADR 1.3 後片付けの結果. */
export interface Cleanup {
  result: CleanupResult;
  notes: string;
  details?: string[];
}

export interface Reproduction {
  result: ReproductionResult;
  defectRef?: string;
  notes: string;
}

export interface EvidenceManifest {
  schemaVersion: SchemaVersion;
  sourceFormat?: SourceFormat;
  envelope: Envelope;
  runtime: Runtime;
  attempts: Attempt[];
  operations: Operation[];
  assertions: Assertion[];
  environmentFidelity: EnvironmentFidelity;
  inputFidelity: InputFidelity;
  visualJudge: VisualJudge | null;
  cleanup: Cleanup;
  /** What this evidence does NOT prove. At least one entry. */
  limitations: string[];
  reproduction: Reproduction;
}

/** Loaded from `state-transitions.json` (single source of truth with validate.py). */
export interface StateTransitionTable {
  schemaVersion: number;
  states: State[];
  initialStates: State[];
  terminalStates: State[];
  transitions: Record<State, State[]>;
  cancelRequestedState: State;
  notes: string[];
}

export type ValidationErrorCode =
  | "unsupported-schema-version"
  | "missing-required-evidence"
  | "schema-violation"
  | "invalid-transition"
  | "cancel-state-mismatch"
  | "stale-epoch"
  | "missing-current-epoch"
  | "unknown-entity-reference"
  | "duplicate-entity-id"
  | "missing-entity"
  | "missing-relationship"
  | "invalid-id-namespace"
  | "invalid-operation-order"
  | "missing-limitation";

export interface ValidationError {
  code: ValidationErrorCode;
  path: string;
  message: string;
}

export interface ValidationResult {
  ok: boolean;
  errors: ValidationError[];
}

export interface ValidationOptions {
  /** Central epoch; when given, older reports are rejected as stale. */
  currentEpoch?: number;
  /** `scheduler` (central intake) requires `currentEpoch`. */
  role?: "scheduler" | "runner";
}
