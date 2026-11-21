// SPDX-License-Identifier: MPL-2.0
import type {
  FinalPackageQaConsistency,
  FinalQaTarget,
} from "./fdh_final_package_qa_gate_types.ts";

/** Canonical JSON.stringify(value, null, 2), with optional final LF. Policy is
 * independently trusted input; this API does not establish its origin. */
export interface FinalPackageQaEvidenceRequest {
  expectedJson: string;
  observationJson: string;
  archives: readonly { artifactId: number; bytes: Uint8Array }[];
}
export interface EvidenceByteSnapshot {
  artifactId: number;
  bytes: Uint8Array;
  length: number;
}
export interface FinalPackageQaEvidenceScope {
  scope: "offline-archive-integrity-and-attestation-consistency-only";
  transportAuthenticityVerified: false;
  apiAuthenticityVerified: false;
  nativeQaVerified: false;
  liveApprovalVerified: false;
  publicationAuthorized: false;
}
export type FinalPackageQaEvidenceResult =
  & FinalPackageQaEvidenceScope
  & (
    | {
      offlineEvidenceMatched: false;
      stage: "snapshot" | "mapping" | "archive" | "consistency";
    }
    | {
      offlineEvidenceMatched: true;
      offlineArchiveIntegrity: {
        archiveIntegrityMatched: true;
        artifacts: readonly {
          target: FinalQaTarget;
          id: number;
          name: string;
          digest: string;
          entryCount: number;
          expandedBytes: number;
        }[];
      };
      attestationConsistency: FinalPackageQaConsistency;
    }
  );
