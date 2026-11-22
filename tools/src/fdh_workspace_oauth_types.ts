// SPDX-License-Identifier: MPL-2.0
import type {
  authorizationCodeGrant,
  Configuration,
  customFetch,
} from "openid-client";
import type { WorkspaceOidcDependencies } from "./fdh_workspace_oidc_types.ts";

/** A provider client, NOT an authorization server or MCP credential issuer. */
export interface WorkspaceOAuthOptions {
  readonly clientId: string;
  readonly clientSecret?: string;
  readonly redirectUri: string;
  readonly authorizationTtlMs: number;
  readonly maxPending: number;
  readonly maxSessions: number;
}
/** Trusted offline test seams only. The operator CLI exposes none of these. */
export interface WorkspaceOAuthDependencies {
  readonly now?: () => number;
  readonly fetch?: NonNullable<Configuration[typeof customFetch]>;
  readonly oidc?: WorkspaceOidcDependencies;
}
/** Offline protocol test observation, never a production evidence record. */
export interface WorkspaceOAuthTestRequest {
  readonly url: string;
  readonly params: URLSearchParams;
  readonly redirect: RequestRedirect | undefined;
}

export interface WorkspaceAuthorization {
  readonly authorizationUrl: string;
  readonly expiresAt: number;
}
export interface WorkspaceOAuthPending {
  readonly nonce: string;
  readonly verifier: string;
  readonly expiresAt: number;
}
export type WorkspaceOAuthTokenResponse = Awaited<
  ReturnType<typeof authorizationCodeGrant>
>;
export type WorkspaceOAuthErrorCode =
  | "configuration-rejected"
  | "authorization-rejected"
  | "callback-rejected"
  | "exchange-rejected"
  | "refresh-rejected";
