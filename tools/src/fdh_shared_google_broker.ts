// SPDX-License-Identifier: MPL-2.0
import { types } from "node:util";
import type {
  SharedGoogleBrokerResolver,
  SharedGoogleBrokerTrustedIO,
} from "./fdh_shared_google_broker_types.ts";

const TEXT = /^[\x21-\x7e]{1,2048}$/;
const SUBJECT = /^[\x21-\x7e]{1,255}$/;
const SCOPE = /^[\x21\x23-\x5b\x5d-\x7e]{1,256}$/;
const EMAIL =
  /^[A-Za-z0-9!#$%&'*+\/=?^_`{|}~-]+(?:\.[A-Za-z0-9!#$%&'*+\/=?^_`{|}~-]+)*@floorp\.app$/;

class BrokerIOError extends Error {
  constructor() {
    super("Google broker unavailable");
  }
}

async function brokerSnapshot(read: () => Promise<unknown>): Promise<unknown> {
  let response;
  try {
    response = await read();
  } catch {
    throw new BrokerIOError();
  }
  return ownDataSnapshot(response);
}

/** Pure RS adapter, NOT Google signature verification, an AS, MCP grant, or
 * human approval. The root IO pins hosts/realm/credentials and authenticates the
 * complete responses plus identity_provider's USER SESSION NOTE provenance.
 * Snapshots prove consistency at observation time, not atomic broker state.
 * Google provenance additionally depends on the pinned Keycloak 26.8.2 provider
 * implementation and no privileged out-of-band profile/federation tampering.
 * Fail closed with constant diagnostics; never echo tokens, claims or IO errors. */
export function createSharedGoogleBrokerResolver(
  policy: unknown,
  trustedIO: unknown,
  { propagateIOErrors = false } = {},
): SharedGoogleBrokerResolver {
  let issuer: string;
  let audience: string;
  let clientId: string;
  let requiredScopes: string[];
  let io: SharedGoogleBrokerTrustedIO;
  try {
    const p = record(ownDataSnapshot(policy));
    issuer = text(p.issuer);
    audience = text(p.resourceAudience);
    clientId = text(p.expectedGoogleClientId);
    requiredScopes = scopes(p.requiredScopes);
    const d = record(trustedIO);
    io = {
      introspect: callback(own(d, "introspect")),
      readUser: callback(own(d, "readUser")),
      readFederatedIdentities: callback(own(d, "readFederatedIdentities")),
      readGoogleProviderConfig: callback(own(d, "readGoogleProviderConfig")),
      readGoogleProviderMappers: callback(own(d, "readGoogleProviderMappers")),
      readUserProfile: callback(own(d, "readUserProfile")),
      now: callback(own(d, "now")),
    };
  } catch {
    throw new Error("Invalid Google broker configuration");
  }
  return async (token, access) => {
    try {
      if (
        typeof token !== "string" || !/^[\x21-\x7e]{1,16384}$/.test(token)
      ) return null;
      const a = record(ownDataSnapshot(access));
      const grantedScopes = scopes(a.scopes);
      const subject = text(a.subject);
      const expires = epoch(a.expiresAt);
      const start = epoch(io.now());
      if (
        a.kind !== "verified-shared-access-token" || a.issuer !== issuer ||
        a.resourceAudience !== audience || expires <= start ||
        subject.length > 255 ||
        !requiredScopes.every((s) => grantedScopes.includes(s))
      ) return null;

      // Snapshot each completed response immediately, before starting another IO.
      const i = record(await brokerSnapshot(() => io.introspect(token)));
      if (
        i.active !== true || i.iss !== issuer || i.sub !== subject ||
        i.identity_provider !== "google" || i.exp !== expires ||
        i.grant_type === "client_credentials" ||
        !audiences(i.aud).includes(audience) ||
        typeof i.scope !== "string" || i.scope.length > 32768 ||
        !grantedScopes.every((s) => scopeString(i.scope).includes(s))
      ) return null;
      const provider = record(
        await brokerSnapshot(() => io.readGoogleProviderConfig()),
      );
      if (!validProvider(provider, clientId)) return null;
      if (
        array(await brokerSnapshot(() => io.readGoogleProviderMappers()))
          .length !==
          0
      ) return null;
      if (
        !validProfile(record(await brokerSnapshot(() => io.readUserProfile())))
      ) {
        return null;
      }
      const user = record(await brokerSnapshot(() => io.readUser(subject)));
      if (
        user.id !== subject || user.emailVerified !== true ||
        typeof user.email !== "string" || user.email.length > 254 ||
        !EMAIL.test(user.email) ||
        user.serviceAccountClientId !== undefined || user.enabled === false
      ) return null;
      const identities = array(
        await brokerSnapshot(() => io.readFederatedIdentities(subject)),
      );
      const google = identities.map(record).filter((f) =>
        f.identityProvider === "google"
      );
      if (
        google.length !== 1 || typeof google[0].userId !== "string" ||
        !SUBJECT.test(google[0].userId)
      ) return null;
      const end = epoch(io.now());
      if (end < start || expires <= end) return null;
      return Object.freeze({
        kind: "verified-online-google-binding",
        active: true,
        sessionProvider: "google",
        authorizationSubject: subject,
        identityKey: google[0].userId,
        identityIssuer: "https://accounts.google.com",
        email: user.email,
        emailVerified: true,
        hostedDomain: "floorp.app",
      });
    } catch (error) {
      if (propagateIOErrors && error instanceof BrokerIOError) throw error;
      return null;
    }
  };
}

function validProvider(p: Record<string, unknown>, clientId: string): boolean {
  const c = record(p.config);
  // Official 26.8.2 OIDCIdentityProviderConfig uses disableUserInfo, NOT
  // disableUserInfoService. IdentityProviderModel defines the three filter keys.
  // setDisableNonce(false) removes the key; absence is the official false default.
  return p.alias === "google" && p.providerId === "google" &&
    p.enabled === true &&
    p.trustEmail === true && p.storeToken === false && p.linkOnly !== true &&
    c.clientId === clientId && c.hostedDomain === "floorp.app" &&
    c.syncMode === "FORCE" && c.validateSignature === "true" &&
    c.disableUserInfo === "true" && c.filteredByClaim === "true" &&
    c.claimFilterName === "email_verified" && c.claimFilterValue === "true" &&
    (c.disableNonce === undefined || c.disableNonce === "false") &&
    (c.userIDClaim === undefined || c.userIDClaim === "sub") &&
    (c.emailClaim === undefined || c.emailClaim === "email");
}

function validProfile(p: Record<string, unknown>): boolean {
  // Actual Keycloak UserProfileConfig: attributes[].permissions.edit, not an
  // invented immutable flag. Missing unmanaged policy is not assumed safe.
  if (
    !["DISABLED", "ADMIN_VIEW", "ADMIN_EDIT"].includes(
      text(p.unmanagedAttributePolicy),
    )
  ) return false;
  const attributes = array(p.attributes).map(record);
  return ["email", "username"].every((name) => {
    const matches = attributes.filter((a) => a.name === name);
    if (matches.length !== 1) return false;
    const edits = array(record(matches[0].permissions).edit);
    return edits.length === 1 && edits[0] === "admin";
  });
}

function record(value: unknown): Record<string, unknown> {
  if (
    typeof value !== "object" || value === null || types.isProxy(value) ||
    Array.isArray(value) ||
    (Object.getPrototypeOf(value) !== Object.prototype &&
      Object.getPrototypeOf(value) !== null)
  ) throw Error();
  return value as Record<string, unknown>;
}
function array(value: unknown): unknown[] {
  if (!Array.isArray(value) || types.isProxy(value) || value.length > 256) {
    throw Error();
  }
  return value;
}
function own(value: Record<string, unknown>, key: string): unknown {
  const descriptor = Object.getOwnPropertyDescriptor(value, key);
  if (!descriptor || !("value" in descriptor)) throw Error();
  return descriptor.value;
}
function text(value: unknown): string {
  if (typeof value !== "string" || !TEXT.test(value)) throw Error();
  return value;
}
function epoch(value: unknown): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value) || value < 0) {
    throw Error();
  }
  return value;
}
function scopes(value: unknown): string[] {
  const result = array(value).map((s) => {
    if (typeof s !== "string" || !SCOPE.test(s)) throw Error();
    return s;
  });
  if (
    !result.length || result.length > 128 ||
    new Set(result).size !== result.length
  ) throw Error();
  return result;
}
function scopeString(value: unknown): string[] {
  if (typeof value !== "string" || value.length > 32768) throw Error();
  return scopes(value.split(" "));
}
function audiences(value: unknown): string[] {
  return typeof value === "string" ? [text(value)] : array(value).map(text);
}
function callback<T extends (...args: never[]) => unknown>(value: unknown): T {
  if (typeof value !== "function" || types.isProxy(value)) throw Error();
  return value as T;
}

