// SPDX-License-Identifier: MPL-2.0
import { types } from "node:util";
import type {
  SharedServiceConfig,
  SharedServiceSettings,
} from "./fdh_shared_config_types.ts";

const rejected = () => new Error("Shared service configuration rejected");
const keys = [
  "publicOrigin",
  "issuer",
  "gatewayLoginUrl",
  "expectedGoogleClientId",
  "readerClientId",
  "readerSecretEnv",
] as const;

/** Pure parsing: no environment, session, network, or gateway attestation. */
export function parseSharedServiceConfig(value: unknown): SharedServiceConfig {
  if (value === undefined || value === null) {
    return Object.freeze({
      mode: "unconfigured",
      loginAvailable: false,
      productionQualified: false,
      protectedOperationsAllowed: false,
    });
  }
  try {
    if (
      typeof value !== "object" || Array.isArray(value) || types.isProxy(value)
    ) {
      throw rejected();
    }
    const prototype = Object.getPrototypeOf(value);
    if (prototype !== Object.prototype && prototype !== null) throw rejected();
    const descriptors = Object.getOwnPropertyDescriptors(value);
    const ownKeys = Reflect.ownKeys(value);
    if (ownKeys.length !== keys.length) throw rejected();
    for (const key of ownKeys) {
      if (typeof key !== "string" || !keys.some((name) => name === key)) {
        throw rejected();
      }
    }
    const field = (key: keyof SharedServiceSettings): string => {
      const descriptor = descriptors[key];
      if (!descriptor || !("value" in descriptor)) throw rejected();
      const text: unknown = descriptor.value;
      if (typeof text !== "string" || text.length < 1 || text.length > 2048) {
        throw rejected();
      }
      for (const character of text) {
        const code = character.charCodeAt(0);
        if (code < 33 || code > 126) throw rejected();
      }
      return text;
    };
    const publicOrigin = field("publicOrigin");
    const issuer = field("issuer");
    const gatewayLoginUrl = field("gatewayLoginUrl");
    const expectedGoogleClientId = field("expectedGoogleClientId");
    const readerClientId = field("readerClientId");
    const readerSecretEnv = field("readerSecretEnv");
    const https = (text: string): URL => {
      const url = new URL(text);
      if (
        url.protocol !== "https:" || url.username || url.password ||
        url.search || url.hash
      ) {
        throw rejected();
      }
      return url;
    };
    const origin = https(publicOrigin);
    if (origin.origin !== publicOrigin || origin.pathname !== "/") {
      throw rejected();
    }
    const provider = https(issuer);
    if (provider.href !== issuer || issuer.endsWith("/")) throw rejected();
    const gateway = https(gatewayLoginUrl);
    const reservedPaths = [
      "/",
      "/auth/login",
      "/auth/bootstrap",
      "/mcp",
      "/api/auth/status",
      "/workspace",
    ];
    if (gateway.href !== gatewayLoginUrl || gateway.origin !== publicOrigin) {
      throw rejected();
    }
    if (!/^\/[A-Za-z0-9/_-]+$/.test(gateway.pathname)) throw rejected();
    if (
      gateway.pathname.includes("//") ||
      reservedPaths.includes(gateway.pathname)
    ) throw rejected();
    if (
      !/^[A-Za-z0-9_-]{1,200}\.apps\.googleusercontent\.com$/.test(
        expectedGoogleClientId,
      )
    ) throw rejected();
    if (!/^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$/.test(readerClientId)) {
      throw rejected();
    }
    if (!/^FDH_[A-Z][A-Z0-9_]{0,62}_SECRET$/.test(readerSecretEnv)) {
      throw rejected();
    }
    const server = Object.freeze({
      publicOrigin,
      issuer,
      gatewayLoginUrl,
      expectedGoogleClientId,
      readerClientId,
      readerSecretEnv,
    });
    const publicSettings = Object.freeze({
      mode: "configured" as const,
      publicOrigin,
      loginPath: "/auth/login" as const,
      mcpResource: publicOrigin + "/mcp",
      apiAuthStatusPath: "/api/auth/status" as const,
      productionQualified: false as const,
      protectedOperationsAllowed: false as const,
    });
    return Object.freeze({
      mode: "configured",
      public: publicSettings,
      server,
    });
  } catch {
    throw rejected();
  }
}
