// SPDX-License-Identifier: MPL-2.0
import type { JWTVerifyGetKey } from "jose";

/** Trusted deployment configuration, never derived from the token or HTTP host. */
export interface SharedAccessPolicy {
  readonly issuer: string;
  readonly resourceAudience: string;
  readonly requiredScopes: readonly string[];
  /** Explicit local-only spike: permits HTTP on literal loopback, not localhost. */
  readonly localSpike?: boolean;
}

/** The caller pins the key source out of band; this module performs no discovery.
 * A resolver can do I/O, so the runtime must independently constrain its endpoint.
 * The optional clock is captured once and cloned before asynchronous key lookup. */
export interface SharedAccessTrustedDependencies {
  readonly resolveKey: JWTVerifyGetKey;
  readonly now?: () => Date;
}

/** Offline signature/claims/scope admission ONLY, not tool authorization.
 * subject is the authorization server's opaque sub, NOT Google identity_key.
 * Google binding, active introspection and root human/tool approval are separate. */
export interface VerifiedSharedAccessToken {
  readonly kind: "verified-shared-access-token";
  readonly issuer: string;
  readonly subject: string;
  readonly resourceAudience: string;
  readonly expiresAt: number;
  readonly scopes: readonly string[];
}

export type SharedAccessErrorCode =
  | "invalid-policy"
  | "invalid-dependencies"
  | "invalid-token"
  | "verification-failed"
  | "invalid-claims"
  | "insufficient-scope";
