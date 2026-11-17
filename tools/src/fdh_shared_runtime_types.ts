// SPDX-License-Identifier: MPL-2.0
import type { SharedMcpTrustedDependencies } from "./fdh_shared_mcp_types.ts";

/** Trusted server-only capabilities. No environment enumeration or fallback. */
export interface SharedRuntimePrivateIO {
  readonly backend: "keycloak-readonly";
  readonly lookupSecret: (name: string) => unknown | Promise<unknown>;
  readonly fetch: typeof fetch;
  /** Unix seconds. Must be monotonic for the lifetime of this runtime. */
  readonly now: () => number;
}
/** Private closure state only; never included in a runtime result or diagnostics. */
export interface SharedRuntimeReaderGrant {
  readonly token: string;
  /** Conservative early-renew deadline, Unix seconds. */
  readonly until: number;
  /** AS grant expiry upper bound, Unix seconds. */
  readonly expiresAt: number;
}

/** Synthetic private-IO fixture observation; no credential-bearing fields. */
export interface SharedRuntimeFixtureCall {
  readonly url: string;
  readonly method: string;
}

export type SharedRuntimeResult =
  | {
    readonly mode: "unconfigured";
    readonly productionQualified: false;
    readonly protectedOperationsAllowed: false;
  }
  | {
    readonly mode: "configured";
    readonly dependencies: SharedMcpTrustedDependencies;
    readonly productionQualified: false;
    readonly protectedOperationsAllowed: false;
  };
