// SPDX-License-Identifier: MPL-2.0
import { createRemoteJWKSet, customFetch } from "jose";
import * as oidc from "openid-client";
import {
  AUDIT_SCOPE,
  AuditAdmissionError,
  auditResource,
} from "./fdh_audit_http.ts";
import {
  SharedAccessError,
  verifySharedAccessToken,
} from "./fdh_shared_access.ts";
import { createSharedGoogleBrokerResolver } from "./fdh_shared_google_broker.ts";
import type { SharedGoogleBrokerTrustedIO } from "./fdh_shared_google_broker_types.ts";

interface AuditBrokerConfig {
  readonly issuer: string;
  readonly resource: string;
  readonly googleClientId: string;
  readonly readerId: string;
  readonly readerSecret: string;
}

/** Uses a configured Keycloak realm. Its broker, not this service, issues tokens. */
export function createAuditAuthenticator(
  config: AuditBrokerConfig,
  upstreamFetch: typeof fetch = fetch,
): (token: string) => Promise<string> {
  const { issuer, resource, googleClientId, readerId, readerSecret } = config;
  const url = new URL(issuer);
  const realm = /^(\/(?:[A-Za-z0-9_-]+\/)*)realms\/([A-Za-z0-9_-]{1,80})$/.exec(
    url.pathname,
  );
  if (
    url.protocol !== "https:" || issuer !== url.origin + url.pathname ||
    !realm ||
    !/^[A-Za-z0-9_-]{1,200}\.apps\.googleusercontent\.com$/.test(
      googleClientId,
    ) ||
    !/^[A-Za-z0-9_-]{1,80}$/.test(readerId) || !readerSecret
  ) throw new Error("Invalid audit broker configuration");
  const policy = {
    issuer,
    resourceAudience: auditResource(resource).href,
    requiredScopes: [AUDIT_SCOPE],
  };
  const protocol = issuer + "/protocol/openid-connect";
  const admin = url.origin + realm[1] + "admin/realms/" + realm[2];
  const fetchPinned = (
    input: URL | string,
    init?: RequestInit,
  ): Promise<Response> => {
    const requestUrl = new URL(input);
    if (requestUrl.origin !== url.origin) {
      throw new Error("Invalid audit broker endpoint");
    }
    const timeout = AbortSignal.timeout(5000);
    return upstreamFetch(requestUrl, {
      ...init,
      signal: init?.signal ? AbortSignal.any([init.signal, timeout]) : timeout,
      redirect: "error",
    });
  };
  const resolveKey = createRemoteJWKSet(new URL(protocol + "/certs"), {
    [customFetch]: fetchPinned,
  });
  const service = new oidc.Configuration(
    {
      issuer,
      token_endpoint: protocol + "/token",
      introspection_endpoint: protocol + "/token/introspect",
    },
    readerId,
    readerSecret,
    oidc.ClientSecretBasic(),
  );
  service[oidc.customFetch] = (input, { body, ...init }) =>
    fetchPinned(input, {
      ...init,
      body: body instanceof Uint8Array ? new Uint8Array(body) : body,
    });
  service.timeout = 5;
  let serviceToken = "";
  let serviceExpiry = 0;
  let pending: Promise<string> | undefined;

  async function refreshServiceToken(): Promise<string> {
    const response = await oidc.clientCredentialsGrant(service);
    if (!response.expires_in || response.expires_in > 300) {
      throw new Error("Invalid broker reader token");
    }
    serviceToken = response.access_token;
    serviceExpiry = Date.now() + Math.max(0, response.expires_in - 5) * 1000;
    return serviceToken;
  }

  async function read(path: string): Promise<unknown> {
    if (Date.now() >= serviceExpiry) {
      pending ??= refreshServiceToken().finally(() => pending = undefined);
      await pending;
    }
    const response = await fetchPinned(admin + path, {
      headers: { authorization: "Bearer " + serviceToken },
    });
    if (!response.ok) throw new Error("Audit broker read failed");
    return await response.json();
  }

  const io: SharedGoogleBrokerTrustedIO = {
    introspect: (token) => oidc.tokenIntrospection(service, token),
    readUser: (sub) => read("/users/" + encodeURIComponent(sub)),
    readFederatedIdentities: (sub) =>
      read("/users/" + encodeURIComponent(sub) + "/federated-identity"),
    readGoogleProviderConfig: () => read("/identity-provider/instances/google"),
    readGoogleProviderMappers: () =>
      read("/identity-provider/instances/google/mappers"),
    readUserProfile: () => read("/users/profile"),
    now: () => Math.floor(Date.now() / 1000),
  };
  const resolveBinding = createSharedGoogleBrokerResolver({
    ...policy,
    expectedGoogleClientId: googleClientId,
  }, io);
  return async (token) => {
    let access;
    try {
      access = await verifySharedAccessToken(token, policy, { resolveKey });
    } catch (error) {
      if (error instanceof SharedAccessError) {
        throw new AuditAdmissionError(
          error.code === "insufficient-scope"
            ? "unauthorized"
            : "unauthenticated",
        );
      }
      throw error;
    }
    // Keycloak user IDs are path segments in the admin API below.
    if (!/^[A-Za-z0-9_-]{1,255}$/.test(access.subject)) {
      throw new AuditAdmissionError("unauthorized");
    }
    const binding = await resolveBinding(token, access);
    if (!binding || !/^[A-Za-z0-9_-]{1,255}$/.test(binding.identityKey)) {
      throw new AuditAdmissionError("unauthorized");
    }
    return binding.identityKey;
  };
}
