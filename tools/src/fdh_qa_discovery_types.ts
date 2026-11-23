// SPDX-License-Identifier: MPL-2.0
import type {
  QaCollectionReport,
  QaCollectorDependencies,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";
import type { QaGhTransport } from "./fdh_qa_gh_transport_types.ts";

/** Source-side bindings only: the QA run is discovered, never assumed. */
export interface QaDiscoveryOptions {
  readonly sourceRunId: number;
  readonly sourceAttempt: number;
  readonly sourceHead: string;
  readonly windowsPolicy: "require-success" | "shadow-observation";
  readonly maxAgeMs: number;
}

/** Test dependency seam ONLY; never an executable/CLI configuration surface. */
export type QaDiscoveryCollect = (
  options: QaCollectorOptions,
  deps?: QaCollectorDependencies,
) => Promise<QaCollectionReport>;

/** Test dependency seam ONLY; never an executable/CLI configuration surface. */
export interface QaDiscoveryDependencies {
  readonly transport?: QaGhTransport;
  readonly now?: () => string;
  readonly collect?: QaDiscoveryCollect;
}

/** Internal projection of one validated QA listing entry. */
export interface QaDiscoveryCandidate {
  readonly id: number;
  readonly attempt: number;
  readonly head: string;
}

/** Test fixture specification for one synthetic QA candidate. */
export interface QaDiscoveryCandidateSpec {
  readonly id: number;
  /** runId written inside the QA archive input.json binding. */
  readonly boundSourceRunId: number;
  readonly windows: "success" | "failure";
  /** Broken candidate: incomplete job inventory (collector uncertainty). */
  readonly jobs?: 3;
  /** Run data attempt differing from the listing attempt. */
  readonly dataAttempt?: number;
}

/** Internal classification of one listed run: a completed candidate with its
 * canonical fingerprint, or an unsettled pending observation. */
export type QaDiscoveryClassifiedRun =
  | {
    readonly kind: "candidate";
    readonly candidate: QaDiscoveryCandidate;
    readonly fingerprint: Record<string, unknown>;
  }
  | { readonly kind: "pending"; readonly fingerprint: Record<string, unknown> };

export type QaDiscoveryStage =
  | "options"
  | "source"
  | "runList"
  | "candidates"
  | "finalRecheck";

export type QaDiscoveryFailureCode =
  | "invalidOptions"
  | "sourceRunFailed"
  | "runListFailed"
  | "paginationIncomplete"
  | "candidateCapExceeded"
  | "pendingQaRun"
  | "noValidRun"
  | "ambiguous"
  | "uncertainCandidate"
  | "changed"
  | "finalRecheckFailed";

export interface QaDiscoveryScope {
  readonly scope: "read-only-point-in-time-qa-discovery";
  readonly transportAuthenticityVerified: false;
  readonly apiAuthenticityVerified: false;
  readonly nativeQaVerified: false;
  readonly liveApprovalVerified: false;
  readonly publicationAuthorized: false;
}

export type QaDiscoveryReport =
  & QaDiscoveryScope
  & (
    | {
      readonly matches: false;
      readonly stage: QaDiscoveryStage;
      readonly code: QaDiscoveryFailureCode;
    }
    | {
      readonly matches: true;
      readonly sourceRunId: number;
      readonly sourceAttempt: number;
      readonly sourceHead: string;
      readonly qaRunId: number;
      readonly qaAttempt: number;
      readonly qaHead: string;
      readonly checkedAt: string;
      readonly archiveBytesVerified: true;
      readonly runsListed: number;
      readonly pagesFetched: number;
      readonly candidatesChecked: number;
    }
  );
