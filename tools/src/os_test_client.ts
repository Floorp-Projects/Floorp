// SPDX-License-Identifier: MPL-2.0

import type {
  OsTestClient,
  OsTestClientArgs,
  OsTestClientDependencies,
  OsTestClientOptions,
} from "./os_test_client_types.ts";

const MAX_TOKEN_BYTES = 4096;

/** Explicit paths only. No Home/profile discovery and no raw-token CLI/env. */
export function resolveTokenFile(
  flag?: string,
  env?: string,
): string | undefined {
  const selected = flag !== undefined ? flag : env;
  if (selected === undefined) return undefined;
  if (!selected.trim()) throw new Error("Token file path must not be empty");
  return selected;
}

/** Literal hosts only: URL normalization must not turn DNS/alternate IPv4 into approval. */
export function validateAuthenticatedBaseUrl(baseUrl: string): URL {
  let url: URL;
  try {
    url = new URL(baseUrl);
  } catch {
    throw new Error("Invalid authenticated OS API base URL");
  }
  if (
    !/^http:\/\/(?:127\.0\.0\.1|\[::1\])(?::[0-9]+)?\/?$/.test(baseUrl) ||
    url.protocol !== "http:" ||
    !["127.0.0.1", "[::1]"].includes(url.hostname) ||
    url.username || url.password || url.search || url.hash ||
    url.pathname !== "/"
  ) {
    throw new Error(
      "Authenticated OS API requires an HTTP literal-loopback root origin",
    );
  }
  return url;
}

/** Reconcile both CLI scopes before launching/probing; never echo argument values. */
export function reconcileOsTestClientArgs(
  wrapper: string[],
  verifier: string[],
): OsTestClientArgs {
  const result: OsTestClientArgs = { wrapperArgs: [], verifierArgs: [] };
  const values = new Map<string, string>();
  function scan(args: string[]): string[] {
    const rest: string[] = [];
    for (let i = 0; i < args.length; i++) {
      const arg = args[i];
      const name = arg.split("=", 1)[0];
      if (["--token", "--auth-token", "--authorization"].includes(name)) {
        throw new Error(
          "Raw token arguments are unsupported; use --token-file",
        );
      }
      if (name !== "--base-url" && name !== "--token-file") {
        rest.push(arg);
        continue;
      }
      const equal = arg.indexOf("=");
      const value = equal < 0 ? args[++i] : arg.slice(equal + 1);
      if (value === undefined || !value.trim() || value.startsWith("--")) {
        throw new Error("OS API target arguments require a nonempty value");
      }
      if (values.has(name)) {
        throw new Error("Duplicate OS API target arguments across CLI scopes");
      }
      values.set(name, value);
    }
    return rest;
  }
  result.wrapperArgs = scan(wrapper);
  result.verifierArgs = scan(verifier);
  result.baseUrlFlag = values.get("--base-url");
  result.tokenFileFlag = values.get("--token-file");
  return result;
}

async function readBoundedTokenFile(
  path: string,
  maxBytes: number,
): Promise<Uint8Array> {
  const file = await Deno.open(path, { read: true });
  try {
    const bytes = new Uint8Array(maxBytes + 1);
    let length = 0;
    while (length < bytes.length) {
      const count = await file.read(bytes.subarray(length));
      if (count === null) break;
      length += count;
    }
    return bytes.subarray(0, length);
  } finally {
    file.close();
  }
}

/** The secret lives only in this closure, never on enumerable runtime/client state. */
export async function createOsTestClient(
  options: OsTestClientOptions,
  dependencies: OsTestClientDependencies = {},
): Promise<OsTestClient> {
  const origin = options.tokenFile === undefined
    ? undefined
    : validateAuthenticatedBaseUrl(options.baseUrl);
  let token: string | undefined;
  if (options.tokenFile !== undefined) {
    try {
      const bytes = await (dependencies.readTokenFile ?? readBoundedTokenFile)(
        options.tokenFile,
        MAX_TOKEN_BYTES,
      );
      if (bytes.length > MAX_TOKEN_BYTES) throw new Error();
      const text = new TextDecoder("utf-8", { fatal: true }).decode(bytes);
      token = text.replace(/\r?\n$/, "");
      if (!/^[A-Za-z0-9._~+\/-]+={0,2}$/.test(token)) throw new Error();
    } catch {
      throw new Error("Unable to load a valid OS API token file");
    }
  }
  const baseUrl = options.baseUrl.replace(/\/$/, "");
  const fetchRequest = dependencies.fetch ?? fetch;
  return {
    baseUrl,
    async request(path, init = {}, stream = false) {
      try {
        const target = new URL(baseUrl + path);
        if (
          origin &&
          (target.origin !== origin.origin || target.username ||
            target.password)
        ) {
          throw new Error();
        }
        const headers = new Headers(init.headers);
        // No caller-supplied auth, even in unauthenticated mode.
        headers.delete("authorization");
        if (token !== undefined) {
          headers.set("authorization", "Bearer " + token);
        }
        const response = await fetchRequest(target, {
          ...init,
          headers,
          redirect: "error",
        });
        // Also fail closed for injected/nonstandard fetch implementations.
        if (
          response.status >= 300 && response.status < 400 || response.redirected
        ) {
          await response.body?.cancel();
          throw new Error();
        }
        if (stream) {
          await response.body?.cancel();
          return { status: response.status, body: "<stream>" };
        }
        const raw = await response.text();
        const safe = token === undefined
          ? raw
          : raw.split(token).join("[REDACTED]");
        let body: unknown = safe;
        try {
          body = JSON.parse(safe);
        } catch { /* non-JSON response */ }
        // JSON escapes can conceal the token in the raw wire text. Sanitize again
        // after parsing, including object keys, before any result reaches logs.
        if (token !== undefined) {
          body = JSON.parse(
            JSON.stringify(body).split(token).join("[REDACTED]"),
          );
        }
        return { status: response.status, body };
      } catch {
        // Never expose fetch/reader errors, Request objects, headers or reflected text.
        throw new Error(
          "OS API request failed (network, redirect, or origin policy)",
        );
      }
    },
  };
}
