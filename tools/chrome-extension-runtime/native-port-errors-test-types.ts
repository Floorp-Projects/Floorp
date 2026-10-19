// SPDX-License-Identifier: MPL-2.0
export interface NativePortLastError {
  message: string;
  keys: string[];
  isError: boolean;
  nameType: string;
}
export interface NativePortErrorSnapshot {
  listener: number;
  phase: "callback" | "microtask" | "next-turn";
  samePort: boolean;
  lastError: NativePortLastError | null;
  portError: { message: string; name: string } | null;
}
export interface NativePortErrorResult {
  isWorker: boolean;
  runtimeId: string;
  before: string | null;
  normal: boolean;
  throwFirst: boolean;
  throwReached: boolean;
  records: NativePortErrorSnapshot[];
  microtasks: NativePortErrorSnapshot[];
  turns: NativePortErrorSnapshot[];
  after: string | null;
}
export interface NativePortErrorResponse {
  requestId: string;
  result?: NativePortErrorResult;
  error?: string;
}
export interface NativePortErrorsReport {
  passed: boolean;
  runId: string;
  cases: {
    caller: string;
    mode: "document" | "worker";
    result: NativePortErrorResult;
  }[];
  error?: string;
}