/** Bounded own-data JSON copy. Never evaluate accessors or trust inherited
 * properties, array iterators, toJSON, or Proxy traps. Each IO result is detached
 * before subsequent awaits; native Proxy detection is essential here. */
function ownDataSnapshot(input: unknown): unknown {
  let budget = 8192;
  function copy(value: unknown, depth: number): unknown {
    if (--budget < 0 || depth > 12) throw Error();
    if (value === null || typeof value === "boolean") return value;
    if (typeof value === "number" && Number.isFinite(value)) return value;
    if (typeof value === "string" && value.length <= 32768) return value;
    if (typeof value !== "object" || value === null || types.isProxy(value)) {
      throw Error();
    }
    if (Array.isArray(value)) {
      if (
        Object.getPrototypeOf(value) !== Array.prototype ||
        value.length > 256 ||
        Reflect.ownKeys(value).length !== value.length + 1
      ) throw Error();
      const result: unknown[] = [];
      for (let n = 0; n < value.length; n++) {
        const d = Object.getOwnPropertyDescriptor(value, String(n));
        if (!d || !("value" in d)) throw Error();
        result.push(copy(d.value, depth + 1));
      }
      return result;
    }
    const object = record(value);
    const keys = Reflect.ownKeys(object);
    if (keys.length > 256) throw Error();
    const result: Record<string, unknown> = Object.create(null);
    for (const key of keys) {
      if (typeof key !== "string") throw Error();
      result[key] = copy(own(object, key), depth + 1);
    }
    return result;
  }
  return copy(input, 0);
}
