// SPDX-License-Identifier: MPL-2.0
import type {
  QaDispatchHttp,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";

export interface QaDispatchWireExitStatus {
  readonly success: boolean;
  readonly code: number;
}
export interface QaDispatchWireHeaderResult {
  readonly status: number;
  readonly body: Uint8Array;
}
export type QaDispatchWireTestSpawn = readonly [string[], string | null];
export interface QaDispatchWireTestFixture {
  readonly deps: QaDispatchWireTestDependencies;
  readonly spawns: readonly QaDispatchWireTestSpawn[];
}

/** Narrow wire seam imported by the assignment runtime; no CLI-supplied repo, path, exe, or flags. */
export interface QaDispatchWireTransport {
  mainRef(): Promise<QaDispatchHttp>;
  dispatch(request: QaDispatchRequest): Promise<QaDispatchHttp>;
}

/** Synthetic test seam only; never wired to CLI/model-controlled options. */
export interface QaDispatchWireTestChild {
  readonly stdin?: WritableStream<Uint8Array>;
  readonly stdout: ReadableStream<Uint8Array>;
  readonly stderr: ReadableStream<Uint8Array>;
  readonly status: Promise<QaDispatchWireExitStatus>;
  kill(signal: "SIGKILL"): void;
}

export interface QaDispatchWireTestDependencies {
  spawn(
    args: readonly string[],
    stdinBody: string | null,
  ): QaDispatchWireTestChild;
  now(): number;
  setTimer(callback: () => void, milliseconds: number): unknown;
  clearTimer(handle: unknown): void;
}
