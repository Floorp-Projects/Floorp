// SPDX-License-Identifier: MPL-2.0
import * as client from "openid-client";
import {
  createGoogleKeyResolver,
  MAX_CLIENT_ID_CHARS,
  verifyWorkspaceIdToken,
  verifyWorkspaceSubjectIdToken,
} from "./fdh_workspace_oidc.ts";
import type {
  VerifiedWorkspaceIdentity,
  WorkspaceOidcDependencies,
} from "./fdh_workspace_oidc_types.ts";
import { WorkspaceSessions } from "./fdh_workspace_sessions.ts";
import type {
  WorkspaceSessionLogoutResult,
  WorkspaceSessionMaterial,
  WorkspaceSessionSnapshot,
} from "./fdh_workspace_sessions_types.ts";
import type {
  WorkspaceAuthorization,
  WorkspaceOAuthDependencies,
  WorkspaceOAuthErrorCode,
  WorkspaceOAuthOptions,
  WorkspaceOAuthPending,
  WorkspaceOAuthTokenResponse,
} from "./fdh_workspace_oauth_types.ts";

const ISSUER = "https://accounts.google.com";
const TOKEN = "https://oauth2.googleapis.com/token";
const REVOKE = "https://oauth2.googleapis.com/revoke";
const SCOPES = "openid email profile";
const CALLBACK_KEYS = new Set([
  "code",
  "state",
  "scope",
  "authuser",
  "prompt",
  "hd",
  "iss",
  "error",
  "error_description",
  "error_uri",
  "session_state",
]);

/** No provider response, secret, callback URL, or cause is ever interpolated. */
export class WorkspaceOAuthError extends Error {
  readonly code: WorkspaceOAuthErrorCode;
  constructor(code: WorkspaceOAuthErrorCode) {
    super("Workspace OAuth operation rejected");
    this.name = "WorkspaceOAuthError";
    this.code = code;
  }
}
function reject(code: WorkspaceOAuthErrorCode): never {
  throw new WorkspaceOAuthError(code);
}
function data(object: object, key: string): unknown {
  const d = Object.getOwnPropertyDescriptor(object, key);
  if (d === undefined) return undefined;
  if (!("value" in d)) reject("configuration-rejected");
  return d.value;
}
function boundedText(value: unknown, max: number): value is string {
  return typeof value === "string" && value.length > 0 && value.length <= max &&
    ![...value].some((character) => {
      const code = character.charCodeAt(0);
      return code < 32 || code === 127;
    });
}
function positive(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}
function optionsSnapshot(
  value: WorkspaceOAuthOptions,
): Readonly<WorkspaceOAuthOptions> {
  try {
    if (typeof value !== "object" || value === null) {
      reject("configuration-rejected");
    }
    const clientId = data(value, "clientId");
    const clientSecret = data(value, "clientSecret");
    const redirectUri = data(value, "redirectUri");
    const authorizationTtlMs = data(value, "authorizationTtlMs");
    const maxPending = data(value, "maxPending");
    const maxSessions = data(value, "maxSessions");
    if (
      !boundedText(clientId, MAX_CLIENT_ID_CHARS) ||
      !boundedText(redirectUri, 2048) ||
      (clientSecret !== undefined && !boundedText(clientSecret, 4096)) ||
      !positive(authorizationTtlMs) || !positive(maxPending) ||
      !positive(maxSessions)
    ) reject("configuration-rejected");
    const redirect = new URL(redirectUri);
    if (
      redirect.href !== redirectUri || redirect.username || redirect.password ||
      redirect.search || redirect.hash ||
      !(redirect.protocol === "https:" ||
        (redirect.protocol === "http:" && redirect.hostname === "127.0.0.1"))
    ) reject("configuration-rejected");
    return Object.freeze({
      clientId,
      clientSecret,
      redirectUri,
      authorizationTtlMs,
      maxPending,
      maxSessions,
    });
  } catch {
    return reject("configuration-rejected");
  }
}

/** Process-local OAuth client. Provider tokens stay private and memory-only.
 * A callback success is NOT a Floorp MCP bearer token or a runner identity.
 * No constructor I/O, provider/issuer override, disk cache, or implicit login.
 */
export class WorkspaceOAuthClient {
  #options: Readonly<WorkspaceOAuthOptions>;
  #configuration: client.Configuration;
  #pending = new Map<string, WorkspaceOAuthPending>();
  #authorizationGeneration = 0;
  #sessions: WorkspaceSessions;
  #clock: () => number;
  #lastClock = -Infinity;
  #oidc: WorkspaceOidcDependencies | undefined;

