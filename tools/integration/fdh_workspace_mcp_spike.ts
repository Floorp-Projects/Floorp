// SPDX-License-Identifier: MPL-2.0
// Opt-in local experiment. Never launch from imports or fixture tests.
import * as oidc from "openid-client";
import { createRemoteJWKSet, customFetch } from "jose";
// @deno-types="npm:@modelcontextprotocol/sdk@1.32.2/client/index"
import { Client } from "@modelcontextprotocol/sdk/client";
// @deno-types="npm:@modelcontextprotocol/sdk@1.32.2/client/streamableHttp"
import { StreamableHTTPClientTransport } from "@modelcontextprotocol/sdk/client/streamableHttp.js";
import { withEphemeralKeycloak } from "./fdh_shared_mcp_spike.ts";
import type {
  EphemeralKeycloakContext,
  JsonRecord,
} from "./fdh_shared_mcp_spike_types.ts";
import { verifySharedAccessToken } from "../src/fdh_shared_access.ts";
import { createSharedMcpHandler } from "../src/fdh_shared_mcp.ts";
import { createSharedGoogleBrokerResolver } from "../src/fdh_shared_google_broker.ts";
import type { SharedGoogleBrokerTrustedIO } from "../src/fdh_shared_google_broker_types.ts";
import type {
  WorkspaceCliDependencies,
  WorkspaceSpikeOptions,
  WorkspaceSpikeReport,
} from "./fdh_workspace_mcp_spike_types.ts";

const CALLBACK = "http://127.0.0.1:18882/oauth/callback";
const PUBLIC_CLIENT = "fdh-workspace-probe";
const READER_CLIENT = "fdh-broker-reader";
const fail = () => new Error("Workspace MCP experiment failed");
const delay = (ms: number) =>
  new Promise<void>((resolve) => setTimeout(resolve, ms));
function record(value: unknown): JsonRecord {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw fail();
  return value as JsonRecord;
}
function rows(value: unknown): JsonRecord[] {
  if (!Array.isArray(value) || value.length > 256) throw fail();
  return value.map(record);
}
function id(value: unknown): string {
  if (typeof value !== "string" || !/^[A-Za-z0-9_-]{1,255}$/.test(value)) {
    throw fail();
  }
  return encodeURIComponent(value);
}
function one(values: JsonRecord[], field: string, value: string): JsonRecord {
  const matches = values.filter((item) => item[field] === value);
  if (matches.length !== 1) throw fail();
  return matches[0];
}

/** Raw strict parser: invalid/help paths cannot read an environment or start IO. */
export function parseWorkspaceSpikeArgs(
  args: readonly string[],
): WorkspaceSpikeOptions | null {
  if (
    args[0] !== "--execute-google-workspace" ||
    (args.length !== 7 && args.length !== 9)
  ) return null;
  const values = new Map<string, string>();
  const allowed = [
    "--task-id",
    "--client-id",
    "--client-secret-env",
    "--timeout-ms",
  ];
  for (let i = 1; i < args.length; i += 2) {
    const key = args[i], value = args[i + 1];
    if (
      !allowed.includes(key) || values.has(key) || !value ||
      value.startsWith("--")
    ) return null;
    values.set(key, value);
  }
  const taskId = values.get("--task-id") ?? "";
  const clientId = values.get("--client-id") ?? "";
  const timeout = values.get("--timeout-ms") ?? "900000";
  if (
    !/^[A-Za-z0-9][A-Za-z0-9_-]{0,99}$/.test(taskId) ||
    !/^[A-Za-z0-9._-]{1,240}\.apps\.googleusercontent\.com$/.test(clientId) ||
    values.get("--client-secret-env") !== "FDH_GOOGLE_CLIENT_SECRET" ||
    !/^[1-9][0-9]{0,5}$/.test(timeout) || Number(timeout) > 900000 ||
    Number(timeout) < 1000
  ) return null;
  return Object.freeze({
    taskId,
    clientId,
    clientSecretEnv: "FDH_GOOGLE_CLIENT_SECRET",
    timeoutMs: Number(timeout),
  });
}

