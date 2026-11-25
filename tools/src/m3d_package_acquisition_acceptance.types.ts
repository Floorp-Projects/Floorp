// SPDX-License-Identifier: MPL-2.0

export type M3dAcceptanceCaseStatus = "not-run" | "passed" | "failed";

export const M3D_REQUIRED_CASE_IDS = [
  "trusted-bytes-reuse",
  "source-inert",
  "untrusted-tls",
  "redirect",
  "http-status",
  "content-type",
  "content-encoding",
  "declared-oversize",
  "streamed-oversize",
  "truncated-length",
  "read-failure",
  "wrong-package",
  "cookie-privacy",
  "caller-abort",
  "cancel-reuse",
  "ordinary-abort",
  "abort-reuse",
  "ordinary-network-failure",
  "network-reuse",
  "ordinary-read-failure",
  "read-reuse",
  "twenty-second-deadline",
  "raw-equal-length",
  "raw-conflicting-length",
  "final-reuse",
  "download-lifecycle-inert",
  "download-lifecycle-transactions",
  "close-during-download",
  "wrapper-contract",
  "connected-status",
] as const;

export interface M3dAcceptanceCaseResult {
  readonly id: string;
  readonly status: M3dAcceptanceCaseStatus;
  readonly observations: Readonly<
    Record<string, boolean | number | string | null>
  >;
}

export interface M3dProfileSnapshot {
  readonly exists: boolean;
  readonly entries: number;
  readonly byteLength: number;
  readonly sha256: string;
}

export interface M3dAcceptanceReport {
  readonly schemaVersion: 1;
  readonly runId: string;
  readonly startedAt: string;
  readonly completedAt: string | null;
  readonly passed: boolean;
  readonly cases: readonly M3dAcceptanceCaseResult[];
  readonly packageMetadata: readonly {
    readonly featureId: string;
    readonly version: string;
    readonly releaseSequence: number;
    readonly packageSha256: string;
    readonly byteLength: number;
  }[];
  readonly sharedProfileBefore: M3dProfileSnapshot | null;
  readonly sharedProfileAfter: M3dProfileSnapshot | null;
  readonly sharedProfileUnchanged: boolean | null;
  readonly launchFailure: string | null;
  readonly runFailure: string | null;
  readonly launchAttempted: boolean;
  readonly launchSucceeded: boolean;
  readonly browserCleanupProven: boolean;
  readonly cleanup: Readonly<Record<string, boolean | number | null>>;
}

export interface M3dAcceptanceVerdictInput {
  readonly complete: boolean;
  readonly cases: readonly M3dAcceptanceCaseResult[];
  readonly launchFailure: string | null;
  readonly runFailure: string | null;
  readonly launchAttempted: boolean;
  readonly launchSucceeded: boolean;
  readonly browserCleanupProven: boolean;
  readonly sharedProfileBefore: M3dProfileSnapshot | null;
  readonly sharedProfileAfter: M3dProfileSnapshot | null;
  readonly cleanup: Readonly<Record<string, boolean | number | null>>;
}