  constructor(
    options: WorkspaceOAuthOptions,
    dependencies: WorkspaceOAuthDependencies = {},
  ) {
    try {
      this.#options = optionsSnapshot(options);
      this.#clock = dependencies.now ?? Date.now;
      const oidc = dependencies.oidc;
      this.#oidc = Object.freeze({
        resolveKey: oidc?.resolveKey ?? createGoogleKeyResolver(),
        now: () => new Date(this.#now()),
      });
      if (typeof this.#clock !== "function") reject("configuration-rejected");
      const { clientId, clientSecret } = this.#options;
      this.#configuration = new client.Configuration(
        {
          issuer: ISSUER,
          authorization_endpoint:
            "https://accounts.google.com/o/oauth2/v2/auth",
          token_endpoint: TOKEN,
          revocation_endpoint: REVOKE,
          jwks_uri: "https://www.googleapis.com/oauth2/v3/certs",
          response_types_supported: ["code"],
          code_challenge_methods_supported: ["S256"],
          id_token_signing_alg_values_supported: ["RS256"],
        },
        clientId,
        { id_token_signed_response_alg: "RS256" },
        clientSecret === undefined
          ? client.None()
          : client.ClientSecretPost(clientSecret),
      );
      this.#configuration.timeout = 10;
      const transport: NonNullable<WorkspaceOAuthDependencies["fetch"]> =
        dependencies.fetch ?? ((url, init) =>
          fetch(url, {
            ...init,
            body: init.body instanceof Uint8Array
              ? new Uint8Array(init.body)
              : init.body,
          }));
      this.#configuration[client.customFetch] = async (url, init) => {
        const address = new URL(String(url)).href;
        if (address !== TOKEN && address !== REVOKE) {
          reject("exchange-rejected");
        }
        // oauth4webapi requires manual mode. Never follow a token POST redirect.
        const response = await transport(address, {
          ...init,
          redirect: "manual",
        });
        if (response.status >= 300 && response.status < 400) {
          await response.body?.cancel();
          reject("exchange-rejected");
        }
        return response;
      };
      this.#sessions = new WorkspaceSessions({
        now: () => this.#now(),
        maxSessions: this.#options.maxSessions,
        refresh: (token: string, identity: VerifiedWorkspaceIdentity) =>
          this.#refreshMaterial(token, identity),
        revoke: (token: string) =>
          client.tokenRevocation(this.#configuration, token),
      });
    } catch {
      throw new WorkspaceOAuthError("configuration-rejected");
    }
  }

  #now(): number {
    const current = this.#clock();
    if (
      !Number.isSafeInteger(current) || current < 0 || current < this.#lastClock
    ) reject("authorization-rejected");
    this.#lastClock = current;
    return current;
  }

  async beginAuthorization(): Promise<WorkspaceAuthorization> {
    let state: string | undefined;
    try {
      const now = this.#now();
      for (const [key, pending] of this.#pending) {
        if (pending.expiresAt <= now) this.#pending.delete(key);
      }
      if (this.#pending.size >= this.#options.maxPending) {
        reject("authorization-rejected");
      }
      const expiresAt = now + this.#options.authorizationTtlMs;
      if (!Number.isSafeInteger(expiresAt)) reject("authorization-rejected");
      state = client.randomState();
      const nonce = client.randomNonce();
      const verifier = client.randomPKCECodeVerifier();
      // Reserve before awaiting, so parallel begin calls cannot exceed capacity.
      this.#pending.set(state, Object.freeze({ nonce, verifier, expiresAt }));
      const challenge = await client.calculatePKCECodeChallenge(verifier);
      if (this.#now() >= expiresAt || !this.#pending.has(state)) {
        reject("authorization-rejected");
      }
      const url = client.buildAuthorizationUrl(this.#configuration, {
        redirect_uri: this.#options.redirectUri,
        scope: SCOPES,
        state,
        nonce,
        code_challenge: challenge,
        code_challenge_method: "S256",
        access_type: "offline",
        prompt: "consent",
        hd: "floorp.app",
        response_type: "code",
      });
      return Object.freeze({ authorizationUrl: url.href, expiresAt });
    } catch {
      if (state !== undefined) this.#pending.delete(state);
      return reject("authorization-rejected");
    }
  }

  clearPending(): void {
    this.#pending.clear();
    this.#authorizationGeneration++;
  }

  async completeAuthorization(
    callbackUrl: string,
  ): Promise<WorkspaceSessionSnapshot> {
    let tokens: WorkspaceOAuthTokenResponse | undefined;
    try {
      if (!boundedText(callbackUrl, 8192)) reject("callback-rejected");
      const url = new URL(callbackUrl);
      const target = new URL(this.#options.redirectUri);
      if (
        url.origin !== target.origin || url.pathname !== target.pathname ||
        url.username || url.password || url.hash
      ) reject("callback-rejected");
      for (const key of url.searchParams.keys()) {
        if (
          !CALLBACK_KEYS.has(key) || url.searchParams.getAll(key).length !== 1
        ) reject("callback-rejected");
      }
      const state = url.searchParams.get("state");
      if (state === null) reject("callback-rejected");
      const pending = this.#pending.get(state);
      if (pending === undefined) reject("callback-rejected");
      // Single consumption BEFORE all provider I/O, including denied/error callbacks.
      this.#pending.delete(state);
      const generation = this.#authorizationGeneration;
      const startedAt = this.#now();
      if (startedAt >= pending.expiresAt) reject("callback-rejected");
      if (
        url.searchParams.has("error") ||
        !boundedText(url.searchParams.get("code"), 4096)
      ) reject("callback-rejected");
      tokens = await client.authorizationCodeGrant(this.#configuration, url, {
        expectedState: state,
        expectedNonce: pending.nonce,
        pkceCodeVerifier: pending.verifier,
        idTokenExpected: true,
      });
      if (typeof tokens.id_token !== "string") reject("exchange-rejected");
      const identity = await verifyWorkspaceIdToken(tokens.id_token, {
        clientId: this.#options.clientId,
        expectedNonce: pending.nonce,
      }, this.#verificationDependencies());
      if (
        generation !== this.#authorizationGeneration ||
        this.#now() >= pending.expiresAt
      ) reject("callback-rejected");
      return this.#sessions.create(this.#material(tokens, identity, startedAt));
    } catch {
      if (tokens !== undefined) await this.#discard(tokens);
      return reject("callback-rejected");
    }
  }

  #verificationDependencies(): WorkspaceOidcDependencies | undefined {
    return this.#oidc;
  }

  #material(
    tokens: WorkspaceOAuthTokenResponse,
    identity: VerifiedWorkspaceIdentity,
    startedAt: number,
  ): WorkspaceSessionMaterial {
    if (
      !positive(tokens.expires_in) ||
      !boundedText(tokens.access_token, 16384) ||
      (tokens.refresh_token !== undefined &&
        !boundedText(tokens.refresh_token, 16384))
    ) reject("exchange-rejected");
    const providerExpiresAt = startedAt + tokens.expires_in * 1000;
    if (!Number.isSafeInteger(providerExpiresAt)) reject("exchange-rejected");
    const expiresAt = Math.min(providerExpiresAt, identity.expiresAt * 1000);
    if (!Number.isSafeInteger(expiresAt) || expiresAt <= this.#now()) {
      reject("exchange-rejected");
    }
    return {
      identity,
      accessToken: tokens.access_token,
      refreshToken: tokens.refresh_token,
      expiresAt,
    };
  }

  async #refreshMaterial(
    token: string,
    previous: VerifiedWorkspaceIdentity,
  ): Promise<WorkspaceSessionMaterial> {
    let tokens: WorkspaceOAuthTokenResponse | undefined;
    try {
      const startedAt = this.#now();
      tokens = await client.refreshTokenGrant(this.#configuration, token);
      // Require fresh verified domain/email/subject, not a stale cached identity.
      if (typeof tokens.id_token !== "string") reject("refresh-rejected");
      const identity = await verifyWorkspaceSubjectIdToken(tokens.id_token, {
        clientId: this.#options.clientId,
        expectedSubject: previous.identityKey,
      }, this.#verificationDependencies());
      return this.#material(tokens, identity, startedAt);
    } catch {
      if (tokens !== undefined) await this.#discard(tokens);
      return reject("refresh-rejected");
    }
  }

  async #discard(tokens: WorkspaceOAuthTokenResponse): Promise<void> {
    try {
      await client.tokenRevocation(
        this.#configuration,
        tokens.refresh_token ?? tokens.access_token,
      );
    } catch { /* Uncertain provider state, never reuse locally. */ }
  }

  getSession(id: string): WorkspaceSessionSnapshot | undefined {
    return this.#sessions.get(id);
  }
  refreshSession(id: string): Promise<WorkspaceSessionSnapshot> {
    return this.#sessions.refresh(id);
  }
  logout(
    id: string,
  ): Promise<WorkspaceSessionLogoutResult> {
    return this.#sessions.logout(id);
  }
}
