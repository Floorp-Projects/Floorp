import path, { resolve } from "node:path";
import type { Plugin } from "vite";
import type { ResolvedConfig } from "vite";
import { normalizePath } from "vite";
import { promises as dns } from "node:dns";
import { createHash } from "node:crypto";
import { fileURLToPath } from "node:url";

//https://github.com/vitejs/vite/blob/22b299429599834bf1855b53264a28ae5ff8f888/packages/vite/src/node/utils.ts#L385
function isObject(value: unknown): value is Record<string, any> {
  return Object.prototype.toString.call(value) === "[object Object]";
}

const wildcardHosts = new Set([
  "0.0.0.0",
  "::",
  "0000:0000:0000:0000:0000:0000:0000:0000",
]);

interface Hostname {
  /** undefined sets the default behaviour of server.listen */
  host: string | undefined;
  /** resolve to localhost when possible */
  name: string;
}

async function resolveHostname(
  optionsHost: string | boolean | undefined,
): Promise<Hostname> {
  let host: string | undefined;
  if (optionsHost === undefined || optionsHost === false) {
    // Use a secure default
    host = "localhost";
  } else if (optionsHost === true) {
    // If passed --host in the CLI without arguments
    host = undefined; // undefined typically means 0.0.0.0 or :: (listen on all IPs)
  } else {
    host = optionsHost;
  }

  // Set host name to localhost when possible
  let name = host === undefined || wildcardHosts.has(host) ? "localhost" : host;

  if (host === "localhost") {
    // See #8647 for more details.
    const localhostAddr = await getLocalhostAddressIfDiffersFromDNS();
    if (localhostAddr) {
      name = localhostAddr;
    }
  }

  return { host, name };
}

async function getLocalhostAddressIfDiffersFromDNS(): Promise<
  string | undefined
> {
  const [nodeResult, dnsResult] = await Promise.all([
    dns.lookup("localhost"),
    dns.lookup("localhost", { verbatim: true }),
  ]);
  const isSame =
    nodeResult.family === dnsResult.family &&
    nodeResult.address === dnsResult.address;
  return isSame ? undefined : nodeResult.address;
}
function getHash(text: Buffer | string, length = 8): string {
  const h = createHash("sha256")
    .update(text)
    .digest("hex")
    .substring(0, length);
  if (length <= 64) return h;
  return h.padEnd(length, "_");
}

function canJsonParse(value: any): boolean {
  try {
    JSON.parse(value);
    return true;
  } catch {
    return false;
  }
}

function handleDefineValue(value: any): string {
  if (typeof value === "undefined") return "undefined";
  if (typeof value === "string") return value;
  return JSON.stringify(value);
}

/**
 * Like `JSON.stringify` but keeps raw string values as a literal
 * in the generated code. For example: `"window"` would refer to
 * the global `window` object directly.
 */
function serializeDefine(define: Record<string, any>): string {
  let res = `{`;
  const keys = Object.keys(define);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const val = define[key];
    res += `${JSON.stringify(key)}: ${handleDefineValue(val)}`;
    if (i !== keys.length - 1) {
      res += `, `;
    }
  }
  return res + `}`;
}

export const VITE_OVERRIDE_PACKAGE_DIR = resolve(
  // import.meta.url is `dist/node/constants.js` after bundle
  fileURLToPath(import.meta.url),
  "../..",
);

/**
 * some values used by the client needs to be dynamically injected by the server
 * @server-only
 */
export function clientInjectionsPlugin(): Plugin {
  let injectConfigValues: (code: string) => string;
  let config: ResolvedConfig;

  return {
    name: "vite:client-inject",
    configResolved(_config) {
      config = _config;
    },
    async buildStart() {
      const resolvedServerHostname = (await resolveHostname(config.server.host))
        .name;
      const resolvedServerPort = config.server.port!;
      const devBase = config.base;

      const serverHost = `${resolvedServerHostname}:${resolvedServerPort}${devBase}`;

      let hmrConfig = config.server.hmr;
      hmrConfig = isObject(hmrConfig) ? hmrConfig : undefined;
      const host = hmrConfig?.host || null;
      const protocol = hmrConfig?.protocol || null;
      const timeout = hmrConfig?.timeout || 30000;
      const overlay = hmrConfig?.overlay !== false;
      const isHmrServerSpecified = !!hmrConfig?.server;
      const hmrConfigName = path.basename(
        config.configFile || "vite.config.js",
      );

      // hmr.clientPort -> hmr.port
      // -> (24678 if middleware mode and HMR server is not specified) -> new URL(import.meta.url).port
      let port = hmrConfig?.clientPort || hmrConfig?.port || null;
      if (config.server.middlewareMode && !isHmrServerSpecified) {
        port ||= 24678;
      }

      let directTarget = hmrConfig?.host || resolvedServerHostname;
      directTarget += `:${hmrConfig?.port || resolvedServerPort}`;
      directTarget += devBase;

      let hmrBase = devBase;
      if (hmrConfig?.path) {
        hmrBase = path.posix.join(hmrBase, hmrConfig.path);
      }

      const userDefine: Record<string, any> = {};
      for (const key in config.define) {
        // import.meta.env.* is handled in `importAnalysis` plugin
        if (!key.startsWith("import.meta.env.")) {
          userDefine[key] = config.define[key];
        }
      }
      const serializedDefines = serializeDefine(userDefine);

      const modeReplacement = escapeReplacement(config.mode);
      const baseReplacement = escapeReplacement(devBase);
      const definesReplacement = () => serializedDefines;
      const serverHostReplacement = escapeReplacement(serverHost);
      const hmrProtocolReplacement = escapeReplacement(protocol);
      const hmrHostnameReplacement = escapeReplacement(host);
      const hmrPortReplacement = escapeReplacement(port);
      const hmrDirectTargetReplacement = escapeReplacement(directTarget);
      const hmrBaseReplacement = escapeReplacement(hmrBase);
      const hmrTimeoutReplacement = escapeReplacement(timeout);
      const hmrEnableOverlayReplacement = escapeReplacement(overlay);
      const hmrConfigNameReplacement = escapeReplacement(hmrConfigName);

      injectConfigValues = (code: string) => {
        return code
          .replace(`__MODE__`, modeReplacement)
          .replace(/__BASE__/g, baseReplacement)
          .replace(`__DEFINES__`, definesReplacement)
          .replace(`__SERVER_HOST__`, serverHostReplacement)
          .replace(`__HMR_PROTOCOL__`, hmrProtocolReplacement)
          .replace(`__HMR_HOSTNAME__`, hmrHostnameReplacement)
          .replace(`__HMR_PORT__`, hmrPortReplacement)
          .replace(`__HMR_DIRECT_TARGET__`, hmrDirectTargetReplacement)
          .replace(`__HMR_BASE__`, hmrBaseReplacement)
          .replace(`__HMR_TIMEOUT__`, hmrTimeoutReplacement)
          .replace(`__HMR_ENABLE_OVERLAY__`, hmrEnableOverlayReplacement)
          .replace(`__HMR_CONFIG_NAME__`, hmrConfigNameReplacement);
      };
    },
    async transform(code, id, options) {
      if (
        id.includes("@nora/vite-override")
        //|| id === normalizedEnvEntry
      ) {

        return injectConfigValues(code);
      }
    },
  };
}

function escapeReplacement(value: string | number | boolean | null) {
  const jsonValue = JSON.stringify(value);
  return () => jsonValue;
}
