// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals, assertThrows } from "@std/assert";
import { runInNewContext } from "node:vm";

const sourcePatch =
  ".github/patches/floorp-runtime/common/chrome-extension-proxy.patch";
const artifactPatch = "tools/patches/chrome-extension-proxy.patch";

type NativeConfig = { proxyType: string; autoConfigUrl: string };
type Adapter = {
  toGecko(value: unknown): NativeConfig;
  fromGecko(value: NativeConfig): unknown;
  route(config: unknown, url: string): string;
  prefs: Record<string, string | number>;
};

async function adapter(): Promise<Adapter> {
  const patch = await Deno.readTextFile(sourcePatch);
  const additions = patch.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
  const start = additions.indexOf("const FLOORP_FIXED_PAC =");
  const end = additions.indexOf("function readGeckoProxySettings(");
  assert(start >= 0 && end > start, "Proxy adapter is present in source patch");
  const source = additions.slice(start, end);
  const prefs: Record<string, string | number> = {};
  const scope = {
    URL,
    Services: {
      prefs: {
        getBoolPref: () => true,
        getCharPref: (name: string) => String(prefs[name] ?? ""),
        getIntPref: (name: string) => Number(prefs[name] ?? 0),
      },
    },
    ExtensionError: Error,
    toGecko: undefined as unknown as Adapter["toGecko"],
    fromGecko: undefined as unknown as Adapter["fromGecko"],
  };
  runInNewContext(
    source +
      "\nthis.toGecko = value => chromeProxyToGecko(value, {baseURL:'moz-extension://fixture/'});" +
      "\nthis.fromGecko = geckoProxyToChrome;",
    scope,
  );
  return {
    prefs,
    toGecko: scope.toGecko,
    fromGecko: (config) => JSON.parse(JSON.stringify(scope.fromGecko(config))),
    route(config, url) {
      const native = scope.toGecko(config);
      const source = decodeURIComponent(
        native.autoConfigUrl.split(",").slice(1).join(","),
      );
      const context = {
        shExpMatch(host: string, pattern: string) {
          const escaped = pattern.replace(/[.+?^${}()|[\]\\]/g, "\\$&");
          return new RegExp("^" + escaped.replaceAll("*", ".*") + "$").test(
            host,
          );
        },
        FindProxyForURL: undefined as unknown as (
          url: string,
          host: string,
        ) => string,
      };
      runInNewContext(source, context);
      return context.FindProxyForURL(url, new URL(url).hostname);
    },
  };
}

Deno.test("Chrome fixed proxy routes protocol and fallback endpoints through PAC", async () => {
  const a = await adapter();
  const config = {
    mode: "fixed_servers",
    rules: {
      proxyForHttp: { host: "http-proxy.test" },
      proxyForHttps: { scheme: "https", host: "secure-proxy.test" },
      fallbackProxy: { scheme: "socks5", host: "socks-proxy.test" },
    },
  };
  assertEquals(
    a.route(config, "http://example.com/"),
    "PROXY http-proxy.test:80",
  );
  assertEquals(
    a.route(config, "https://example.com/"),
    "HTTPS secure-proxy.test:443",
  );
  assertEquals(
    a.route(config, "ws://example.com/"),
    "SOCKS5 socks-proxy.test:1080",
  );
  assertEquals(
    a.route(config, "wss://example.com/"),
    "SOCKS5 socks-proxy.test:1080",
  );
  const partial = {
    mode: "fixed_servers",
    rules: { proxyForHttp: { host: "proxy.test" } },
  };
  assertEquals(a.route(partial, "https://example.com/"), "DIRECT");
  assertEquals(a.route(partial, "ws://example.com/"), "PROXY proxy.test:80");
});

Deno.test("Chrome manual proxy bypass preserves host patterns and implicit local bypass", async () => {
  const a = await adapter();
  const config = {
    mode: "fixed_servers",
    rules: {
      singleProxy: { scheme: "socks4", host: "::1", port: 1081 },
      bypassList: ["exact.example", "*.private.example", "<local>"],
    },
  };
  for (
    const host of [
      "exact.example",
      "service.private.example",
      "intranet",
      "localhost",
      "test.localhost",
      "127.0.0.2",
      "[::1]",
      "169.254.0.2",
      "[fe80::1]",
    ]
  ) {
    assertEquals(a.route(config, `http://${host}/`), "DIRECT", host);
  }
  assertEquals(a.route(config, "http://public.example/"), "SOCKS4 [::1]:1081");
  for (
    const host of [
      "127.example.test",
      "169.254.example.test",
      "127.0.0.1.example.test",
    ]
  ) {
    assertEquals(a.route(config, `http://${host}/`), "SOCKS4 [::1]:1081", host);
  }
});

