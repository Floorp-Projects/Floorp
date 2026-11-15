// SPDX-License-Identifier: MPL-2.0
// EXPERIMENTAL local dev only. Root executes under its D23 intent after review.
import * as oidc from "openid-client";
import { createRemoteJWKSet, customFetch } from "jose";
import {
  SharedAccessError,
  verifySharedAccessToken,
} from "../src/fdh_shared_access.ts";
import { createSharedMcpHandler } from "../src/fdh_shared_mcp.ts";
import type {
  EphemeralKeycloakContext,
  JsonRecord,
  ProtocolCleanupDependencies,
  ProtocolCliDependencies,
  ProtocolContainerOwnership,
  ProtocolSpikeReport,
} from "./fdh_shared_mcp_spike_types.ts";

const ORIGIN = "http://127.0.0.1:18880";
const RESOURCE = "http://127.0.0.1:18881/mcp";
const ISSUER = ORIGIN + "/realms/fdh-spike";
const IMAGE =
  "quay.io/keycloak/keycloak@sha256:4da21f9a53bcbac1026a0d654b740a81e98e5de4c45457076acc74b4d94b1551";
const ADMIN = "fdh-spike-admin";
const LABEL = "fdh.spike=workspace-mcp";
const OWNERSHIP = "_dist/fdh/keycloak-protocol-ownership.json";
const AUDIENCE_MAPPER = {
  name: "resource-audience",
  protocol: "openid-connect",
  protocolMapper: "oidc-audience-mapper",
  config: {
    "included.client.audience": "fdh-mcp-resource",
    "included.custom.audience": RESOURCE,
    "access.token.claim": "true",
    "id.token.claim": "false",
  },
};
const delay = (ms: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, ms));
const fail = () => new Error("Protocol spike stage failed");
function record(value: unknown): JsonRecord {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw fail();
  return value as JsonRecord;
}
function secret(): string {
  return Array.from(
    crypto.getRandomValues(new Uint8Array(32)),
    (v) => v.toString(16).padStart(2, "0"),
  ).join("");
}
// Never forward Docker stdout/stderr or exceptions: bootstrap secrets may appear.
function safeDockerEnvironment(): Record<string, string> {
  const result: Record<string, string> = {};
  // Explicit OS/Docker connectivity allowlist; never inherit Google credentials.
  for (
    const key of [
      "PATH",
      "SystemRoot",
      "WINDIR",
      "USERPROFILE",
      "HOME",
      "DOCKER_CONFIG",
      "DOCKER_HOST",
      "DOCKER_CONTEXT",
      "DOCKER_TLS_VERIFY",
      "DOCKER_CERT_PATH",
      "TEMP",
      "TMP",
    ]
  ) {
    const value = Deno.env.get(key);
    if (value !== undefined) result[key] = value;
  }
  return result;
}
async function docker(
  args: string[],
  env: Record<string, string> = {},
): Promise<string> {
  try {
    const result = await new Deno.Command("docker", {
      args,
      clearEnv: true,
      env: { ...safeDockerEnvironment(), ...env },
      stdin: "null",
      stdout: "piped",
      stderr: "piped",
      signal: AbortSignal.timeout(30_000),
    }).output();
    if (!result.success) throw fail();
    return new TextDecoder().decode(result.stdout).trim();
  } catch {
    throw fail();
  }
}
// All HTTP destinations originate here; no redirects or network retry of mutations.
async function localFetch(
  url: string,
  init: RequestInit = {},
): Promise<Response> {
  const parsed = new URL(url);
  if (
    parsed.origin !== ORIGIN || parsed.username || parsed.password ||
    parsed.hash
  ) throw fail();
  const response = await fetch(url, {
    ...init,
    redirect: "error",
    signal: init.signal
      ? AbortSignal.any([init.signal, AbortSignal.timeout(10_000)])
      : AbortSignal.timeout(10_000),
  });
  // Bound response bytes before JSON/OAuth/JWKS consumers parse them.
  const reader = response.body?.getReader();
  const chunks: Uint8Array[] = [];
  let size = 0;
  try {
    if (reader) {
      while (true) {
        const chunk = await reader.read();
        if (chunk.done) break;
        size += chunk.value.length;
        if (size > 1_048_576) throw fail();
        chunks.push(chunk.value);
      }
    }
  } finally {
    if (reader) await reader.cancel();
  }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    bytes.set(chunk, offset);
    offset += chunk.length;
  }
  return new Response(size ? bytes : null, {
    status: response.status,
    headers: response.headers,
  });
}
async function jsonGet(url: string, signal?: AbortSignal): Promise<unknown> {
  const response = await localFetch(url, { signal });
  if (!response.ok) throw fail();
  return await response.json();
}
async function configuration(
  issuer: string,
  id: string,
  password?: string,
  assertLive: () => void = () => {},
): Promise<oidc.Configuration> {
  assertLive();
  const metadata = record(
    await jsonGet(issuer + "/.well-known/openid-configuration"),
  );
  const token = issuer + "/protocol/openid-connect/token";
  const certs = issuer + "/protocol/openid-connect/certs";
  if (
    metadata.issuer !== issuer || metadata.token_endpoint !== token ||
    metadata.jwks_uri !== certs ||
    metadata.authorization_endpoint !==
      issuer + "/protocol/openid-connect/auth" ||
    metadata.introspection_endpoint !==
      issuer + "/protocol/openid-connect/token/introspect" ||
    metadata.revocation_endpoint !== issuer + "/protocol/openid-connect/revoke"
  ) throw fail();
  const config = new oidc.Configuration(
    metadata as oidc.ServerMetadata,
    id,
    password ? { client_secret: password } : {},
    password ? oidc.ClientSecretPost(password) : oidc.None(),
  );
  oidc.enableNonRepudiationChecks(config);
  oidc.allowInsecureRequests(config); // Fixed literal loopback only.
  config[oidc.customFetch] = (url, init) => {
    assertLive();
    if (
      ![
        token,
        certs,
        issuer + "/protocol/openid-connect/token/introspect",
        issuer + "/protocol/openid-connect/revoke",
      ].includes(String(url))
    ) throw fail();
    const body = init.body instanceof Uint8Array
      ? new Uint8Array(init.body).buffer
      : init.body;
    return localFetch(String(url), { ...init, body });
  };
  return config;
}
async function inspect(name: string): Promise<JsonRecord> {
  const data: unknown = JSON.parse(
    await docker(["inspect", "--type=container", name]),
  );
  if (!Array.isArray(data) || data.length !== 1) throw fail();
  return record(data[0]);
}
async function owned(ownership: ProtocolContainerOwnership): Promise<void> {
  const info = await inspect(ownership.name);
  const config = record(info.Config);
  const labels = record(config.Labels);
  if (
    info.Id !== ownership.containerID || info.Name !== "/" + ownership.name ||
    ownership.image !== IMAGE || config.Image !== IMAGE ||
    labels["fdh.spike"] !== "workspace-mcp" ||
    labels["fdh.nonce"] !== ownership.nonce
  ) throw fail();
}
async function scopeRejected(
  token: string,
  resolveKey: ReturnType<typeof createRemoteJWKSet>,
): Promise<boolean> {
  try {
    await verifySharedAccessToken(token, {
      issuer: ISSUER,
      resourceAudience: RESOURCE,
      requiredScopes: ["fdh:probe"],
      localSpike: true,
    }, { resolveKey });
    return false;
  } catch (error) {
    return error instanceof SharedAccessError &&
      error.code === "insufficient-scope";
  }
}

