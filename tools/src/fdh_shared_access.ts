// SPDX-License-Identifier: MPL-2.0
import { jwtVerify } from "jose";
import type { JWTPayload } from "jose";
import type {
  SharedAccessErrorCode,
  SharedAccessPolicy,
  SharedAccessTrustedDependencies,
  VerifiedSharedAccessToken,
} from "./fdh_shared_access_types.ts";

const MAX_TOKEN_CHARS = 16 * 1024;
const MAX_SCOPES = 128;
// OAuth scope-token (RFC 6749): printable ASCII excluding space, quote, backslash.
const SCOPE_TOKEN = /^[\x21\x23-\x5B\x5D-\x7E]{1,256}$/;
const TEXT = /^[\x21-\x7E]{1,2048}$/;

/** Constant diagnostics only: no cause, token, resolver error or claim values. */
export class SharedAccessError extends Error {
  readonly code: SharedAccessErrorCode;
  constructor(code: SharedAccessErrorCode) {
    super("Shared access token rejected");
    this.name = "SharedAccessError";
    this.code = code;
  }
}

/** RS admission against existing AS keys, not an AS or a tool-approval decision.
 * No discovery, fetch, Google binding, introspection, token issuance or storage.
 * Resource aud must match exactly (or be an exact member of an aud array);
 * azp/client ID cannot substitute for aud. All policy inputs are copied before
 * await. No arbitrary maximum age is imposed beyond the AS expiry/nbf claims. */
export async function verifySharedAccessToken(
  rawToken: string,
  policy: SharedAccessPolicy,
  trustedDependencies: SharedAccessTrustedDependencies,
): Promise<VerifiedSharedAccessToken> {
  // Compact JWS is ASCII; length therefore also bounds bytes and JSON parsing.
  if (
    typeof rawToken !== "string" || rawToken.length > MAX_TOKEN_CHARS ||
    !/^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$/.test(rawToken)
  ) {
    throw new SharedAccessError("invalid-token");
  }
  const pinned = snapshotPolicy(policy);
  // Snapshot dependencies too: no getters and no mutable Date handed to jose.
  const { resolveKey, now } = snapshotDependencies(trustedDependencies);
  let payload: JWTPayload;
  try {
    payload = (await jwtVerify(rawToken, resolveKey, {
      algorithms: ["RS256"],
      issuer: pinned.issuer,
      audience: pinned.resourceAudience,
      requiredClaims: ["iss", "aud", "sub", "exp"],
      clockTolerance: 0,
      currentDate: now,
    })).payload;
  } catch {
    throw new SharedAccessError("verification-failed");
  }
  if (
    payload.typ !== "Bearer" ||
    typeof payload.sub !== "string" || payload.sub.length === 0 ||
    payload.sub.length > 255 ||
    [...payload.sub].some((char) =>
      char.charCodeAt(0) <= 32 || char.charCodeAt(0) === 127
    ) ||
    typeof payload.exp !== "number" || !Number.isSafeInteger(payload.exp) ||
    (payload.nbf !== undefined && !Number.isSafeInteger(payload.nbf))
  ) {
    throw new SharedAccessError("invalid-claims");
  }
  const audience = payload.aud;
  if (
    Array.isArray(audience) &&
    (audience.length < 1 || audience.length > MAX_SCOPES ||
      audience.some((item) => typeof item !== "string" || !TEXT.test(item)))
  ) {
    throw new SharedAccessError("invalid-claims");
  }
  const scopes = parseScopes(payload.scope);
  if (!pinned.requiredScopes.every((scope) => scopes.includes(scope))) {
    throw new SharedAccessError("insufficient-scope");
  }
  return Object.freeze({
    kind: "verified-shared-access-token",
    issuer: pinned.issuer,
    subject: payload.sub,
    resourceAudience: pinned.resourceAudience,
    expiresAt: payload.exp,
    scopes: Object.freeze(scopes),
  });
}

/** Reject inherited/accessor configuration fields without evaluating getters. */
function own(record: unknown, field: string): unknown {
  if (typeof record !== "object" || record === null) throw Error();
  const descriptor = Object.getOwnPropertyDescriptor(record, field);
  if (descriptor === undefined) return undefined;
  if (!("value" in descriptor)) throw Error();
  return descriptor.value;
}

function snapshotPolicy(policy: SharedAccessPolicy): SharedAccessPolicy {
  try {
    const issuer = own(policy, "issuer");
    const resourceAudience = own(policy, "resourceAudience");
    const required = own(policy, "requiredScopes");
    const localSpike = own(policy, "localSpike");
    if (
      typeof issuer !== "string" || !TEXT.test(issuer) ||
      typeof resourceAudience !== "string" || !TEXT.test(resourceAudience) ||
      (localSpike !== undefined && typeof localSpike !== "boolean") ||
      !Array.isArray(required)
    ) throw Error();
    validateIssuer(issuer, localSpike === true);
    const length = own(required, "length");
    if (
      typeof length !== "number" || !Number.isSafeInteger(length) ||
      length < 1 || length > MAX_SCOPES
    ) throw Error();
    const requiredScopes: string[] = [];
    for (let index = 0; index < length; index++) {
      const scope = own(required, String(index));
      if (
        typeof scope !== "string" || !SCOPE_TOKEN.test(scope) ||
        requiredScopes.includes(scope)
      ) throw Error();
      requiredScopes.push(scope);
    }
    return Object.freeze({
      issuer,
      resourceAudience,
      requiredScopes: Object.freeze(requiredScopes),
      localSpike,
    });
  } catch {
    throw new SharedAccessError("invalid-policy");
  }
}

function validateIssuer(issuer: string, localSpike: boolean): void {
  const url = new URL(issuer);
  // Require raw authority to equal canonical host, blocking WHATWG's conversion
  // of integer/octal/short IPv4 into loopback. Never normalize the issuer match.
  const authority = /^[a-z]+:\/\/([^/]+)(?:\/|$)/.exec(issuer)?.[1];
  if (
    url.username || url.password || url.search || url.hash ||
    issuer.includes("?") || issuer.includes("#") || issuer.includes("\\") ||
    authority !== url.host
  ) throw Error();
  if (url.protocol === "https:") return;
  if (
    localSpike && url.protocol === "http:" &&
    (url.hostname === "127.0.0.1" || url.hostname === "[::1]")
  ) return;
  throw Error();
}

function snapshotDependencies(dependencies: SharedAccessTrustedDependencies) {
  try {
    const resolveKey = own(dependencies, "resolveKey");
    const clock = own(dependencies, "now");
    if (
      typeof resolveKey !== "function" ||
      (clock !== undefined && typeof clock !== "function")
    ) throw Error();
    const value: unknown = clock === undefined ? new Date() : clock();
    const milliseconds = Date.prototype.getTime.call(value);
    if (!Number.isFinite(milliseconds)) throw Error();
    return {
      resolveKey: resolveKey as SharedAccessTrustedDependencies["resolveKey"],
      now: new Date(milliseconds),
    };
  } catch {
    throw new SharedAccessError("invalid-dependencies");
  }
}

function parseScopes(value: unknown): string[] {
  if (typeof value !== "string" || value.length > MAX_TOKEN_CHARS) {
    throw new SharedAccessError("invalid-claims");
  }
  const scopes = value.split(" ");
  if (
    scopes.length < 1 || scopes.length > MAX_SCOPES ||
    scopes.some((scope) => !SCOPE_TOKEN.test(scope))
  ) {
    throw new SharedAccessError("invalid-claims");
  }
  return [...new Set(scopes)];
}
