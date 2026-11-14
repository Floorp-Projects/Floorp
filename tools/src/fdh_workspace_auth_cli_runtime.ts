// SPDX-License-Identifier: MPL-2.0
import { workspaceProbeRedirect } from "./fdh_workspace_auth_cli_validation.ts";
import {
  WorkspaceOAuthClient,
  WorkspaceOAuthError,
} from "./fdh_workspace_oauth.ts";
import type {
  WorkspaceAuthProbeDependencies,
  WorkspaceAuthProbeOptions,
  WorkspaceAuthProbeResult,
  WorkspaceAuthProbeServer,
} from "./fdh_workspace_auth_cli_types.ts";

/** Explicit operator probe only. A loopback OAuth callback, NOT an AS/MCP server.
 * No state, provider token, session identifier, or secret is stored on disk.
 * The ephemeral consent URL is written once to operator stderr, never evidence.
 */
export async function runWorkspaceAuthProbe(
  options: WorkspaceAuthProbeOptions,
  dependencies: WorkspaceAuthProbeDependencies = {},
): Promise<WorkspaceAuthProbeResult> {
  let redirect: URL;
  try {
    redirect = workspaceProbeRedirect(options.redirectUri);
    if (
      !Number.isSafeInteger(options.timeoutMs) || options.timeoutMs <= 0 ||
      options.timeoutMs > 900000
    ) throw new Error();
  } catch {
    throw new WorkspaceOAuthError("configuration-rejected");
  }
  const oauth = dependencies.client ?? new WorkspaceOAuthClient({
    clientId: options.clientId,
    clientSecret: options.clientSecret,
    redirectUri: options.redirectUri,
    authorizationTtlMs: options.timeoutMs,
    maxPending: 1,
    maxSessions: 1,
  });
  const flow = await oauth.beginAuthorization();
  const state = new URL(flow.authorizationUrl).searchParams.get("state");
  const controller = new AbortController();
  let sessionId: string | undefined;
  let finish = (_success: boolean) => {};
  const completed = new Promise<boolean>((resolve) => {
    finish = resolve;
  });
  let callbackStarted = false;
  let server: WorkspaceAuthProbeServer | undefined;
  let timer: ReturnType<typeof setTimeout> | undefined;
  const headers = {
    "content-type": "text/plain; charset=utf-8",
    "cache-control": "no-store",
    "referrer-policy": "no-referrer",
    "x-frame-options": "DENY",
    "content-security-policy": "default-src 'none'; frame-ancestors 'none'",
  };
  try {
    const serve = dependencies.serve ??
      ((options, handler) => Deno.serve(options, handler));
    server = serve({
      hostname: "127.0.0.1",
      port: Number(redirect.port),
      signal: controller.signal,
      onListen() {},
      onError() {
        return new Response("OAuth callback rejected", {
          status: 400,
          headers,
        });
      },
    }, async (request) => {
      const url = new URL(request.url);
      if (
        url.origin !== redirect.origin || url.pathname !== redirect.pathname
      ) return new Response("Not found", { status: 404, headers });
      if (request.method !== "GET") {
        return new Response("Method not allowed", { status: 405, headers });
      }
      if (
        callbackStarted || url.searchParams.getAll("state").length !== 1 ||
        url.searchParams.get("state") !== state
      ) {
        return new Response("OAuth callback rejected", {
          status: 400,
          headers,
        });
      }
      callbackStarted = true;
      try {
        sessionId = (await oauth.completeAuthorization(url.href)).id;
        finish(true);
        return new Response(
          "FDH authentication received. You may close this tab.",
          { headers },
        );
      } catch {
        finish(false);
        return new Response("OAuth callback rejected", {
          status: 400,
          headers,
        });
      }
    });
    timer = setTimeout(() => {
      oauth.clearPending();
      finish(false);
      controller.abort();
    }, options.timeoutMs);
    const announce = dependencies.announce ??
      ((url: string) =>
        console.error(
          "Open this ephemeral Google consent URL in your browser; do not publish it:\n" +
            url,
        ));
    announce(flow.authorizationUrl);
    if (!await completed || sessionId === undefined) {
      throw new WorkspaceOAuthError("callback-rejected");
    }
    // timeoutMs bounds browser authorization; subsequent provider operations
    // have the client library's independent ten-second timeout each.
    if (timer !== undefined) clearTimeout(timer);
    timer = undefined;
    // These are real provider operations, not a decoded-ID-token-only shortcut.
    const refreshed = await oauth.refreshSession(sessionId);
    const logout = await oauth.logout(sessionId);
    sessionId = undefined;
    return Object.freeze({
      schemaVersion: 1,
      scope: "workspace-oauth-probe",
      codeExchangeVerified: true,
      refreshVerified: true,
      localLogout: true,
      providerRevocationConfirmed: logout.providerRevocationConfirmed,
      identity: Object.freeze({
        subject: refreshed.identity.identityKey,
        email: refreshed.identity.email,
        hostedDomain: refreshed.identity.hostedDomain,
        issuer: refreshed.identity.issuer,
      }),
      mcpAuthorizationGranted: false,
      ac05Qualified: false,
    });
  } catch {
    throw new WorkspaceOAuthError("exchange-rejected");
  } finally {
    if (timer !== undefined) clearTimeout(timer);
    oauth.clearPending();
    if (sessionId !== undefined) await oauth.logout(sessionId).catch(() => {});
    if (server !== undefined) await server.shutdown().catch(() => {});
  }
}