Deno.test("Chrome proxy manual readback omits native disabled ports and refuses local socket proxies", async () => {
  const a = await adapter();
  Object.assign(a.prefs, {
    "network.proxy.http": "proxy.example",
    "network.proxy.http_port": 0,
    "network.proxy.ssl": "secure.example",
    "network.proxy.ssl_port": 0,
    "network.proxy.socks": "socks.example",
    "network.proxy.socks_port": 0,
  });
  const native = { proxyType: "manual", autoConfigUrl: "" };
  assertEquals(a.fromGecko(native), { mode: "fixed_servers", rules: {} });
  a.prefs["network.proxy.http_port"] = 80;
  assertEquals(a.fromGecko(native), {
    mode: "fixed_servers",
    rules: {
      proxyForHttp: { scheme: "http", host: "proxy.example", port: 80 },
    },
  });
  for (const host of ["file:///tmp/proxy.sock", "\\\\.\\pipe\\proxy"]) {
    a.prefs["network.proxy.socks"] = host;
    assertThrows(() => a.fromGecko(native), Error, "cannot be represented");
  }
});

Deno.test("Chrome proxy rejects unsupported or ambiguous configuration without fallback", async () => {
  const a = await adapter();
  for (
    const config of [
      { mode: "manual", http: "proxy.test" },
      { mode: "direct", rules: {} },
      { mode: "fixed_servers", rules: {} },
      {
        mode: "fixed_servers",
        rules: { singleProxy: { host: "proxy.test", port: 0 } },
      },
      {
        mode: "fixed_servers",
        rules: { singleProxy: { host: "proxy.test", scheme: "quic" } },
      },
      {
        mode: "fixed_servers",
        rules: {
          singleProxy: { host: "proxy.test" },
          bypassList: ["10.0.0.0/33"],
        },
      },
      {
        mode: "fixed_servers",
        rules: {
          singleProxy: { host: "proxy.test" },
          bypassList: ["https://example.com:65536"],
        },
      },
      {
        mode: "pac_script",
        pacScript: { url: "http://example.com/", data: "x" },
      },
      { mode: "pac_script", pacScript: { data: "x", mandatory: true } },
      {
        mode: "pac_script",
        pacScript: { url: "moz-extension://other/private.pac" },
      },
    ]
  ) {
    assertThrows(() => a.toGecko(config), Error);
  }
});

Deno.test("Chrome proxy get decodes only exact generated PAC and never executes arbitrary scripts", async () => {
  const a = await adapter();
  const value = {
    mode: "fixed_servers",
    rules: { singleProxy: { host: "proxy.test", scheme: "http", port: 80 } },
  };
  const native = a.toGecko(value);
  assertEquals(a.fromGecko(native), value);
  const tampered = {
    ...native,
    autoConfigUrl: native.autoConfigUrl +
      encodeURIComponent("throw new Error('must not execute');"),
  };
  assertEquals(a.fromGecko(tampered), {
    mode: "pac_script",
    pacScript: { url: tampered.autoConfigUrl, mandatory: false },
  });
  const data = "function FindProxyForURL(){return 'DIRECT';}";
  assertEquals(
    a.fromGecko(a.toGecko({ mode: "pac_script", pacScript: { data } })),
    { mode: "pac_script", pacScript: { data, mandatory: false } },
  );
});

Deno.test("Chrome proxy development patch carries the same API code as source patch", async () => {
  const source = (await Deno.readTextFile(sourcePatch)).split("diff --git ")
    .find((part) =>
      part.startsWith("a/toolkit/components/extensions/parent/ext-proxy.js ")
    );
  assert(source);
  const mapped = "diff --git " + source.replaceAll(
    "toolkit/components/extensions/parent/ext-proxy.js",
    "chrome/toolkit/content/extensions/parent/ext-proxy.js",
  );
  assertEquals(await Deno.readTextFile(artifactPatch), mapped);
});

Deno.test("Chrome proxy bypass handles literal CIDR, IPv6, schemes, ports and loopback subtraction", async () => {
  const a = await adapter();
  const config = (bypassList: string[]) => ({mode: "fixed_servers", rules: {singleProxy: {host: "proxy.example"}, bypassList}});
  assertEquals(a.route(config(["10.0.0.0/8"]), "http://10.42.5.6/"), "DIRECT");
  assertEquals(a.route(config(["10.0.0.0/8"]), "http://10.example.test/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["[2001:db8::]/32"]), "https://[2001:db8:abcd::42]/"), "DIRECT");
  assertEquals(a.route(config(["[2001:db8::]/32"]), "https://[2001:db9::42]/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["https://[2001:db8::42]:443"]), "https://[2001:db8::42]/"), "DIRECT");
  assertEquals(a.route(config(["https://example.test:443"]), "https://example.test/"), "DIRECT");
  assertEquals(a.route(config(["https://example.test:443"]), "http://example.test/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["https://example.test:443"]), "https://example.test:444/"), "PROXY proxy.example:80");
  assertEquals(a.route(config([".example.test"]), "http://child.example.test/"), "DIRECT");
  assertEquals(a.route(config([".example.test"]), "http://example.test/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["<-loopback>"]), "http://127.0.0.1/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["<-loopback>", "<local>"]), "http://127.0.0.1/"), "PROXY proxy.example:80");
  assertEquals(a.route(config(["<-loopback>", "<local>"]), "http://intranet/"), "DIRECT");
  for (const pattern of ["[2001:gg::]/32", "10.0.0.0/33", "https://x.test:65536", "2001:db8::42", "https://x.test/path"]) assertThrows(() => a.toGecko(config([pattern])));
});
