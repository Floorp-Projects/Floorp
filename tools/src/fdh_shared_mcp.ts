// SPDX-License-Identifier: MPL-2.0
// SDK wildcard types exports append .d.ts; bind the extensionless declaration.
// @deno-types="npm:@modelcontextprotocol/sdk@1.32.2/server/mcp"
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
// @deno-types="npm:@modelcontextprotocol/sdk@1.32.2/server/webStandardStreamableHttp"
import { WebStandardStreamableHTTPServerTransport } from "@modelcontextprotocol/sdk/server/webStandardStreamableHttp.js";
import { SharedAccessError } from "./fdh_shared_access.ts";
import type { VerifiedSharedAccessToken } from "./fdh_shared_access_types.ts";
import type {
  SharedMcpHandler,
  SharedMcpPolicy,
  SharedMcpTrustedDependencies,
  SharedMcpWorkspaceBinding,
} from "./fdh_shared_mcp_types.ts";

const SCOPE = /^[\x21\x23-\x5B\x5D-\x7E]{1,256}$/;
function own(value: unknown, key: string): unknown {
  if (typeof value !== "object" || value === null) {
    throw Error("Shared MCP configuration rejected");
  }
  const descriptor = Object.getOwnPropertyDescriptor(value, key);
  if (!descriptor || !("value" in descriptor)) {
    throw Error("Shared MCP configuration rejected");
  }
  return descriptor.value;
}
function pinnedUrl(value: unknown, local: boolean): URL {
  if (typeof value !== "string" || value.length > 2048) {
    throw Error("Shared MCP configuration rejected");
  }
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw Error("Shared MCP configuration rejected");
  }
  const authority = /^[a-z]+:\/\/([^/]+)(?:\/|$)/.exec(value)?.[1];
  if (
    url.href !== value || url.username || url.password || url.search ||
    url.hash || value.includes("?") || value.includes("#") ||
    authority !== url.host
  ) throw Error("Shared MCP configuration rejected");
  if (
    url.protocol !== "https:" &&
    !(local && url.protocol === "http:" &&
      (url.hostname === "127.0.0.1" || url.hostname === "[::1]"))
  ) throw Error("Shared MCP configuration rejected");
  return url;
}
function validBinding(
  value: SharedMcpWorkspaceBinding | null,
  subject: string,
): boolean {
  if (!value) return false;
  return value.kind === "verified-online-google-binding" &&
    value.active === true &&
    value.sessionProvider === "google" &&
    value.authorizationSubject === subject &&
    value.identityIssuer === "https://accounts.google.com" &&
    value.emailVerified === true &&
    value.hostedDomain === "floorp.app" && typeof value.email === "string" &&
    value.email.length <= 320 && value.email.endsWith("@floorp.app") &&
    value.email.split("@").length === 2 &&
    value.email.split("@")[0].length > 0 &&
    !/\s/.test(value.email) &&
    ![...value.email].some((char) =>
      char.charCodeAt(0) < 32 || char.charCodeAt(0) === 127
    ) &&
    typeof value.identityKey === "string" && value.identityKey.length > 0 &&
    value.identityKey.length <= 255 &&
    ![...value.identityKey].some((char) =>
      char.charCodeAt(0) <= 32 || char.charCodeAt(0) === 127
    );
}

/** Resource server only. The supplied online authorizer is a trusted deployment
 * boundary, not an HTTP option or an offline claims shortcut. Only a read-only
 * probe is registered: no job, runner, merge, release or approval operation. */
