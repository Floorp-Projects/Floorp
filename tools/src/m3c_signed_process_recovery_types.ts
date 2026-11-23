// SPDX-License-Identifier: MPL-2.0

import type {
  DurableBoundary,
  DurableMarker,
  LinuxProcessIdentity,
} from "./hotfix_process_recovery_types.ts";

export type M3cProcessBoundary = Extract<
  DurableBoundary,
  | "PREPARED"
  | "OLD_STOPPED"
  | "CANDIDATE_READY"
  | "committed_pending_publish"
  | "fallback_pending_before_mark"
  | "fallback_attempt_marked"
>;

export interface M3cBoundaryExpectation {
  boundary: M3cProcessBoundary;
  transactionId: string;
  operation: "install" | "update" | "enable" | "disable";
  targetPackageId: string;
  desiredEnabled?: boolean;
  attempted?: boolean;
}

export interface M3cResolvedBoundaryExpectation extends M3cBoundaryExpectation {
  targetVersion: string;
  targetDigest: string;
}

export interface M3cDurableMarker extends DurableMarker {
  boundary: M3cProcessBoundary;
  testId: string;
  featureId: string;
  journal: Record<string, unknown>;
  journalSha256: string;
  journalByteLength: number;
}

export interface M3cBrowserDriverResult {
  schemaVersion: 1;
  runNonce: string;
  launchId: number;
  buildId: string;
  testId: string;
  featureId: string;
  evaluationCount: number;
  evaluationCountBefore: number;
  diagnostics: Record<string, unknown>;
  cleanupDiagnostics: {
    proven: boolean;
    evidence: string[];
  } | null;
  journal: Record<string, unknown>;
  journalSha256: string;
  journalByteLength: number;
  outcomes: Record<string, unknown>;
  windowObservation: Record<string, unknown> | null;
}

export interface M3cAdmissionState {
  journalSha256: string;
  journalByteLength: number;
  revision: number;
  evaluationCount: number;
  highWaterFingerprint: string;
}

export interface M3cOwnedProcessEvidence extends LinuxProcessIdentity {
  profilePath: string;
  killedPids: number[] | null;
  portClosed: boolean | null;
  teardownError?: string;
}

export interface M3cProcessLaunchEvidence {
  launchId: number;
  mode: "create" | "reopen";
  process: M3cOwnedProcessEvidence;
  boundary: M3cProcessBoundary | null;
  evaluationCount: number | null;
  journalSha256: string | null;
  journal: Record<string, unknown> | null;
  diagnostics: Record<string, unknown> | null;
  cleanupDiagnostics: M3cBrowserDriverResult["cleanupDiagnostics"];
  outcomes: Record<string, unknown> | null;
  failure?: string;
}

export interface M3cProcessScenarioEvidence extends Record<string, unknown> {
  id: string;
  testId: string;
  featureId: string;
  runDirectory: string;
  profilePath: string;
  launches: M3cProcessLaunchEvidence[];
  failure?: string;
}

export interface M3cPackageFixture {
  version: string;
  releaseSequence: number;
  packageSha256: string;
  bytes: Uint8Array;
}

export interface M3cPublicAuthorization {
  trustKind: "core-system-principal";
  policy: {
    featureId: string;
    hostSchema: 2;
    hostBuildId: string;
    runtimeBuildId: string;
    allowedCapabilities: string[];
    minimumReleaseSequence: number;
    trustedKeys: Array<{ keyId: string; publicKey: string }>;
  };
}

export type M3cBrowserAction =
  | { kind: "stage"; packageId: string }
  | {
    kind: "update-rejected";
    txId: string;
    packageId: string;
    code: string;
  }
  | { kind: "install"; txId: string; packageId: string }
  | { kind: "update"; txId: string; packageId: string }
  | { kind: "enable"; txId: string }
  | { kind: "disable"; txId: string }
  | { kind: "close"; repeat?: number }
  | { kind: "owner-retention-probe" }
  | { kind: "window-close-poison" }
  | { kind: "token-null-detach"; txId: string; packageId: string }
  | { kind: "registry-release-poison"; packageId: string };

export interface M3cProcessStep {
  mode: "create" | "reopen";
  actions: M3cBrowserAction[];
  boundary?: M3cBoundaryExpectation;
  injectRecoveryFailure?: boolean;
  removeCachedPackage?: string;
  expected?: M3cExpectedState;
}

export interface M3cExpectedState {
  selectedPackageId?: string | null;
  desiredEnabled?: boolean;
  highWaterSequence?: number | null;
  highWaterPackageId?: string | null;
  evaluationCount?: number;
  pending?: boolean;
  outcomeStatus?: string;
  blockedReason?: string | null;
  blocked?: boolean;
  cleanupProven?: boolean;
  cleanupEvidenceIncludes?: string[];
  windowClosed?: boolean;
  mainWindowOpen?: boolean;
  windowDisposerRan?: boolean;
  detachReportObserved?: boolean;
  signedWindowCleanupPending?: boolean;
  windowDetachPending?: boolean;
  closeWaitedForDetach?: boolean;
  updateEvaluationDeltas?: number[];
  attemptedPackageIds?: string[];
  rejectedPackageIds?: string[];
  registryClaims?: number;
  registryReleaseAttempts?: number;
  closeErrorIncludes?: string;
}

export interface M3cProcessScenario {
  id: string;
  featureId: string;
  capabilities?: string[];
  steps: M3cProcessStep[];
}
