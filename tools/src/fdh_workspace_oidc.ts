// SPDX-License-Identifier: MPL-2.0
import {
  createRemoteJWKSet,
  type JWTPayload,
  jwtVerify,
  type JWTVerifyGetKey,
  type RemoteJWKSetOptions,
} from "jose";
import type {
  VerifiedWorkspaceIdentity,
  WorkspaceIdTokenExpectation,
  WorkspaceOidcDependencies,
  WorkspaceOidcErrorCode,
} from "./fdh_workspace_oidc_types.ts";

/** Google OpenID Connect issuers accepted verbatim; never caller supplied. */
export const GOOGLE_ID_TOKEN_ISSUERS = Object.freeze(
  [
    "https://accounts.google.com",
    "accounts.google.com",
  ] as const,
);

/** The only Google key-set endpoint this module may ever request. */
export const GOOGLE_ID_TOKEN_JWKS_URL =
  "https://www.googleapis.com/oauth2/v3/certs";

/** Five second request timeout, thirty second cooldown, five minute cache. */
export const GOOGLE_JWKS_OPTIONS: RemoteJWKSetOptions = Object.freeze({
  timeoutDuration: 5_000,
  cooldownDuration: 30_000,
  cacheMaxAge: 5 * 60_000,
});

export const WORKSPACE_HOSTED_DOMAIN = "floorp.app";
export const WORKSPACE_EMAIL_PATTERN = /^[^@\s]+@floorp\.app$/i;
/** Opaque identity string: printable ASCII only, length 1 to 255. */
export const IDENTITY_PATTERN = /^[\x20-\x7E]{1,255}$/;
export const ALLOWED_ALGORITHMS = Object.freeze(["RS256"] as const);
export const MAX_ID_TOKEN_BYTES = 16 * 1024;
export const MAX_CLIENT_ID_CHARS = 256;
export const MAX_NONCE_CHARS = 256;

/** Constant messages. No untrusted value is ever interpolated into them. */
export const ERROR_MESSAGES = Object.freeze(
  {
    "invalid-expectation": "Workspace OIDC expectation rejected",
    "invalid-token": "Workspace OIDC ID token rejected before verification",
    "invalid-claims": "Workspace OIDC ID token claims rejected",
    "verification-failed": "Workspace OIDC ID token verification failed",
  } as const satisfies Record<WorkspaceOidcErrorCode, string>,
);

/** Verification failure that carries only a constant code and message. */
export class WorkspaceOidcError extends Error {
  readonly code: WorkspaceOidcErrorCode;

  constructor(code: WorkspaceOidcErrorCode) {
    super(ERROR_MESSAGES[code]);
    this.name = "WorkspaceOidcError";
    this.code = code;
  }
}

let cachedGoogleKeyResolver: JWTVerifyGetKey | undefined;

/**
 * Fixed-endpoint key resolver. Constructing it performs no network I/O. A
 * default-path verification does perform one request to that fixed endpoint
 * the first time a key is actually needed, and then reuses the key document
 * within the cooldown and cache windows configured above.
 */
export function createGoogleKeyResolver(): JWTVerifyGetKey {
  if (cachedGoogleKeyResolver === undefined) {
    const remote = createRemoteJWKSet(
      new URL(GOOGLE_ID_TOKEN_JWKS_URL),
      GOOGLE_JWKS_OPTIONS,
    );
    const resolver: JWTVerifyGetKey = (header, token) => remote(header, token);
    cachedGoogleKeyResolver = resolver;
  }
  return cachedGoogleKeyResolver;
}

/**
 * Verify a Google-signed OpenID Connect ID token against the fixed Floorp
 * Workspace rules and return the verified identity.
 *
 * Scope is workspace ID-token verification only: no authorization server, no
 * token-endpoint interaction, no client credential handling, no storage, and
 * no session or refresh state. A successful result reports liveOAuthVerified
 * false and mcpAuthorizationGranted false on purpose; AC-05 is not satisfied
 * by this function.
 *
 * Exactly one clock reading is taken from the optional test seam, cloned, and
 * used for every timing decision, including the one handed to the library.
 * Expiry is judged only against the issuer "exp" claim and that captured
 * clock: no application maximum token age is imposed, because no such cap is
 * an approved requirement, and this function sets no session or MCP
 * authorization policy.
 *
 * @param idToken Compact JWS encoded ID token, bounded before any crypto work.
 * @param expectation Required audience target and required replay nonce.
 * @param dependencies Optional key source and clock seam, tests only.
 */
