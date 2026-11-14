// SPDX-License-Identifier: MPL-2.0

export type FinalQaTarget =
  | "windows/x86_64"
  | "linux/x86_64"
  | "mac/x86_64"
  | "mac/aarch64";

export interface FinalQaArtifactIdentity {
  id: number;
  name: string;
  digest: string;
}

export interface FinalQaRunIdentity {
  id: number;
  workflowId: number;
  attempt: number;
  head: string;
}

/** Policy supplied by a trusted caller, never inferred from evidence contents. */
export interface FinalPackageQaExpectation {
  repository: { id: number; fullName: "Floorp-Projects/Floorp" };
  packageRun: FinalQaRunIdentity;
  qaRun: FinalQaRunIdentity;
  bundle: FinalQaArtifactIdentity;
  targets: Record<FinalQaTarget, {
    package: FinalQaArtifactIdentity;
    record: FinalQaArtifactIdentity;
    evidence: FinalQaArtifactIdentity;
    floorpVersion: string;
    firefoxVersion: string;
    runtimeBuildId: string;
  }>;
  now: string;
  maxAgeMs: number;
  windowsPolicy: "require-success" | "shadow-observation";
}

/**
 * Untrusted data boundary: native REST objects plus caller-decoded archive entries.
 * No archive authenticity is asserted by this shape. Duplicate entries stay visible.
 */
export interface FinalPackageQaObservation {
  packageRun: unknown;
  qaRun: unknown;
  qaJobs: unknown;
  packageArtifacts: unknown;
  qaArtifacts: unknown;
  evidence: unknown;
}

export interface FinalPackageQaConsistency {
  attestationConsistencyMatched: true;
  archiveBytesVerified: false;
  publicationAuthorized: false;
  scope: "source-attestation-consistency-only";
  packageRunId: number;
  qaRunId: number;
  qaAttempt: number;
  targets: readonly FinalQaTarget[];
  windows: {
    policy: "require-success" | "shadow-observation";
    conclusion: "success" | "failure";
    scope: "required-success" | "shadow-observation-only";
  };
  limitations: readonly string[];
}
