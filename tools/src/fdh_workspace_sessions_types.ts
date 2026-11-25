// SPDX-License-Identifier: MPL-2.0
import type { VerifiedWorkspaceIdentity } from "./fdh_workspace_oidc_types.ts";

/** One provider token response accepted by the in-memory registry. */
export interface WorkspaceSessionMaterial {
  readonly identity: VerifiedWorkspaceIdentity;
  readonly accessToken: string;
  readonly refreshToken?: string;
  /** Provider expiry in epoch milliseconds; no local session TTL is added. */
  readonly expiresAt: number;
}

/** Sanitized view returned by every public registry operation. */
export interface WorkspaceSessionSnapshot {
  readonly id: string;
  readonly identity: VerifiedWorkspaceIdentity;
  readonly expiresAt: number;
  readonly version: number;
  readonly scope: "workspace-oauth-session";
  readonly liveOAuthVerified: false;
  readonly mcpAuthorizationGranted: false;
}

/** Caller supplied clock, capacity, refresh, and optional revocation seams. */
export interface WorkspaceSessionOptions {
  readonly now: () => number;
  readonly maxSessions: number;
  readonly refresh: (
    refreshToken: string,
    identity: VerifiedWorkspaceIdentity,
  ) => Promise<WorkspaceSessionMaterial>;
  readonly revoke?: (token: string) => Promise<void>;
}

/** Constant failure classes; every error carries one code and a fixed message. */
export type WorkspaceSessionErrorCode =
  | "invalid-options"
  | "invalid-material"
  | "invalid-session-id"
  | "invalid-clock"
  | "capacity-exceeded"
  | "session-not-found"
  | "refresh-failed";

/** Constant local logout result; provider confirmation is a boolean only. */
export interface WorkspaceSessionLogoutResult {
  readonly localLogout: true;
  readonly providerRevocationConfirmed: boolean;
}

/** Internal private record shape; never returned to callers. */
export interface WorkspaceSessionRecord {
  readonly identity: VerifiedWorkspaceIdentity;
  readonly accessToken: string;
  readonly refreshToken?: string;
  readonly expiresAt: number;
  readonly snapshot: WorkspaceSessionSnapshot;
  readonly generation: number;
}

/** Internal single-flight marker shared by concurrent refresh callers. */
export interface WorkspaceRefreshFlight {
  readonly promise: Promise<WorkspaceSessionSnapshot>;
  readonly generation: number;
}

/** Internal material copy captured before the provider refresh await. */
export interface WorkspaceSessionMaterialSnapshot {
  readonly identity: VerifiedWorkspaceIdentity;
  readonly accessToken: string;
  readonly refreshToken?: string;
  readonly expiresAt: number;
}