export async function verifyWorkspaceIdToken(
  idToken: string,
  expectation: WorkspaceIdTokenExpectation,
  dependencies?: WorkspaceOidcDependencies,
): Promise<VerifiedWorkspaceIdentity> {
  assertToken(idToken);
  const snapshot = snapshotExpectation(expectation);
  const now = readClock(dependencies);
  const resolveKey = readResolveKey(dependencies) ?? createGoogleKeyResolver();
  let payload: JWTPayload;
  try {
    const verified = await jwtVerify(idToken, resolveKey, {
      algorithms: [...ALLOWED_ALGORITHMS],
      issuer: [...GOOGLE_ID_TOKEN_ISSUERS],
      audience: snapshot.clientId,
      clockTolerance: 0,
      currentDate: now,
    });
    payload = verified.payload;
  } catch {
    // Library failures can carry raw claim values, so none of them is
    // propagated: only the constant code and message leave this boundary.
    throw new WorkspaceOidcError("verification-failed");
  }
  return readVerifiedClaims(payload, snapshot, now);
}

/**
 * Single clock capture for one verification. The returned Date is a private
 * clone, so a caller mutating its own object while a key lookup is awaiting
 * cannot change any timing decision here. Clock failures are redacted to the
 * constant expectation message.
 */
function readClock(dependencies?: WorkspaceOidcDependencies): Date {
  try {
    const clock = dependencies?.now;
    if (clock !== undefined && typeof clock !== "function") {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    const value = clock === undefined ? new Date() : clock();
    if (!(value instanceof Date)) {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    // Read the Date internal slot exactly once, never a caller override.
    const milliseconds = Date.prototype.getTime.call(value);
    if (!Number.isFinite(milliseconds)) {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    return new Date(milliseconds);
  } catch {
    // Even our Error class can be thrown by callers with a modified message.
    throw new WorkspaceOidcError("invalid-expectation");
  }
}

/** Read the optional test-seam key source once, redacting any failure. */
function readResolveKey(
  dependencies?: WorkspaceOidcDependencies,
): JWTVerifyGetKey | undefined {
  try {
    const resolver = dependencies?.resolveKey;
    if (resolver !== undefined && typeof resolver !== "function") {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    return resolver;
  } catch {
    // Even our Error class can be thrown by callers with a modified message.
    throw new WorkspaceOidcError("invalid-expectation");
  }
}

/** Reject oversized or non text tokens before any cryptographic work. */
function assertToken(value: unknown): void {
  if (typeof value !== "string" || value.length === 0) {
    throw new WorkspaceOidcError("invalid-token");
  }
  if (value.length > MAX_ID_TOKEN_BYTES) {
    throw new WorkspaceOidcError("invalid-token");
  }
  // The cheap test above bounds the encoding allocation; this second test
  // enforces the sixteen kibibyte ceiling on the encoded bytes, still ahead
  // of any signature or key work.
  if (new TextEncoder().encode(value).byteLength > MAX_ID_TOKEN_BYTES) {
    throw new WorkspaceOidcError("invalid-token");
  }
}

/**
 * Read the two required fields exactly once, as own non-accessor data
 * properties, and return a frozen local snapshot. Only the snapshot is handed
 * to the library and to claim mapping, so mutating or reconfiguring the
 * caller's object while verification is awaiting cannot change the audience
 * or nonce binding of this verification. Accessors are never invoked, and
 * exotic property traps that throw are redacted to the constant message.
 */
function snapshotExpectation(
  expectation: WorkspaceIdTokenExpectation,
): WorkspaceIdTokenExpectation {
  const record: unknown = expectation;
  if (typeof record !== "object" || record === null) {
    throw new WorkspaceOidcError("invalid-expectation");
  }
  const clientId = readDataString(record, "clientId");
  const expectedNonce = readDataString(record, "expectedNonce");
  if (
    clientId.length < 1 ||
    clientId.length > MAX_CLIENT_ID_CHARS ||
    expectedNonce.length < 1 ||
    expectedNonce.length > MAX_NONCE_CHARS
  ) {
    throw new WorkspaceOidcError("invalid-expectation");
  }
  return Object.freeze({ clientId, expectedNonce });
}

function readDataString(record: object, field: string): string {
  try {
    const descriptor = Object.getOwnPropertyDescriptor(record, field);
    if (descriptor === undefined || !("value" in descriptor)) {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    const value: unknown = descriptor.value;
    if (typeof value !== "string") {
      throw new WorkspaceOidcError("invalid-expectation");
    }
    return value;
  } catch {
    // Even our Error class can be thrown by callers with a modified message.
    throw new WorkspaceOidcError("invalid-expectation");
  }
}

/** NumericDate claims must be safe integers; conservative for identity work. */
function numericDate(value: unknown): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value)) {
    throw new WorkspaceOidcError("invalid-claims");
  }
  return value;
}

function epoch(value: Date): number {
  return Math.floor(value.getTime() / 1000);
}

/**
 * Strict Workspace claim rules over an already signature-verified payload.
 * Every rejection uses the constant invalid-claims message.
 */
function readVerifiedClaims(
  payload: JWTPayload,
  expectation: WorkspaceIdTokenExpectation,
  now: Date,
): VerifiedWorkspaceIdentity {
  const { clientId, expectedNonce } = expectation;

  if (payload.nonce !== expectedNonce) {
    throw new WorkspaceOidcError("invalid-claims");
  }

  const email = payload.email;
  if (typeof email !== "string" || !WORKSPACE_EMAIL_PATTERN.test(email)) {
    throw new WorkspaceOidcError("invalid-claims");
  }
  if (payload.email_verified !== true) {
    throw new WorkspaceOidcError("invalid-claims");
  }

  const hostedDomain = payload.hd;
  if (
    typeof hostedDomain !== "string" ||
    hostedDomain !== WORKSPACE_HOSTED_DOMAIN
  ) {
    throw new WorkspaceOidcError("invalid-claims");
  }

  const identityKey = payload.sub;
  if (typeof identityKey !== "string" || !IDENTITY_PATTERN.test(identityKey)) {
    throw new WorkspaceOidcError("invalid-claims");
  }

  const issuer = payload.iss;
  if (typeof issuer !== "string") {
    throw new WorkspaceOidcError("invalid-claims");
  }

  const issuedAt = numericDate(payload.iat);
  const expiresAt = numericDate(payload.exp);
  if (issuedAt > epoch(now)) {
    throw new WorkspaceOidcError("invalid-claims");
  }
  if (expiresAt <= issuedAt) {
    throw new WorkspaceOidcError("invalid-claims");
  }
  if (payload.nbf !== undefined) {
    numericDate(payload.nbf);
  }

  const audience = payload.aud;
  let authorizedParty: string | undefined;
  if (Array.isArray(audience)) {
    if (audience.length < 1) {
      throw new WorkspaceOidcError("invalid-claims");
    }
    for (const element of audience) {
      if (typeof element !== "string" || element.length < 1) {
        throw new WorkspaceOidcError("invalid-claims");
      }
    }
    const azp = payload.azp;
    if (typeof azp !== "string" || azp !== clientId) {
      throw new WorkspaceOidcError("invalid-claims");
    }
    authorizedParty = azp;
  } else if (typeof audience === "string") {
    const azp = payload.azp;
    if (azp !== undefined) {
      if (typeof azp !== "string" || azp !== clientId) {
        throw new WorkspaceOidcError("invalid-claims");
      }
      authorizedParty = azp;
    }
  } else {
    throw new WorkspaceOidcError("invalid-claims");
  }

  return {
    scope: "workspace-id-token-verification-only",
    liveOAuthVerified: false,
    mcpAuthorizationGranted: false,
    identityKey,
    email,
    hostedDomain,
    issuer,
    audience: typeof audience === "string" ? audience : [...audience],
    authorizedParty,
    issuedAt,
    expiresAt,
    notBefore: payload.nbf,
  };
}
