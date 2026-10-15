// SPDX-License-Identifier: MPL-2.0

import type { TcpDial } from "../final-workspace-qa/types.ts";

export type BrowserOperationFailure =
  | { failed: false }
  | { failed: true; error: unknown };

export interface MarionetteConnectionOptions {
  /** Total bound covering TCP, greeting and the original session creation. */
  startupTimeoutMilliseconds?: number;
  /** Trusted controller dependency; defaults to real loopback TCP. */
  dial?: TcpDial;
  /** Trusted monotonic-clock dependency for host deadline fault injection. */
  now?: () => number;
}

export interface MarionetteScreenshotOptions {
  full?: boolean;
  element?: string;
}

export interface MarionetteBrowserConnectionOptions
  extends MarionetteConnectionOptions {
  port?: number;
}