/** Setup is completed in a NEW realm before the first authorization URL exists. */
async function configureGoogle(
  context: EphemeralKeycloakContext,
  googleId: string,
  googleSecret: string,
): Promise<oidc.Configuration> {
  const { adminRest: admin, realmPath: base } = context;
  await admin(base + "/identity-provider/instances", "POST", {
    alias: "google",
    providerId: "google",
    enabled: true,
    trustEmail: true,
    storeToken: false,
    linkOnly: false,
    config: {
      clientId: googleId,
      clientSecret: googleSecret,
      hostedDomain: "floorp.app",
      validateSignature: "true",
      useJwksUrl: "true",
      issuer: "https://accounts.google.com",
      jwksUrl: "https://www.googleapis.com/oauth2/v3/certs",
      disableUserInfo: "true",
      filteredByClaim: "true",
      claimFilterName: "email_verified",
      claimFilterValue: "true",
      syncMode: "FORCE",
      disableNonce: "false",
      offlineAccess: "false",
    },
  });
  if (
    rows(
      await admin(base + "/identity-provider/instances/google/mappers", "GET"),
    ).length
  ) throw fail();
  const profile = record(await admin(base + "/users/profile", "GET"));
  const attributes = rows(profile.attributes);
  for (const name of ["email", "username"]) {
    const attribute = one(attributes, "name", name);
    attribute.permissions = {
      ...record(attribute.permissions),
      edit: ["admin"],
    };
  }
  await admin(base + "/users/profile", "PUT", {
    ...profile,
    attributes,
    unmanagedAttributePolicy: "ADMIN_VIEW",
  });
  await admin(base + "/clients", "POST", {
    clientId: PUBLIC_CLIENT,
    protocol: "openid-connect",
    enabled: true,
    publicClient: true,
    standardFlowEnabled: true,
    directAccessGrantsEnabled: false,
    serviceAccountsEnabled: false,
    redirectUris: [CALLBACK],
    attributes: { "pkce.code.challenge.method": "S256" },
    protocolMappers: [context.audienceMapper, {
      name: "google-session-provider",
      protocol: "openid-connect",
      protocolMapper: "oidc-usersessionmodel-note-mapper",
      config: {
        "user.session.note": "identity_provider",
        "claim.name": "identity_provider",
        "jsonType.label": "String",
        "access.token.claim": "true",
        "introspection.token.claim": "true",
        "id.token.claim": "false",
      },
    }],
  });
  // Generated service credential is separate from both bootstrap admin and Google.
  const readerSecret = oidc.randomPKCECodeVerifier();
  await admin(base + "/clients", "POST", {
    clientId: READER_CLIENT,
    protocol: "openid-connect",
    enabled: true,
    publicClient: false,
    secret: readerSecret,
    serviceAccountsEnabled: true,
    standardFlowEnabled: false,
    directAccessGrantsEnabled: false,
    fullScopeAllowed: false,
    // Keycloak >= 26.6.2 denies introspection when the caller is not in the
    // token's audience; the RFC 8707 resource aud never contains client ids.
    attributes: { "allow.token.introspection.without.audience.check": "true" },
  });
  const clients = rows(await admin(base + "/clients", "GET"));
  const client = one(clients, "clientId", PUBLIC_CLIENT);
  const reader = one(clients, "clientId", READER_CLIENT);
  const management = one(clients, "clientId", "realm-management");
  const scopes = rows(await admin(base + "/client-scopes", "GET"));
  const probe = one(scopes, "name", "fdh:probe");
  // Keycloak >= 26.8.2: the Admin REST API rejects tokens without the
  // built-in kc-admin-api scope/audience (RFC 9700 audience check).
  const kcAdminApi = one(scopes, "name", "kc-admin-api");
  await admin(
    base + "/clients/" + id(reader.id) + "/default-client-scopes/" +
      id(kcAdminApi.id),
    "PUT",
  );
  await admin(
    base + "/clients/" + id(client.id) + "/optional-client-scopes/" +
      id(probe.id),
    "PUT",
  );
  const account = record(
    await admin(
      base + "/clients/" + id(reader.id) + "/service-account-user",
      "GET",
    ),
  );
  const available = rows(
    await admin(base + "/clients/" + id(management.id) + "/roles", "GET"),
  );
  // No manage/admin fallback: missing read roles stop setup before any login.
  const roles = ["view-users", "view-identity-providers", "view-realm"].map(
    (name) => one(available, "name", name),
  );
  await admin(
    base + "/users/" + id(account.id) + "/role-mappings/clients/" +
      id(management.id),
    "POST",
    roles,
  );
  await admin(
    base + "/clients/" + id(reader.id) + "/scope-mappings/clients/" +
      id(management.id),
    "POST",
    roles,
  );
  return await context.configuration(READER_CLIENT, readerSecret);
}