export function createSharedMcpHandler(
  policy: SharedMcpPolicy,
  dependencies: SharedMcpTrustedDependencies,
): SharedMcpHandler {
  const localValue = Object.getOwnPropertyDescriptor(policy, "localSpike");
  if (
    localValue &&
    (!("value" in localValue) || typeof localValue.value !== "boolean")
  ) throw Error("Shared MCP configuration rejected");
  const local = localValue?.value === true;
  const issuer = pinnedUrl(own(policy, "issuer"), local).href;
  const resource = pinnedUrl(own(policy, "resourceAudience"), local);
  const inputScopes = own(policy, "requiredScopes");
  if (
    !Array.isArray(inputScopes) || inputScopes.length < 1 ||
    inputScopes.length > 128
  ) throw Error("Shared MCP configuration rejected");
  const scopes: string[] = [];
  for (let i = 0; i < inputScopes.length; i++) {
    const scope = own(inputScopes, String(i));
    if (
      typeof scope !== "string" || !SCOPE.test(scope) || scopes.includes(scope)
    ) throw Error("Shared MCP configuration rejected");
    scopes.push(scope);
  }
  Object.freeze(scopes);
  const verify = own(dependencies, "verifyAccessToken");
  const online = own(dependencies, "resolveOnlineWorkspaceBinding");
  if (typeof verify !== "function" || typeof online !== "function") {
    throw Error("Shared MCP configuration rejected");
  }
  const verifyAccessToken =
    verify as SharedMcpTrustedDependencies["verifyAccessToken"];
  const resolveOnlineWorkspaceBinding =
    online as SharedMcpTrustedDependencies["resolveOnlineWorkspaceBinding"];
  const metadataPath = "/.well-known/oauth-protected-resource" +
    resource.pathname;
  const metadataUrl = resource.origin + metadataPath;
  const metadata = JSON.stringify({
    resource: resource.href,
    authorization_servers: [issuer],
    scopes_supported: scopes,
    bearer_methods_supported: ["header"],
    resource_name: "FDH shared MCP local qualification probe",
  });
  function refusal(status: number, insufficient = false): Response {
    const headers = new Headers({
      "cache-control": "no-store",
      "content-type": "text/plain; charset=utf-8",
    });
    if (status === 401 || insufficient) {
      headers.set(
        "www-authenticate",
        'Bearer resource_metadata="' + metadataUrl + '"' +
          (insufficient
            ? ', error="insufficient_scope", scope="' + scopes.join(" ") + '"'
            : ""),
      );
    }
    return new Response(
      status === 503
        ? "Shared authorization unavailable"
        : "Shared authorization rejected",
      { status, headers },
    );
  }
  return async (request: Request): Promise<Response> => {
    const url = new URL(request.url);
    const host = request.headers.get("host");
    if (
      url.origin !== resource.origin ||
      (host !== null && host.toLowerCase() !== resource.host)
    ) return refusal(403);
    const origin = request.headers.get("origin");
    if (origin !== null && origin !== resource.origin) return refusal(403);
    if (
      url.pathname === metadataPath && !url.search && request.method === "GET"
    ) {
      return new Response(metadata, {
        headers: {
          "content-type": "application/json",
          "cache-control": "no-store",
        },
      });
    }
    if (url.pathname !== resource.pathname || url.search) {
      return new Response("Not found", { status: 404 });
    }
    const authorization = request.headers.get("authorization") ?? "";
    if (authorization.length > 16 * 1024 + 7) return refusal(401);
    const token = /^Bearer ([^\s,]+)$/i.exec(authorization)?.[1];
    if (!token) return refusal(401);
    let admitted: VerifiedSharedAccessToken;
    try {
      admitted = await verifyAccessToken(token);
    } catch (error) {
      return error instanceof SharedAccessError &&
          error.code === "insufficient-scope"
        ? refusal(403, true)
        : refusal(401);
    }
    if (
      admitted.kind !== "verified-shared-access-token" ||
      admitted.issuer !== issuer ||
      admitted.resourceAudience !== resource.href ||
      !scopes.every((scope) => admitted.scopes.includes(scope))
    ) return refusal(403);
    let binding: SharedMcpWorkspaceBinding | null;
    try {
      binding = await resolveOnlineWorkspaceBinding(token, admitted);
      if (!validBinding(binding, admitted.subject)) return refusal(403);
    } catch {
      return refusal(503);
    }
    if (admitted.expiresAt <= Math.floor(Date.now() / 1000)) {
      return refusal(401);
    }
    const server = new McpServer({
      name: "fdh-shared-authz-spike",
      version: "0.1.0",
    }, { maxToolInputElements: 64 });
    const transport = new WebStandardStreamableHTTPServerTransport({
      sessionIdGenerator: undefined,
      enableJsonResponse: true,
      maxRequestBodySize: 65536,
    });
    server.registerTool("fdh.authz.probe", {
      description:
        "Read-only proof of FDH scope admission and online Workspace binding; never executes protected operations.",
      annotations: {
        readOnlyHint: true,
        destructiveHint: false,
        idempotentHint: true,
        openWorldHint: false,
      },
    }, () => ({
      content: [{
        type: "text",
        text: JSON.stringify({
          scope: "local-shared-mcp-spike-only",
          issuer,
          resource: resource.href,
          scopes: admitted.scopes,
          hostedDomain: "floorp.app",
          identityKeySource: "google-sub-through-online-broker",
          protectedOperationsAllowed: false,
          productionQualified: false,
        }),
      }],
    }));
    try {
      await server.connect(transport);
      // Authorization is closed over by this per-request server. Broker sub is
      // not an OAuth client_id; do not mislabel it or forward the raw bearer.
      return await transport.handleRequest(request);
    } catch {
      return refusal(503);
    } finally {
      await server.close();
    }
  };
}