/** Recovery may return null ONLY after successful absence readback, never on an exception. */
export async function cleanupProtocolContainer(
  ownership: ProtocolContainerOwnership | undefined,
  dependencies: ProtocolCleanupDependencies,
): Promise<void> {
  const candidate = ownership ?? await dependencies.recover();
  if (!candidate) return;
  await dependencies.assertOwned(candidate);
  await dependencies.stop(candidate.containerID);
  if (!await dependencies.confirmAbsent(candidate.containerID)) {
    throw new Error("Protocol container cleanup unconfirmed");
  }
}

export async function withEphemeralKeycloak<T>(
  callback: (context: EphemeralKeycloakContext) => Promise<T>,
  assertLive: () => void = () => {},
): Promise<T> {
  assertLive();
  // Refuse an existing listener, never stop it. The publish bind also guards races.
  const reservation = Deno.listen({
    hostname: "127.0.0.1",
    port: 18880,
    transport: "tcp",
  });
  reservation.close();
  const nonce = crypto.randomUUID();
  const name = "fdh-sso-protocol-" + nonce;
  assertLive();
  const existing = await docker(["ps", "-a", "--format", "{{.Names}}"]);
  if (existing.split(/\r?\n/).includes(name)) throw fail();
  const adminPassword = secret();
  const machineSecret = secret();
  let ownership: ProtocolContainerOwnership | undefined;
  let launchAttempted = false;
  let stage = "container-launch";
  try {
    assertLive();
    launchAttempted = true;
    const id = await docker([
      "run",
      "--pull=never",
      "--detach",
      "--rm",
      "--name",
      name,
      "--label",
      LABEL,
      "--label",
      "fdh.nonce=" + nonce,
      "--publish",
      "127.0.0.1:18880:8080",
      "--env",
      "KC_BOOTSTRAP_ADMIN_USERNAME",
      "--env",
      "KC_BOOTSTRAP_ADMIN_PASSWORD",
      IMAGE,
      "start-dev",
      "--hostname=" + ORIGIN,
      "--features=resource-indicators",
    ], {
      KC_BOOTSTRAP_ADMIN_USERNAME: ADMIN,
      KC_BOOTSTRAP_ADMIN_PASSWORD: adminPassword,
    });
    if (!/^[a-f0-9]{64}$/.test(id)) throw fail();
    ownership = { containerID: id, image: IMAGE, name, nonce };
    assertLive();
    await owned(ownership);
    await Deno.mkdir("_dist/fdh", { recursive: true });
    await Deno.writeTextFile(OWNERSHIP, JSON.stringify(ownership));
    stage = "readiness";
    const deadline = performance.now() + 120_000;
    const readinessSignal = AbortSignal.timeout(120_000);
    let ready = false;
    while (performance.now() < deadline) {
      assertLive();
      try {
        await jsonGet(
          ORIGIN + "/realms/master/.well-known/openid-configuration",
          readinessSignal,
        );
        ready = true;
        break;
      } catch {
        await delay(500);
      }
    }
    if (!ready) throw fail();
    stage = "admin-bootstrap";
    const adminConfig = await configuration(
      ORIGIN + "/realms/master",
      "admin-cli",
      undefined,
      assertLive,
    );
    // Local generated bootstrap ADMIN password only; never a user's Google password.
    const admin = await oidc.genericGrantRequest(adminConfig, "password", {
      username: ADMIN,
      password: adminPassword,
    });
    const adminPath = ORIGIN + "/admin/realms";
    const adminRest = async (
      path: string,
      method: string,
      body?: unknown,
    ): Promise<unknown> => {
      assertLive();
      if (path && path !== "/fdh-spike" && !path.startsWith("/fdh-spike/")) {
        throw fail();
      }
      const response = await localFetch(adminPath + path, {
        method,
        headers: {
          authorization: "Bearer " + admin.access_token,
          "content-type": "application/json",
        },
        body: body === undefined ? undefined : JSON.stringify(body),
      });
      if (!response.ok) throw fail(); // No POST retry on uncertain completion.
      return response.status === 204 || response.status === 201
        ? null
        : await response.json();
    };
    stage = "realm-create";
    await adminRest("", "POST", {
      realm: "fdh-spike",
      enabled: true,
      accessTokenLifespan: 20,
      ssoSessionIdleTimeout: 600,
      ssoSessionMaxLifespan: 600,
      revokeRefreshToken: true,
      refreshTokenMaxReuse: 0,
    });
    stage = "clients-and-scopes";
    const base = "/fdh-spike";
    await adminRest(base + "/clients", "POST", {
      clientId: "fdh-mcp-resource",
      protocol: "openid-connect",
      enabled: true,
      bearerOnly: true,
      attributes: { resource_url: RESOURCE },
    });
    await adminRest(base + "/clients", "POST", {
      clientId: "fdh-machine-fixture",
      protocol: "openid-connect",
      enabled: true,
      publicClient: false,
      secret: machineSecret,
      serviceAccountsEnabled: true,
      standardFlowEnabled: false,
      directAccessGrantsEnabled: false,
      protocolMappers: [AUDIENCE_MAPPER], // Also give missing-scope JWT a valid aud: isolate scope rejection.
    });
    for (const scope of ["fdh:probe", "fdh:other"]) {
      await adminRest(base + "/client-scopes", "POST", {
        name: scope,
        protocol: "openid-connect",
        attributes: { "include.in.token.scope": "true" },
        protocolMappers: [AUDIENCE_MAPPER],
      });
    }
    const clients = await adminRest(base + "/clients", "GET");
    const scopes = await adminRest(base + "/client-scopes", "GET");
    if (!Array.isArray(clients) || !Array.isArray(scopes)) throw fail();
    const machine = clients.map(record).find((item) =>
      item.clientId === "fdh-machine-fixture"
    );
    if (typeof machine?.id !== "string") throw fail();
    for (
      const scope of scopes.map(record).filter((item) =>
        ["fdh:probe", "fdh:other"].includes(String(item.name))
      )
    ) {
      if (typeof scope.id !== "string") throw fail();
      await adminRest(
        base + "/clients/" + encodeURIComponent(machine.id) +
          "/optional-client-scopes/" + encodeURIComponent(scope.id),
        "PUT",
      );
    }
    return await callback(Object.freeze({
      issuer: ISSUER,
      resource: RESOURCE,
      realmPath: base,
      audienceMapper: AUDIENCE_MAPPER,
      machineSecret,
      assertLive,
      adminRest,
      strictLocalFetch: (url: string, init?: RequestInit) => {
        assertLive();
        return localFetch(url, init);
      },
      configuration: (id: string, password?: string) =>
        configuration(ISSUER, id, password, assertLive),
    }));
  } catch (error) {
    const codes = [
      "invalid_client",
      "invalid_scope",
      "invalid_target",
      "unauthorized_client",
      "unsupported_grant_type",
      "invalid_request",
      "server_error",
    ];
    const code =
      error instanceof oidc.ResponseBodyError && codes.includes(error.error)
        ? "-" + error.error.replaceAll("_", "-")
        : "";
    throw new Error("Protocol spike stage failed [" + stage + code + "]");
  } finally {
    if (launchAttempted) {
      await cleanupProtocolContainer(ownership, {
        recover: async () => {
          const id = await docker([
            "ps",
            "-a",
            "--no-trunc",
            "--filter",
            "name=^/" + name + "$",
            "--format",
            "{{.ID}}",
          ]);
          if (!id) return null; // Successful exact-name list proves absence. Docker errors propagate.
          if (!/^[a-f0-9]{64}$/.test(id)) throw fail();
          const candidate = { containerID: id, image: IMAGE, name, nonce };
          await owned(candidate);
          await Deno.mkdir("_dist/fdh", { recursive: true });
          await Deno.writeTextFile(OWNERSHIP, JSON.stringify(candidate));
          return candidate;
        },
        assertOwned: owned,
        stop: async (id) => {
          await docker(["stop", "--time", "10", id]);
        },
        confirmAbsent: async (id) => {
          const until = performance.now() + 5000;
          do {
            const remaining = await docker([
              "ps",
              "-a",
              "--no-trunc",
              "--filter",
              "id=" + id,
              "--format",
              "{{.ID}}",
            ]);
            if (!remaining) return true;
            if (performance.now() >= until) return false;
            await delay(200);
          } while (true);
        },
      });
    }
  }
}