function bindingIO(
  context: EphemeralKeycloakContext,
  service: oidc.Configuration,
): SharedGoogleBrokerTrustedIO {
  let serviceToken = "";
  let serviceExpiry = 0;
  const read = async (path: string): Promise<unknown> => {
    context.assertLive();
    if (Date.now() >= serviceExpiry) {
      const response = await oidc.clientCredentialsGrant(service);
      if (!response.expires_in || response.expires_in > 120) throw fail();
      serviceToken = response.access_token;
      serviceExpiry = Date.now() + Math.max(0, response.expires_in - 5) * 1000;
    }
    context.assertLive();
    const response = await context.strictLocalFetch(
      "http://127.0.0.1:18880/admin/realms/fdh-spike" + path,
      {
        headers: { authorization: "Bearer " + serviceToken },
      },
    );
    if (!response.ok) throw fail();
    return await response.json();
  };
  return {
    introspect: (token) => {
      context.assertLive();
      return oidc.tokenIntrospection(service, token);
    },
    readUser: (subject) => read("/users/" + id(subject)),
    readFederatedIdentities: (subject) =>
      read("/users/" + id(subject) + "/federated-identity"),
    readGoogleProviderConfig: () => read("/identity-provider/instances/google"),
    readGoogleProviderMappers: () =>
      read("/identity-provider/instances/google/mappers"),
    readUserProfile: () => read("/users/profile"),
    now: () => Math.floor(Date.now() / 1000),
  };
}

async function sdkProbe(resource: string, token: string): Promise<boolean> {
  const client = new Client({
    name: "fdh-local-workspace-probe",
    version: "0.1.0",
  });
  const transport = new StreamableHTTPClientTransport(new URL(resource), {
    requestInit: {
      headers: { authorization: "Bearer " + token },
      redirect: "error",
      signal: AbortSignal.timeout(10000),
    },
  });
  try {
    await client.connect(transport);
    const tools = await client.listTools();
    if (
      tools.tools.length !== 1 || tools.tools[0].name !== "fdh.authz.probe" ||
      tools.tools[0].annotations?.readOnlyHint !== true
    ) throw fail();
    const response = await client.callTool({
      name: "fdh.authz.probe",
      arguments: {},
    });
    if (response.isError === true) throw fail();
    const content = response.content;
    if (!Array.isArray(content) || content.length !== 1) throw fail();
    const item = record(content[0]);
    if (item.type !== "text" || typeof item.text !== "string") throw fail();
    const result = record(JSON.parse(item.text));
    return result.scope === "local-shared-mcp-spike-only" &&
      result.protectedOperationsAllowed === false &&
      result.productionQualified === false;
  } finally {
    await client.close();
    await transport.close();
  }
}

