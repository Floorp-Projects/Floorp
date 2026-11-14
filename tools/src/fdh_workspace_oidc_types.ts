// SPDX-License-Identifier: MPL-2.0
import type { generateKeyPair, JWK, JWTVerifyGetKey } from "jose";

/** Ephemeral synthetic signing material used only by the offline tests. */
export interface WorkspaceOidcTestSigningKey {
  readonly kid: string;
  readonly privateKey: Awaited<
    ReturnType<typeof generateKeyPair>
  >["privateKey"];
  readonly jwk: JWK;
}

export type WorkspaceOidcTestOutcome = WorkspaceOidcErrorCode | "fulfilled";

/**
 * Failure classes for offline ID-token verification. Every failure is reported
 * with one of these codes plus the constant message defined in the runtime
 * module; no token bytes and no claim values are ever carried on an error.
 */
export type WorkspaceOidcErrorCode =
  | "invalid-expectation"
  | "invalid-token"
  | "invalid-claims"
  | "verification-failed";

/**
 * Caller contract for one verification. Both fields are mandatory: the
 * audience target of the request and the replay nonce this specific request
 * issued. Neither may be defaulted by this module.
 *
 * Both fields are read exactly once, before any asynchronous work, and must be
 * own non-accessor data properties. Accessors, inherited properties and
 * throwing property traps are rejected with invalid-expectation before any key
 * is resolved, and the values are then used from a frozen local snapshot for
 * the rest of the verification.
 */
export interface WorkspaceIdTokenExpectation {
  readonly clientId: string;
  readonly expectedNonce: string;
}

/**
 * Test seam, and the only place a key source other than the fixed Google key
 * set can enter. Production callers omit this value entirely. When present,
 * "now" is read exactly once per verification and drives every timing decision
 * including the one handed to the library.
 */
export interface WorkspaceOidcDependencies {
  readonly resolveKey: JWTVerifyGetKey;
  readonly now?: () => Date;
}

/**
 * Successful verification result. The only claim made is that a Google-signed
 * ID token matched the fixed Floorp Workspace rules. No authorization server,
 * no client credential handling, no refresh or logout state, and no MCP
 * authorization is produced or implied: liveOAuthVerified and
 * mcpAuthorizationGranted stay false, and AC-05 remains unsatisfied.
 */
export interface VerifiedWorkspaceIdentity {
  /** The result scope, so callers cannot widen it by convention. */
  readonly scope: "workspace-id-token-verification-only";
  /** No live OAuth round trip was performed by this module. */
  readonly liveOAuthVerified: false;
  /** No MCP authorization was granted by this module. */
  readonly mcpAuthorizationGranted: false;
  /** The "sub" claim, opaque and never normalized. */
  readonly identityKey: string;
  /** The raw "email" claim, unmodified. */
  readonly email: string;
  /** The "hd" claim, exactly "floorp.app". */
  readonly hostedDomain: string;
  /** The "iss" claim as verified against the fixed issuer list. */
  readonly issuer: string;
  /** The "aud" claim as verified to contain the expected client id. */
  readonly audience: string | readonly string[];
  /** The "azp" claim when present (and required for multi-audience tokens). */
  readonly authorizedParty?: string;
  /** The "iat" claim in epoch seconds. */
  readonly issuedAt: number;
  /** The "exp" claim in epoch seconds. */
  readonly expiresAt: number;
  /** The optional "nbf" claim in epoch seconds. */
  readonly notBefore?: number;
}