export async function executeProtocolOnly(): Promise<ProtocolSpikeReport> {
  return await withEphemeralKeycloak(async (context) => {
    const config = await context.configuration(
      "fdh-machine-fixture",
      context.machineSecret,
    );
    const grant = (scope: string, resource = RESOURCE) =>
      oidc.clientCredentialsGrant(config, { resource, scope });
    // An explicitly empty scope is invalid OAuth syntax in Keycloak. Omit the
    // parameter to obtain its default scopes and isolate missing FDH permission.
    const missing = await oidc.clientCredentialsGrant(config, {
      resource: RESOURCE,
    });
    const wrong = await grant("fdh:other");
    const positive = await grant("fdh:probe");
    const certs = ISSUER + "/protocol/openid-connect/certs";
    const resolveKey = createRemoteJWKSet(new URL(certs), {
      timeoutDuration: 10_000,
      [customFetch]: (url, init) => {
        if (url !== certs) throw fail();
        return localFetch(url, init);
      },
    });
    const policy = {
      issuer: ISSUER,
      resourceAudience: RESOURCE,
      requiredScopes: ["fdh:probe"],
      localSpike: true,
    };
    const verify = (token: string) =>
      verifySharedAccessToken(token, policy, { resolveKey });
    const admitted = await verify(positive.access_token);
    let invalidTargetRejected = false;
    try {
      await grant("fdh:probe", "http://127.0.0.1:18881/unrelated");
    } catch (error) {
      invalidTargetRejected = error instanceof oidc.ResponseBodyError &&
        error.error === "invalid_target";
    }
    let bindingCalled = false;
    const handler = createSharedMcpHandler(policy, {
      verifyAccessToken: verify,
      resolveOnlineWorkspaceBinding: () => {
        bindingCalled = true;
        return Promise.resolve(null);
      },
    });
    const response = await handler(
      new Request(RESOURCE, {
        method: "POST",
        headers: { authorization: "Bearer " + positive.access_token },
      }),
    );
    const report: ProtocolSpikeReport = {
      scope: "protocol-only",
      identity: "nonGoogle",
      mcpAuthorizationGranted: false,
      ac05Qualified: false,
      experimental: true,
      audienceAndScopeAccepted: true,
      invalidTargetRejected,
      missingScopeRejected: await scopeRejected(
        missing.access_token,
        resolveKey,
      ),
      wrongScopeRejected: await scopeRejected(wrong.access_token, resolveKey),
      expiredTokenRejected: false,
      unboundMachineMcpRejected: bindingCalled && response.status === 403,
    };
    await response.body?.cancel();
    const wait = admitted.expiresAt * 1000 - Date.now() + 1100;
    if (wait < 0 || wait > 22_000) throw fail();
    await delay(wait); // Real AS TTL, no fake clock or altered JWT.
    try {
      await verify(positive.access_token);
    } catch (error) {
      report.expiredTokenRejected = error instanceof SharedAccessError &&
        error.code === "verification-failed";
    }
    return report;
  });
}

export async function runProtocolSpikeCli(
  args: readonly string[],
  dependencies: ProtocolCliDependencies = {
    execute: executeProtocolOnly,
    report: (report) => console.log(JSON.stringify(report)),
    error: () => console.error("Protocol spike stage failed"),
  },
): Promise<number> {
  if (args.length !== 1 || args[0] !== "--execute-protocol-only") {
    console.error(
      "Usage: --execute-protocol-only (experimental local dev only)",
    );
    return 2;
  }
  try {
    const report = await dependencies.execute();
    dependencies.report(report);
    const passed = report.audienceAndScopeAccepted === true &&
      report.invalidTargetRejected === true &&
      report.missingScopeRejected === true &&
      report.wrongScopeRejected === true &&
      report.expiredTokenRejected === true &&
      report.unboundMachineMcpRejected === true;
    return passed ? 0 : 1;
  } catch {
    dependencies.error();
    return 1;
  }
}
if (import.meta.main) Deno.exitCode = await runProtocolSpikeCli(Deno.args);