async function googleFlow(
  context: EphemeralKeycloakContext,
  options: WorkspaceSpikeOptions,
  googleSecret: string,
): Promise<WorkspaceSpikeReport> {
  const service = await configureGoogle(
    context,
    options.clientId,
    googleSecret,
  );
  const config = await context.configuration(PUBLIC_CLIENT);
  const policy = {
    issuer: context.issuer,
    resourceAudience: context.resource,
    requiredScopes: ["fdh:probe"],
    localSpike: true,
  };
  const certs = context.issuer + "/protocol/openid-connect/certs";
  const resolveKey = createRemoteJWKSet(new URL(certs), {
    [customFetch]: (url, init) => {
      if (url !== certs) throw fail();
      return context.strictLocalFetch(url, init);
    },
  });
  const io = bindingIO(context, service);
  // Exercise read-only permissions and effective hardening before asking the
  // operator to log in. Never compensate a 403 by granting manage/admin roles.
  const provider = record(await io.readGoogleProviderConfig());
  const providerConfig = record(provider.config);
  if (
    provider.alias !== "google" || provider.providerId !== "google" ||
    provider.enabled !== true || provider.trustEmail !== true ||
    provider.storeToken !== false ||
    providerConfig.clientId !== options.clientId ||
    providerConfig.hostedDomain !== "floorp.app" ||
    providerConfig.validateSignature !== "true" ||
    providerConfig.useJwksUrl !== "true" ||
    providerConfig.issuer !== "https://accounts.google.com" ||
    providerConfig.jwksUrl !== "https://www.googleapis.com/oauth2/v3/certs" ||
    providerConfig.disableUserInfo !== "true" ||
    providerConfig.filteredByClaim !== "true" ||
    providerConfig.claimFilterName !== "email_verified" ||
    providerConfig.claimFilterValue !== "true" ||
    providerConfig.syncMode !== "FORCE" ||
    (providerConfig.disableNonce !== undefined &&
      providerConfig.disableNonce !== "false") ||
    providerConfig.offlineAccess !== "false" ||
    rows(await io.readGoogleProviderMappers()).length !== 0
  ) throw fail();
  const hardenedProfile = record(await io.readUserProfile());
  if (hardenedProfile.unmanagedAttributePolicy !== "ADMIN_VIEW") throw fail();
  for (const name of ["email", "username"]) {
    const edits =
      record(one(rows(hardenedProfile.attributes), "name", name).permissions)
        .edit;
    if (!Array.isArray(edits) || edits.length !== 1 || edits[0] !== "admin") {
      throw fail();
    }
  }
  const handler = createSharedMcpHandler(policy, {
    verifyAccessToken: (token) =>
      verifySharedAccessToken(token, policy, { resolveKey }),
    resolveOnlineWorkspaceBinding: createSharedGoogleBrokerResolver({
      ...policy,
      expectedGoogleClientId: options.clientId,
    }, io),
  });
  const state = oidc.randomState(),
    nonce = oidc.randomNonce(),
    verifier = oidc.randomPKCECodeVerifier();
  const challenge = await oidc.calculatePKCECodeChallenge(verifier);
  const auth = oidc.buildAuthorizationUrl(config, {
    redirect_uri: CALLBACK,
    scope: "openid email profile fdh:probe",
    resource: context.resource,
    state,
    nonce,
    code_challenge: challenge,
    code_challenge_method: "S256",
    kc_idp_hint: "google",
    prompt: "login",
  });
  const controller = new AbortController();
  let callbackServer: Deno.HttpServer | undefined;
  let mcpServer: Deno.HttpServer | undefined;
  let timer: ReturnType<typeof setTimeout> | undefined;
  try {
    let consumed = false;
    const callback = new Promise<URL>((resolve, reject) => {
      timer = setTimeout(() => reject(fail()), options.timeoutMs);
      callbackServer = Deno.serve({
        hostname: "127.0.0.1",
        port: 18882,
        signal: controller.signal,
        onListen: () => {},
        onError: () => new Response("Rejected", { status: 400 }),
      }, (request) => {
        const url = new URL(request.url);
        const headers = {
          "cache-control": "no-store",
          "referrer-policy": "no-referrer",
          "content-security-policy": "default-src 'none'",
        };
        if (
          request.method !== "GET" || url.origin !== "http://127.0.0.1:18882"
        ) return new Response("Not found", { status: 404, headers });
        if (url.pathname === "/complete" && !url.search) {
          return new Response("Return to your terminal.", { headers });
        }
        if (
          url.pathname !== "/oauth/callback" || consumed ||
          url.href.length > 8192 ||
          url.searchParams.getAll("state").length !== 1 ||
          url.searchParams.get("state") !== state ||
          url.searchParams.getAll("code").length !== 1 ||
          !url.searchParams.get("code") || url.searchParams.has("error")
        ) return new Response("Rejected", { status: 400, headers });
        try {
          context.assertLive();
        } catch {
          reject(fail());
          return new Response("Rejected", { status: 400, headers });
        }
        consumed = true;
        resolve(url); // callback code only exists in process memory; no URL logging.
        return new Response(null, {
          status: 303,
          headers: { ...headers, location: "http://127.0.0.1:18882/complete" },
        });
      });
    });
    mcpServer = Deno.serve({
      hostname: "127.0.0.1",
      port: 18881,
      signal: controller.signal,
      onListen: () => {},
      onError: () => new Response("Unavailable", { status: 503 }),
    }, handler);
    context.assertLive();
    if (!Deno.stdout.isTerminal()) throw fail();
    // Only the operator's interactive terminal, once. Never report/artifact/ledger.
    console.log(auth.href);
    const callbackUrl = await callback;
    if (timer !== undefined) {
      clearTimeout(timer);
      timer = undefined;
    }
    context.assertLive();
    const tokens = await oidc.authorizationCodeGrant(config, callbackUrl, {
      expectedState: state,
      expectedNonce: nonce,
      pkceCodeVerifier: verifier,
    }, { resource: context.resource });
    if (!await sdkProbe(context.resource, tokens.access_token)) throw fail();
    const admission = await verifySharedAccessToken(
      tokens.access_token,
      policy,
      { resolveKey },
    );
    const wait = admission.expiresAt * 1000 - Date.now() + 1100;
    if (wait < 0 || wait > 22000) throw fail();
    await delay(wait);
    context.assertLive();
    const expired = await fetch(context.resource, {
      method: "POST",
      redirect: "error",
      signal: AbortSignal.timeout(10000),
      headers: { authorization: "Bearer " + tokens.access_token },
    });
    const expiredTokenRejected = expired.status === 401;
    await expired.body?.cancel();
    if (!expiredTokenRejected || !tokens.refresh_token) throw fail();
    const refreshed = await oidc.refreshTokenGrant(
      config,
      tokens.refresh_token,
      { resource: context.resource },
    );
    if (
      !refreshed.refresh_token ||
      !await sdkProbe(context.resource, refreshed.access_token)
    ) throw fail();
    const refreshedAdmission = await verifySharedAccessToken(
      refreshed.access_token,
      policy,
      { resolveKey },
    );
    await oidc.tokenRevocation(config, refreshed.refresh_token, {
      token_type_hint: "refresh_token",
    });
    // Keycloak-local revocation only. Some AS versions retain the session until
    // local logout; never call Google's project/account revocation endpoints.
    if (record(await io.introspect(refreshed.access_token)).active !== false) {
      const logout = await context.strictLocalFetch(
        context.issuer + "/protocol/openid-connect/logout",
        {
          method: "POST",
          headers: { "content-type": "application/x-www-form-urlencoded" },
          body: new URLSearchParams({
            client_id: PUBLIC_CLIENT,
            refresh_token: refreshed.refresh_token,
          }),
        },
      );
      await logout.body?.cancel();
      if (!logout.ok) throw fail();
    }
    if (record(await io.introspect(refreshed.access_token)).active !== false) {
      throw fail();
    }
    // Demonstrate the distinction: the same signed JWT is still cryptographically
    // valid, while the online broker check must deny the terminated session.
    await verifySharedAccessToken(refreshed.access_token, policy, {
      resolveKey,
    });
    context.assertLive();
    const revoked = await fetch(context.resource, {
      method: "POST",
      redirect: "error",
      signal: AbortSignal.timeout(10000),
      headers: { authorization: "Bearer " + refreshed.access_token },
    });
    const immediate = Date.now() < refreshedAdmission.expiresAt * 1000;
    const revokedTokenRejected = revoked.status === 401 ||
      revoked.status === 403;
    await revoked.body?.cancel();
    if (!immediate || !revokedTokenRejected) throw fail();
    return {
      scope: "local-google-workspace-mcp-probe",
      experimental: true,
      ac05Qualified: false,
      productionQualified: false,
      protectedOperationsAllowed: false,
      mcpAuthorizationGranted: true,
      expiredTokenRejected,
      refreshProbeSucceeded: true,
      revokedTokenRejected,
      immediateJwtInvalidationObserved: false,
      immediateMcpRejectionObserved: true,
      onlineSessionInactiveObserved: true,
      offlineJwtStillValidAfterRevocation: true,
    };
  } finally {
    if (timer !== undefined) clearTimeout(timer);
    controller.abort();
    await Promise.all([callbackServer?.shutdown(), mcpServer?.shutdown()]);
    await Promise.all([callbackServer?.finished, mcpServer?.finished]);
  }
}

