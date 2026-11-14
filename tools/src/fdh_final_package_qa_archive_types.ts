// SPDX-License-Identifier: MPL-2.0

/** Claims from a caller; this module does not authenticate their API origin. */
export interface QaArchiveIdentity {
  id: number;
  name: string;
}

export interface QaArchiveRequest {
  bytes: Uint8Array;
  artifact: QaArchiveIdentity;
  expected: QaArchiveIdentity & { digest: string };
}

export type QaArchiveFailureCode =
  | "invalid-input"
  | "identity-mismatch"
  | "archive-limit"
  | "digest-mismatch"
  | "zip-structure"
  | "unsupported-zip"
  | "unsafe-path"
  | "expanded-limit"
  | "compressed-data"
  | "crc-mismatch"
  | "json-format";

export interface QaArchiveScope {
  scope: "archive-byte-integrity-only";
  publicationAuthorized: false;
  apiAuthenticityVerified: false;
  nativeQaVerified: false;
  liveApprovalVerified: false;
}

export type QaArchiveResult =
  & QaArchiveScope
  & (
    | {
      archiveIntegrityMatched: true;
      artifact: QaArchiveIdentity & { digest: string };
      input: Record<string, unknown>;
      sourceAfterQa: Record<string, unknown>;
      entryCount: number;
      expandedBytes: number;
    }
    | { archiveIntegrityMatched: false; code: QaArchiveFailureCode }
  );

/** Internal validated ZIP32 metadata; offsets always address the copied ZIP. */
export interface QaZipEntry {
  name: string;
  version: number;
  flags: number;
  method: number;
  time: number;
  date: number;
  crc: number;
  compressed: number;
  expanded: number;
  local: number;
  data: number;
}

/** node:zlib typings do not model info:true consistently across runtimes. */
export interface QaInflateInfo {
  buffer: Uint8Array;
  engine: { bytesWritten: number };
}
