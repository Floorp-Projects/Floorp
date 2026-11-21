export type DurableBoundary =
  | "PREPARED"
  | "OLD_STOPPED"
  | "CANDIDATE_READY"
  | "committed_pending_publish"
  | "fallback_pending_before_mark"
  | "fallback_attempt_marked"
  | "disabled_update_prepared"
  | "disable_prepared"
  | "enable_prepared";

export type SetupKind =
  | "enabled_update"
  | "disabled_update"
  | "disable"
  | "enable";

export type RuntimeLockArchitecture = "aarch64" | "x86_64";

export interface ScenarioStep {
  boundary: DurableBoundary | null;
  reopen: boolean;
  checkRollback?: boolean;
}

export interface LinuxProcessIdentity {
  pid: number;
  parentPid: number;
  processGroupId: number;
  sessionId: number;
  startTimeTicks: string;
  executable: string;
  commandLine: string[];
}

export interface DurableMarker {
  schemaVersion: 1;
  runNonce: string;
  launchId: number;
  boundary: DurableBoundary;
  evaluationCount: number;
  snapshot: Record<string, unknown>;
}

export interface BrowserDriverResult {
  schemaVersion: 1;
  runNonce: string;
  launchId: number;
  buildId: string;
  evaluationCount: number;
  evaluationCountBeforeRollback: number;
  diagnostics: Record<string, unknown>;
  journal: Record<string, unknown>;
  outcomeBeforeRollback: unknown;
  rollbackResult: unknown;
  postRollbackJournal: Record<string, unknown> | null;
}

export interface DiskJournalEvidence {
  path: string;
  byteLength: number;
  sha256: string;
  snapshot: Record<string, unknown>;
  canonicalMatch: boolean | null;
}

export interface ProcessRecoveryTrace {
  schemaVersion: 1;
  runNonce: string;
  sourceCommit: string;
  sourceStatus: string;
  sourcePatchSha256: string;
  runtimeLockSha256: string | null;
  runtimeInstalledLockSha256: string | null;
  runtimeExpectedBuildId: string;
  runtimeBuildId: string | null;
  marionettePort: number;
  osServerPort: number;
  assertions: Array<{ id: string; passed: boolean; detail: string }>;
  scenarios: Array<Record<string, unknown>>;
  failure: string | null;
}

export interface ProcessOwnershipMonitor {
  root: LinuxProcessIdentity;
  executable: string;
  profilePath: string;
  members: Map<number, LinuxProcessIdentity>;
  failure: Error | null;
  stopped: boolean;
  task: Promise<void>;
}

export interface ProcessSupervisorDependencies {
  listProcesses(): Promise<LinuxProcessIdentity[]>;
  readProcess(pid: number): Promise<LinuxProcessIdentity | null>;
  signal(pid: number, signal: "SIGKILL"): void;
}

export interface BrowserLaunch {
  child: Deno.ChildProcess;
  root: LinuxProcessIdentity;
  ownership: ProcessOwnershipMonitor;
  client: import("./browser_connector.ts").MarionetteClient;
  port: number;
  stdout: Promise<void>;
  stderr: Promise<void>;
  buildId: string;
}