export async function executeGoogleWorkspace(
  options: WorkspaceSpikeOptions,
): Promise<WorkspaceSpikeReport> {
  // The exported entry point receives the same bounded contract as the CLI.
  const checked = parseWorkspaceSpikeArgs([
    "--execute-google-workspace",
    "--task-id",
    options.taskId,
    "--client-id",
    options.clientId,
    "--client-secret-env",
    options.clientSecretEnv,
    "--timeout-ms",
    String(options.timeoutMs),
  ]);
  if (!checked) throw fail();
  // Named environment input first. Nothing is persisted until it is validated.
  const googleSecret = Deno.env.get(options.clientSecretEnv);
  if (
    !googleSecret || googleSecret.length > 4096 ||
    !/^[\x21-\x7e]+$/.test(googleSecret)
  ) throw fail();
  const { readTaskStateFor } = await import("../src/fdh_task.ts");
  const { startExtOp, finishExtOp, readExtLedger } = await import(
    "../src/fdh_ext.ts"
  );
  const { readJob, DEFAULT_INTENT_TTL_MS } = await import(
    "../src/fdh_executor.ts"
  );
  const state = readTaskStateFor(Deno.cwd(), options.taskId);
  const agent = "fdh-google-workspace-probe";
  const key = "local-google-workspace-mcp-probe:" + crypto.randomUUID();
  const intent = await startExtOp(
    state,
    key,
    agent,
    "external",
    "Local experimental Google Workspace MCP probe; no protected operations",
  );
  if (intent.status !== "started") throw fail();
  const fence = () => {
    const op = readExtLedger(state.worktree, state.taskId).ops[key];
    const job = readJob(state.worktree, state.taskId, intent.jobId);
    const lease = job.lease;
    const age = lease ? Date.now() - Date.parse(lease.heartbeatAt) : NaN;
    if (
      !op || op.jobId !== intent.jobId || op.epoch !== intent.epoch ||
      op.outcome !== null ||
      job.epoch !== intent.epoch || job.state !== "running" || !lease ||
      lease.epoch !== intent.epoch ||
      lease.holder !== agent || lease.ttlMs !== DEFAULT_INTENT_TTL_MS ||
      !Number.isFinite(age) || age < 0 || age >= lease.ttlMs
    ) throw fail();
  };
  let succeeded = false;
  try {
    fence();
    const report = await withEphemeralKeycloak(
      (context) => googleFlow(context, options, googleSecret),
      fence,
    );
    fence();
    succeeded = true;
    return report;
  } finally {
    // Expired/replaced holders must not mutate even the intent; owned AS cleanup
    // already ran inside withEphemeralKeycloak's finally, independently of fence.
    fence();
    await finishExtOp(state, {
      key,
      epoch: intent.epoch,
      agent,
      outcome: succeeded ? "succeeded" : "failed",
    });
  }
}

export async function runWorkspaceSpikeCli(
  args: readonly string[],
  dependencies: WorkspaceCliDependencies = {
    execute: executeGoogleWorkspace,
    report: (report) => console.log(JSON.stringify(report)),
    error: () => console.error("Workspace MCP experiment rejected or failed"),
  },
): Promise<number> {
  const options = parseWorkspaceSpikeArgs(args);
  if (!options) {
    dependencies.error();
    return 2;
  }
  try {
    dependencies.report(await dependencies.execute(options));
    return 0;
  } catch {
    dependencies.error();
    return 1;
  }
}
if (import.meta.main) Deno.exitCode = await runWorkspaceSpikeCli(Deno.args);
