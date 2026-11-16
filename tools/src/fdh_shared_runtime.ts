// SPDX-License-Identifier: MPL-2.0
import { types } from "node:util";
import * as oidc from "openid-client";
import { createRemoteJWKSet, customFetch } from "jose";
import { parseSharedServiceConfig } from "./fdh_shared_config.ts";
import {
  SharedAccessError,
  verifySharedAccessToken,
} from "./fdh_shared_access.ts";
import { createSharedGoogleBrokerResolver } from "./fdh_shared_google_broker.ts";
import type {
  SharedRuntimePrivateIO,
  SharedRuntimeReaderGrant,
  SharedRuntimeResult,
} from "./fdh_shared_runtime_types.ts";

const fail = () => new Error("Shared reader runtime rejected");
const unavailable = Object.freeze({
  mode: "unconfigured" as const,
  productionQualified: false as const,
  protectedOperationsAllowed: false as const,
});
/** Preserve only the maintained verifier's safe scope classification. Never read
 * arbitrary error properties, accessors, Proxy traps, causes or provider text. */
function insufficientScope(error: unknown): boolean {
  if (
    typeof error !== "object" || error === null || types.isProxy(error) ||
    !types.isNativeError(error) ||
    Object.getPrototypeOf(error) !== SharedAccessError.prototype
  ) return false;
  const code = Object.getOwnPropertyDescriptor(error, "code");
  return code !== undefined && "value" in code &&
    code.value === "insufficient-scope";
}
const MAX_BYTES = 256 * 1024;
const TIMEOUT_MS = 5000;
function snapshot(value: unknown): SharedRuntimePrivateIO {
  if (!value || typeof value !== "object" || types.isProxy(value)) throw fail();
  const proto = Object.getPrototypeOf(value);
  if (proto !== null && proto !== Object.prototype) throw fail();
  const keys = Reflect.ownKeys(value);
  if (
    keys.length !== 4 ||
    !["backend", "lookupSecret", "fetch", "now"].every((k) => keys.includes(k))
  ) throw fail();
  const d = Object.getOwnPropertyDescriptors(value);
  for (const key of keys) {
    if (typeof key !== "string" || !("value" in d[key])) throw fail();
  }
  if (d.backend.value !== "keycloak-readonly") throw fail();
  for (const key of ["lookupSecret", "fetch", "now"]) {
    if (typeof d[key].value !== "function" || types.isProxy(d[key].value)) {
      throw fail();
    }
  }
  return Object.freeze({
    backend: "keycloak-readonly",
    lookupSecret: d.lookupSecret.value,
    fetch: d.fetch.value,
    now: d.now.value,
  });
}
function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw fail();
  return value as Record<string, unknown>;
}
// Reject routing syntax before encoding: reverse proxies may decode repeatedly.
function subjectPath(subject: string): string {
  if (
    typeof subject !== "string" || !/^[\x21-\x7e]{1,255}$/.test(subject) ||
    subject === "." || subject === ".." || subject === "profile" ||
    ["/", "\\", "%", "?", "#", ";"].some((delimiter) =>
      subject.includes(delimiter)
    )
  ) throw fail();
  return encodeURIComponent(subject).replace(
    /[!'()*]/g,
    (c) => "%" + c.charCodeAt(0).toString(16).toUpperCase(),
  );
}
/** Private server adapter for an explicitly selected, externally provisioned
 * Keycloak reader (view-users/view-identity-providers/view-realm, never realm-admin).
 * No server, gateway session, Google signature, provenance attestation or approval.
 * The existing broker's session-note/historical-hardening TCB limits still apply.
 * Only reader grant is cached; every identity/session/provenance check stays online. */
export async function createSharedRuntime(
  operatorConfig: unknown,
  privateIO?: SharedRuntimePrivateIO,
): Promise<SharedRuntimeResult> {
  const config = parseSharedServiceConfig(operatorConfig);
  if (config.mode !== "configured") return unavailable;
  const { issuer, readerClientId, readerSecretEnv, expectedGoogleClientId } =
    config.server;
  const url = new URL(issuer);
  const match = new RegExp(
    "^(/(?:[A-Za-z0-9_-]+/)*)realms/([A-Za-z0-9_-]{1,80})$",
  ).exec(url.pathname);
  if (!match || match[2] === "." || match[2] === "..") throw fail();
  const admin = url.origin + match[1] + "admin/realms/" + match[2];
  const metadataUrl = issuer + "/.well-known/openid-configuration";
  const tokenUrl = issuer + "/protocol/openid-connect/token";
  const introspectionUrl = tokenUrl + "/introspect";
  const certsUrl = issuer + "/protocol/openid-connect/certs";
  let io: SharedRuntimePrivateIO;
  try {
    io = snapshot(privateIO);
  } catch {
    return unavailable;
  }
  let lastNow = -1;
  const now = () => {
    let n: number;
    try {
      n = io.now();
    } catch {
      throw fail();
    }
    if (
      !Number.isSafeInteger(n) || n < 0 || n < lastNow || n > 8_640_000_000_000
    ) throw fail();
    lastNow = n;
    return n;
  };
  try {
    now();
    const secret = await io.lookupSecret(readerSecretEnv);
    if (typeof secret !== "string" || !/^[\x21-\x7e]{1,4096}$/.test(secret)) {
      return unavailable;
    }
    // Closed method/URL capability table, never an arbitrary AS/admin transport.
    const allowed = (destination: string, method: string) => {
      if ([metadataUrl, certsUrl].includes(destination)) {
        return method === "GET";
      }
      if ([tokenUrl, introspectionUrl].includes(destination)) {
        return method === "POST";
      }
      if (method !== "GET" || !destination.startsWith(admin + "/")) {
        return false;
      }
      const path = destination.slice(admin.length);
      if (
        [
          "/users/profile",
          "/identity-provider/instances/google",
          "/identity-provider/instances/google/mappers",
        ].includes(path)
      ) return true;
      const m = new RegExp("^/users/([^/]+)(?:/federated-identity)?$").exec(
        path,
      );
      if (!m) return false;
      try {
        return subjectPath(decodeURIComponent(m[1])) === m[1];
      } catch {
        return false;
      }
    };
    const strictFetch: typeof fetch = async (input, init = {}) => {
      const destination = input instanceof URL
        ? input.href
        : typeof input === "string"
        ? input
        : input.url;
      const method = init.method ??
        (input instanceof Request ? input.method : "GET");
      if (
        !allowed(destination, method) ||
        new URL(destination).href !== destination
      ) throw fail();
      const controller = new AbortController();
      let timer: ReturnType<typeof setTimeout> | undefined;
      const aborted = () => controller.abort();
      init.signal?.addEventListener("abort", aborted, { once: true });
      if (init.signal?.aborted) controller.abort();
      try {
        return await Promise.race([
          (async () => {
            const response = await io.fetch(destination, {
              ...init,
              method,
              redirect: "error",
              credentials: "omit",
              cache: "no-store",
              signal: controller.signal,
            });
            if (
              response.redirected ||
              (response.url && response.url !== destination) || !response.ok
            ) throw fail();
            const declared = response.headers.get("content-length");
            if (
              declared !== null &&
              (!/^\d+$/.test(declared) || Number(declared) > MAX_BYTES)
            ) throw fail();
            const reader = response.body?.getReader();
            const chunks: Uint8Array[] = [];
            let total = 0;
            if (reader) {
              try {
                while (true) {
                  const { done, value } = await reader.read();
                  if (done) break;
                  total += value.byteLength;
                  if (total > MAX_BYTES) throw fail();
                  chunks.push(value);
                }
              } finally {
                await reader.cancel().catch(() => {});
                reader.releaseLock();
              }
            }
            const bytes = new Uint8Array(total);
            let offset = 0;
            for (const chunk of chunks) {
              bytes.set(chunk, offset);
              offset += chunk.length;
            }
            return new Response(bytes, {
              status: response.status,
              headers: response.headers,
            });
          })(),
          new Promise<never>((_, reject) => {
            timer = setTimeout(() => {
              controller.abort();
              reject(fail());
            }, TIMEOUT_MS);
          }),
        ]);
      } catch {
        throw fail();
      } finally {
        controller.abort();
        if (timer !== undefined) clearTimeout(timer);
        init.signal?.removeEventListener("abort", aborted);
      }
    };
    const metadata = record(await (await strictFetch(metadataUrl)).json());
    for (
      const [key, expected] of Object.entries({
        issuer,
        token_endpoint: tokenUrl,
        jwks_uri: certsUrl,
        introspection_endpoint: introspectionUrl,
        authorization_endpoint: issuer + "/protocol/openid-connect/auth",
        revocation_endpoint: issuer + "/protocol/openid-connect/revoke",
      })
    ) {
      if (metadata[key] !== expected) throw fail();
    }
    // Construct from the validated allowlist, not mutable/unrecognized metadata.
    const service = new oidc.Configuration(
      {
        issuer,
        token_endpoint: tokenUrl,
        jwks_uri: certsUrl,
        introspection_endpoint: introspectionUrl,
      },
      readerClientId,
      {},
      oidc.ClientSecretPost(secret),
    );
    service[oidc.customFetch] = (input, init) =>
      strictFetch(input, {
        ...init,
        body: init.body instanceof Uint8Array
          ? new Uint8Array(init.body).buffer
          : init.body,
      });
    const resolveKey = createRemoteJWKSet(new URL(certsUrl), {
      [customFetch]: strictFetch,
      timeoutDuration: TIMEOUT_MS,
    });
    let cached: SharedRuntimeReaderGrant | undefined;
    let pending: Promise<SharedRuntimeReaderGrant> | undefined;
    const readerToken = async (): Promise<SharedRuntimeReaderGrant> => {
      const start = now();
      if (cached && start < cached.until) return cached;
      if (pending) return await pending;
      pending = (async () => {
        const grant = await oidc.clientCredentialsGrant(service);
        const end = now();
        if (
          typeof grant.access_token !== "string" ||
          !/^[\x21-\x7e]{1,16384}$/.test(grant.access_token) ||
          grant.token_type.toLowerCase() !== "bearer" ||
          !Number.isSafeInteger(grant.expires_in) || !grant.expires_in ||
          grant.expires_in < 1 || grant.expires_in > 86400 ||
          end >= start + grant.expires_in
        ) throw fail();
        cached = {
          token: grant.access_token,
          expiresAt: start + grant.expires_in,
          until: Math.min(
            start + grant.expires_in -
              Math.min(5, Math.floor(grant.expires_in / 10)),
            start + 60,
          ),
        };
        return cached;
      })();
      try {
        return await pending;
      } finally {
        pending = undefined;
      }
    };
    const read = async (path: string): Promise<unknown> => {
      const grant = await readerToken();
      if (now() >= grant.expiresAt) throw fail();
      return await (await strictFetch(admin + path, {
        headers: { authorization: "Bearer " + grant.token },
      })).json();
    };
    const policy = Object.freeze({
      issuer,
      resourceAudience: config.public.mcpResource,
      requiredScopes: Object.freeze(["fdh:probe"]),
      expectedGoogleClientId,
    });
    const online = createSharedGoogleBrokerResolver(policy, {
      introspect: (token: string) => oidc.tokenIntrospection(service, token),
      readUser: (subject: string) => read("/users/" + subjectPath(subject)),
      readFederatedIdentities: (subject: string) =>
        read("/users/" + subjectPath(subject) + "/federated-identity"),
      readGoogleProviderConfig: () =>
        read("/identity-provider/instances/google"),
      readGoogleProviderMappers: () =>
        read("/identity-provider/instances/google/mappers"),
      readUserProfile: () => read("/users/profile"),
      now,
    });
    return Object.freeze({
      mode: "configured",
      productionQualified: false,
      protectedOperationsAllowed: false,
      dependencies: Object.freeze({
        verifyAccessToken: async (token: string) => {
          try {
            const verified = await verifySharedAccessToken(token, policy, {
              resolveKey,
              now: () => new Date(now() * 1000),
            });
            if (verified.expiresAt <= now()) throw fail();
            return verified;
          } catch (error) {
            if (insufficientScope(error)) {
              throw new SharedAccessError("insufficient-scope");
            }
            throw fail();
          }
        },
        resolveOnlineWorkspaceBinding: online,
      }),
    });
  } catch {
    return unavailable;
  }
}
