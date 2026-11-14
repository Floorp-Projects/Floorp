// SPDX-License-Identifier: MPL-2.0
/** Canonical capabilities only: no URL, repository, executable or extra arguments. */
export type QaGhOperation =
  | { readonly kind: "repository" }
  | { readonly kind: "workflow"; readonly workflow: "source" | "qa" }
  | { readonly kind: "currentRun"; readonly runId: number }
  | {
    readonly kind: "attemptRun";
    readonly runId: number;
    readonly attempt: number;
  }
  | {
    readonly kind: "jobsPage";
    readonly runId: number;
    readonly attempt: number;
    readonly page: number;
  }
  | {
    readonly kind: "artifactsPage";
    readonly runId: number;
    readonly page: number;
  }
  | {
    readonly kind: "qaRunsPage";
    readonly since: string;
    readonly page: number;
  }
  | { readonly kind: "artifactMetadata"; readonly artifactId: number }
  | { readonly kind: "selectedZip"; readonly artifactId: number }
  | {
    readonly kind: "pinnedContents";
    readonly file: "package" | "runtimeLock";
    readonly head: string;
  }
  | { readonly kind: "pendingDeployments"; readonly sourceRunId: number }
  | { readonly kind: "releaseTag"; readonly version: string };

export type QaGhStatusClass =
  | "none"
  | "success"
  | "unauthorized"
  | "forbidden"
  | "notFound"
  | "clientError"
  | "serverError"
  | "other";
export type QaGhErrorCode =
  | "invalidOperation"
  | "busy"
  | "aborted"
  | "timeout"
  | "sessionTimeout"
  | "clock"
  | "stdoutLimit"
  | "stderrLimit"
  | "sessionLimit"
  | "stream"
  | "spawn"
  | "process"
  | "http"
  | "protocol"
  | "metadata";
export type QaGhResult =
  | {
    readonly ok: true;
    readonly kind: "metadata";
    readonly jsonText: string;
    readonly publicationAuthorized: false;
  }
  | {
    readonly ok: true;
    readonly kind: "zip";
    readonly bytes: Uint8Array<ArrayBuffer>;
    readonly publicationAuthorized: false;
  }
  | {
    readonly ok: false;
    readonly code: QaGhErrorCode;
    readonly statusClass: QaGhStatusClass;
  };
export interface QaGhTransport {
  request(operation: QaGhOperation, signal?: AbortSignal): Promise<QaGhResult>;
}
/** Test dependency seam ONLY; not an executable/CLI configuration surface. */
export interface QaGhTestChild {
  readonly stdout: ReadableStream<Uint8Array>;
  readonly stderr: ReadableStream<Uint8Array>;
  readonly status: Promise<
    { readonly success: boolean; readonly code: number }
  >;
  kill(signal: "SIGKILL"): void;
}
export interface QaGhTestDependencies {
  spawn(args: readonly string[]): QaGhTestChild;
  now(): number;
  setTimer(callback: () => void, milliseconds: number): unknown;
  clearTimer(handle: unknown): void;
}
export interface QaGhEndpoint {
  readonly path: string;
  readonly zip: boolean;
}
