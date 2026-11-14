// SPDX-License-Identifier: MPL-2.0

/**
 * Types for the OS-Server verification wrapper option resolver
 * (tools/os-test/run_verify_os_server_full_wrapper.ts, KAN-135).
 * Kept in a dedicated types file per repo convention.
 */

/** Raw wrapper inputs before resolution. */
export type OsVerifyRawOptions = {
  /** Value of --base-url, undefined when absent. */
  baseUrlFlag?: string;
  /** Value of FLOORP_OS_BASE_URL, undefined when unset. */
  envBaseUrl?: string;
  /** Resolved (non-"auto") port offset of the dev launch, if given. */
  portOffset?: number;
  /** --skip-start: do not start a browser, verify an existing target. */
  skipStart: boolean;
};

/** Resolved wrapper options. */
export type OsVerifyOptions = {
  /** Base URL used for the reachability wait and the verifier. */
  baseUrl: string;
  /** Extra arguments appended to the dev start command (--port-offset n). */
  startCommandArgs: string[];
  /** The resolved port offset backing baseUrl (0 when not dev-shifted). */
  portOffset: number;
};
