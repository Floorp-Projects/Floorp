// SPDX-License-Identifier: MPL-2.0
import { types } from "node:util";
import { parseSharedServiceConfig } from "./fdh_shared_config.ts";
import { createSharedMcpHandler } from "./fdh_shared_mcp.ts";
import type { SharedMcpTrustedDependencies } from "./fdh_shared_mcp_types.ts";
import type {
  SharedEntryBootstrap,
  SharedEntryHandler,
} from "./fdh_shared_entry_types.ts";

const metadataPath = "/.well-known/oauth-protected-resource/mcp";
const unavailable = Object.freeze({
  mode: "unconfigured" as const,
  loginAvailable: false as const,
  productionQualified: false as const,
  protectedOperationsAllowed: false as const,
});
const landing =
  '<!doctype html><html lang="en"><meta charset="utf-8"><title>FDH shared workspace</title><h1>FDH shared workspace</h1><p>Protected workspace access is not available here.</p><a href="/auth/login">Sign in with Google</a></html>';

/** Reject exotic containers before reflection; never evaluate getters or Proxy traps.
 * MCP factory then performs its existing hook validation and captures these copies. */
function snapshotDependencies(
  value: unknown,
): SharedMcpTrustedDependencies | null {
  if (typeof value !== "object" || value === null || types.isProxy(value)) {
    return null;
  }
  const prototype = Object.getPrototypeOf(value);
  if (prototype !== Object.prototype && prototype !== null) return null;
  const keys = Reflect.ownKeys(value);
  if (
    keys.length !== 2 || !keys.includes("verifyAccessToken") ||
    !keys.includes("resolveOnlineWorkspaceBinding")
  ) return null;
  const verify = Object.getOwnPropertyDescriptor(value, "verifyAccessToken");
  const online = Object.getOwnPropertyDescriptor(
    value,
    "resolveOnlineWorkspaceBinding",
  );
  if (!verify || !("value" in verify) || !online || !("value" in online)) {
    return null;
  }
  if (
    typeof verify.value !== "function" || types.isProxy(verify.value) ||
    typeof online.value !== "function" || types.isProxy(online.value)
  ) return null;
  return Object.freeze({
    verifyAccessToken: verify
      .value as SharedMcpTrustedDependencies["verifyAccessToken"],
    resolveOnlineWorkspaceBinding: online
      .value as SharedMcpTrustedDependencies["resolveOnlineWorkspaceBinding"],
  });
}
function harden(response: Response): Response {
  const headers = new Headers(response.headers);
  headers.set("cache-control", "no-store");
  headers.set(
    "content-security-policy",
    "default-src 'none'; base-uri 'none'; frame-ancestors 'none'; form-action 'none'",
  );
  headers.set("referrer-policy", "no-referrer");
  headers.set("x-content-type-options", "nosniff");
  headers.set("x-frame-options", "DENY");
  return new Response(response.body, {
    status: response.status,
    statusText: response.statusText,
    headers,
  });
}
function json(value: unknown, status: number): Response {
  return harden(
    new Response(JSON.stringify(value), {
      status,
      headers: { "content-type": "application/json; charset=utf-8" },
    }),
  );
}
function text(body: string, status: number, headers?: HeadersInit): Response {
  return harden(new Response(body, { status, headers }));
}

/** Pure request-handler factory, not a server or OIDC implementation.
 * gatewayLoginUrl pins the existing maintained gateway/OIDC client's start route
 * on the reverse-proxied HTTPS origin, NOT a newly constructed AS authorize URL.
 * That gateway exclusively owns PKCE, state, nonce, callback and session cookies.
 * Hook presence only means configuration exists: no connectivity/SSO proof here.
 * No workspace session is asserted; only the existing read-only MCP probe composes.
 */
export function createSharedEntryHandler(
  operatorConfig: unknown,
  privateDependencies?: SharedMcpTrustedDependencies,
): SharedEntryHandler {
  const config = parseSharedServiceConfig(operatorConfig);
  const dependencies = snapshotDependencies(privateDependencies);
  const configured = config.mode === "configured" && dependencies !== null;
  const pinned = config.mode === "configured"
    ? new URL(config.server.publicOrigin)
    : null;
  const gatewayLoginUrl = config.mode === "configured"
    ? config.server.gatewayLoginUrl
    : null;
  const bootstrap: SharedEntryBootstrap = configured
    ? Object.freeze({
      mode: "configured",
      publicOrigin: config.public.publicOrigin,
      loginAvailable: true,
      loginPath: "/auth/login",
      mcpResource: config.public.mcpResource,
      apiAuthStatusPath: "/api/auth/status",
      productionQualified: false,
      protectedOperationsAllowed: false,
    })
    : unavailable;
  const mcp = configured
    ? createSharedMcpHandler({
      issuer: config.server.issuer,
      resourceAudience: config.public.mcpResource,
      requiredScopes: ["fdh:probe"],
    }, dependencies)
    : null;

  return async (request: Request): Promise<Response> => {
    const url = new URL(request.url);
    const host = request.headers.get("host");
    const origin = request.headers.get("origin");
    if (
      pinned &&
      (url.origin !== pinned.origin ||
        (host !== null && host.toLowerCase() !== pinned.host) ||
        (origin !== null && origin !== pinned.origin))
    ) return text("Shared entry request rejected", 403);
    // No usable configuration means no public login, metadata or private IO.
    if (!mcp) {
      if (url.pathname === "/") {
        return text(
          '<!doctype html><html lang="en"><meta charset="utf-8"><title>FDH shared workspace unavailable</title><h1>Shared service unconfigured</h1><p>Google login is disabled.</p></html>',
          503,
          { "content-type": "text/html; charset=utf-8" },
        );
      }
      return json(bootstrap, 503);
    }
    // Check delimiters too: URL.search/hash hide an empty trailing ? or #.
    if (request.url.includes("?") || request.url.includes("#")) {
      return text("Not found", 404);
    }
    if (["/", "/auth/bootstrap", "/auth/login"].includes(url.pathname)) {
      if (request.method !== "GET") {
        return text("Method not allowed", 405, { allow: "GET" });
      }
      if (url.pathname === "/auth/bootstrap") return json(bootstrap, 200);
      if (url.pathname === "/") {
        return text(landing, 200, {
          "content-type": "text/html; charset=utf-8",
        });
      }
      return text("Continue at the configured gateway", 303, {
        location: gatewayLoginUrl!,
      });
    }
    if (url.pathname === "/mcp" || url.pathname === metadataPath) {
      return harden(await mcp(request));
    }
    if (url.pathname === "/workspace" || url.pathname === "/api/auth/status") {
      return json({
        error: "authenticated-router-unavailable",
        productionQualified: false,
        protectedOperationsAllowed: false,
      }, 503);
    }
    return text("Not found", 404);
  };
}
