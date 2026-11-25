// SPDX-License-Identifier: MPL-2.0

export type MarionetteRequest =
  | { op: "ready"; connectTimeout?: number }
  | { op: "eval"; script: string }
  | { op: "screenshot" };

export interface GuestRunOutput {
  code: number;
  stdout: string;
  stderr: string;
  /** The transport killed the local process after its timeout. */
  timedOut?: boolean;
}

/** Runs a remote script; `stdin` streams an upload when present. */
export type QaTransport = (
  script: string,
  stdin?: ReadableStream<Uint8Array>,
) => Promise<GuestRunOutput>;
