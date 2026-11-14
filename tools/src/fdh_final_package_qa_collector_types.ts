// SPDX-License-Identifier: MPL-2.0
import type { QaGhTransport } from "./fdh_qa_gh_transport_types.ts";
export interface QaCollectorOptions {
  readonly sourceRunId: number;
  readonly sourceAttempt: number;
  readonly sourceHead: string;
  readonly qaRunId: number;
  readonly qaAttempt: number;
  readonly qaHead: string;
  readonly windowsPolicy: "require-success" | "shadow-observation";
  readonly maxAgeMs: number;
}
/** Host/test seam only; never exposed as CLI configuration. */
export interface QaCollectorDependencies {
  readonly transport?: QaGhTransport;
  readonly now?: () => string;
}
export type QaCollectionStage =
  | "options"
  | "identity"
  | "inventory"
  | "versions"
  | "eligibility"
  | "archives"
  | "evidence"
  | "recheck";
export interface QaCollectionScope {
  readonly scope: "read-only-point-in-time-qa-collection";
  readonly transportAuthenticityVerified: false;
  readonly apiAuthenticityVerified: false;
  readonly nativeQaVerified: false;
  readonly liveApprovalVerified: false;
  readonly publicationAuthorized: false;
}
export type QaCollectionReport =
  & QaCollectionScope
  & (
    | {
      readonly matches: false;
      readonly stage: QaCollectionStage;
      readonly code:
        | "invalidOptions"
        | "collectionFailed"
        | "sourceNotReady"
        | "evidenceMismatch"
        | "changed";
    }
    | {
      readonly matches: true;
      readonly sourceRunId: number;
      readonly sourceAttempt: number;
      readonly qaRunId: number;
      readonly qaAttempt: number;
      readonly checkedAt: string;
      readonly archiveBytesVerified: true;
    }
  );
