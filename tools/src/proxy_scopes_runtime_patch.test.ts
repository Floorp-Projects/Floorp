// SPDX-License-Identifier: MPL-2.0
import { assert, assertEquals } from "@std/assert";
import { runInNewContext } from "node:vm";
import type {
  ProxyNativeConfiguration,
  ProxyNativeScopeConfiguration,
  ProxyTestAddon,
  ProxyTestChannel,
  ProxyTestExtension,
  ProxyVMContext,
} from "./proxy_scopes_test_types.ts";

const patchPath =
  ".github/patches/floorp-runtime/common/chrome-extension-proxy.patch";

async function harness() {
  const patch = await Deno.readTextFile(patchPath);
  const additions = patch.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  ).map((line) => line.slice(1)).join("\n");
  const conversion = additions.slice(
    additions.indexOf("const FLOORP_FIXED_PAC ="),
    additions.indexOf("function readGeckoProxySettings("),
  );
  const helper = additions.slice(
    additions.indexOf("const CHROME_PROXY_REGULAR_ONLY"),
    additions.indexOf("if (ChromeProxyScopes.available)"),
  );
  assert(conversion.length && helper.length);
  const settings = new Map<string, { id: string; value: unknown }>();
  const extensions = new Map<string, ProxyTestExtension>();
  const addons = new Map<string, ProxyTestAddon>();
  const locked = new Set<string>();
  let policyAllowed = true;
  let actual: ProxyNativeConfiguration = {
    proxyType: "none",
    autoConfigUrl: "",
  };
  let sequence = 0;
  let retained: string[] = [];
  const native = {
    registerChromeScopeProvider() {},
    retainChromeScopeCaches(keys: string[]) {
      retained = [...keys];
    },
  };
  const EPM = {
    addSetting() {},
    getLevelOfControl: () =>
      Promise.resolve(
        policyAllowed && !locked.size
          ? "controllable_by_this_extension"
          : "not_controllable",
      ),
    setSetting(id: string, name: string, value: unknown) {
      settings.set(name, { id, value });
      return Promise.resolve(true);
    },
    removeSetting(id: string, name: string) {
      if (settings.get(name)?.id === id) settings.delete(name);
      return Promise.resolve(true);
    },
  };
  // These tests exercise selection and ownership boundaries. Browser fixtures
  // separately verify the resulting native PAC and actual HTTP routes.
  const context: ProxyVMContext = {
    URL,
    ExtensionError: Error,
    AppConstants: { platform: "linux" },
    Ci: { nsIProtocolProxyService2: {}, nsIChromeProxyScopeProvider: {} },
    Cc: {
      "@mozilla.org/network/protocol-proxy-service;1": {
        getService: () => native,
      },
    },
    ChromeUtils: { generateQI: () => () => {} },
    Cu: {
      reportError: (error: Error) => {
        throw error;
      },
    },
    Services: {
      policies: { isAllowed: () => policyAllowed },
      prefs: { prefIsLocked: (name: string) => locked.has(name) },
      uuid: { generateUUID: () => "scope-" + ++sequence },
      wm: { getEnumerator: () => [{ private: true }] },
    },
    CHROME_PROXY_CONTROLLED_PREFS: ["network.proxy.type"],
    ExtensionPreferencesManager: EPM,
    ExtensionParent: {
      GlobalManager: { getExtension: (id: string) => extensions.get(id) },
    },
    ExtensionUtils: {
      getChromeExtensionId(extension?: ProxyTestExtension) {
        const marker = extension?.manifest?.browser_specific_settings?.gecko
          ?.floorp_chrome_extension;
        return marker?.schema_version === 1 &&
            /^[a-p]{32}$/.test(marker.original_id) &&
            extension?.id === marker.original_id + "@chromium-extension"
          ? marker.original_id
          : null;
      },
    },
    chromeProxyScopeModules: {
      AddonManager: {
        getAddonByID: (id: string) => Promise.resolve(addons.get(id)),
      },
      ExtensionSettingsStore: {
        initialize: () => Promise.resolve(),
        getSetting: (_type: string, name: string) => settings.get(name),
      },
      ExtensionData: class {
        id: string;
        constructor(root: string) {
          this.id = root;
        }
        readJSON() {
          return Promise.resolve(addons.get(this.id)?.manifest);
        }
      },
      ExtensionPermissions: {
        get: () =>
          Promise.resolve({ permissions: ["internal:privateBrowsingAllowed"] }),
      },
      PrivateBrowsingUtils: { isWindowPrivate: () => true },
    },
    readGeckoProxySettings: () => actual,
  };
  runInNewContext(
    conversion + helper + "\nthis.scopes=ChromeProxyScopes;",
    context,
  );
  assert(context.scopes, "The maintained patch creates its scoped engine");
  const scopes = context.scopes;
  function extension(original: string, date: number, marked = true) {
    const id = marked ? original + "@chromium-extension" : original;
    const manifest = marked
      ? {
        browser_specific_settings: {
          gecko: {
            floorp_chrome_extension: {
              schema_version: 1,
              original_id: original,
            },
          },
        },
      }
      : {};
    const value = {
      id,
      manifest,
      privateBrowsingAllowed: true,
      baseURL: "moz-extension://" + original + "/",
    };
    extensions.set(id, value);
    addons.set(id, {
      id,
      manifest,
      isActive: true,
      installDate: new Date(date),
      getResourceURI: () => id,
    });
    return value;
  }
  const nativeSet = (ext: ProxyTestExtension) =>
  (
    { value }: { value: ProxyNativeConfiguration },
  ) => {
    actual = value;
    settings.set("proxy.settings", { id: ext.id, value });
    return Promise.resolve();
  };
  const nativeClear = (ext: ProxyTestExtension) => () => {
    if (settings.get("proxy.settings")?.id === ext.id) {
      actual = { proxyType: "none", autoConfigUrl: "" };
      settings.delete("proxy.settings");
    }
    return Promise.resolve();
  };
  const config = (host: string) => ({
    mode: "pac_script",
    pacScript: {
      data: `function FindProxyForURL(){return 'PROXY ${host}:80';}`,
    },
  });
  return {
    scopes,
    settings,
    extensions,
    addons,
    extension,
    config,
    locked,
    nativeSet,
    nativeClear,
    retained: () => retained,
    setPolicy: (value: boolean) => policyAllowed = value,
  };
}

Deno.test("Scoped proxy selection isolates private overrides and preserves unchanged PAC keys", async () => {
  const h = await harness();
  const ext = h.extension("a".repeat(32), 100);
  const set = (value: unknown, scope: string) =>
    h.scopes.set(ext, { value, scope }, h.nativeSet(ext), h.nativeClear(ext));
  await set(h.config("regular.invalid"), "regular_only");
  assertEquals(h.scopes.state.private, null);
  assert(h.scopes.state.regular);
  const regularKey = h.scopes.configuration(h.scopes.state.regular, false).key;
  await set(h.config("private.invalid"), "incognito_persistent");
  assertEquals(h.scopes.state.regular.kind, "regularOnly");
  assert(h.scopes.state.private);
  assertEquals(h.scopes.state.private.kind, "private");
  assert(h.retained().includes(regularKey));
  assertEquals(
    h.scopes.configuration(h.scopes.state.regular, false).key,
    regularKey,
  );
  await set(h.config("session.invalid"), "incognito_session_only");
  assertEquals(h.scopes.state.private.kind, "session");
  await h.scopes.clear(
    ext,
    { scope: "incognito_session_only" },
    h.nativeClear(ext),
  );
  assertEquals(h.scopes.state.private.kind, "private");
  await h.scopes.clear(
    ext,
    { scope: "incognito_persistent" },
    h.nativeClear(ext),
  );
  assertEquals(h.scopes.state.private, null);
  assertEquals(
    h.scopes.configuration(h.scopes.state.regular, false).key,
    regularKey,
  );
});

Deno.test("Private speculative channel state takes precedence over a spanning extension's regular principal", async () => {
  const h = await harness();
  const ext = h.extension("e".repeat(32), 100);
  await h.scopes.set(
    ext,
    { value: h.config("regular.invalid"), scope: "regular_only" },
    h.nativeSet(ext),
    h.nativeClear(ext),
  );
  await h.scopes.set(
    ext,
    { value: h.config("private.invalid"), scope: "incognito_persistent" },
    h.nativeSet(ext),
    h.nativeClear(ext),
  );
  assert(h.scopes.state.regular);
  assert(h.scopes.state.private);
  const privateKey = h.scopes.configuration(h.scopes.state.private, true).key;
  const regularKey = h.scopes.configuration(h.scopes.state.regular, false).key;
  const observe = (channel: ProxyTestChannel) =>
    new Promise<ProxyNativeScopeConfiguration | null>((resolve) => {
      assert(h.scopes.provider.getConfiguration(channel, {
        onConfiguration: resolve,
      }));
    });
  const privateChannel = {
    loadInfo: { originAttributes: { privateBrowsingId: 0 } },
    QueryInterface: () => ({ isChannelPrivate: true }),
  };
  assertEquals((await observe(privateChannel))?.key, privateKey);
  assertEquals(
    (await observe({
      loadInfo: { originAttributes: { privateBrowsingId: 0 } },
      QueryInterface: () => ({ isChannelPrivate: false }),
    }))?.key,
    regularKey,
  );
  assertEquals(
    (await observe({
      loadInfo: { originAttributes: { privateBrowsingId: 1 } },
      QueryInterface: () => {
        throw new Error("The native private channel interface is unavailable");
      },
    }))?.key,
    privateKey,
  );
  assertEquals(
    (await observe({
      loadInfo: { originAttributes: { privateBrowsingId: 0 } },
      QueryInterface: () => {
        throw new Error("The native private channel interface is unavailable");
      },
    }))?.key,
    regularKey,
  );
});

Deno.test("Native Firefox control precedence and enterprise locks override Chrome scoped records", async () => {
  const h = await harness();
  const chrome = h.extension("b".repeat(32), 100);
  const firefox = h.extension("native-proxy@example.test", 200, false);
  await h.scopes.set(
    chrome,
    { value: h.config("private.invalid"), scope: "incognito_persistent" },
    h.nativeSet(chrome),
    h.nativeClear(chrome),
  );
  h.settings.set("proxy.settings", {
    id: firefox.id,
    value: { proxyType: "none" },
  });
  await h.scopes.refresh();
  assert(h.scopes.state.private);
  assertEquals(h.scopes.state.private.id, firefox.id);
  assertEquals(h.scopes.handles(h.scopes.state.private, true), false);
  h.settings.delete("proxy.settings");
  await h.scopes.refresh();
  assertEquals(h.scopes.state.private.id, chrome.id);
  h.locked.add("network.proxy.type");
  assertEquals(h.scopes.selected(true), null);
  h.locked.clear();
  h.setPolicy(false);
  assertEquals(h.scopes.selected(true), null);
});

Deno.test("Restored scoped settings authenticate the installed manifest before extension startup", async () => {
  const h = await harness();
  const ext = h.extension("c".repeat(32), 100);
  await h.scopes.set(
    ext,
    { value: h.config("private.invalid"), scope: "incognito_persistent" },
    h.nativeSet(ext),
    h.nativeClear(ext),
  );
  h.extensions.delete(ext.id);
  await h.scopes.refresh();
  assert(h.scopes.state.private);
  assertEquals(h.scopes.state.private.id, ext.id);
  h.scopes.startupManifests.clear();
  h.addons.get(ext.id)!.manifest = {};
  await h.scopes.refresh();
  assertEquals(h.scopes.state.private, null);
});

Deno.test("An extension can clear its own proxy settings after private access is revoked or policy locks settings", async () => {
  const h = await harness();
  const ext = h.extension("d".repeat(32), 100);
  await h.scopes.set(
    ext,
    { value: h.config("private.invalid"), scope: "incognito_persistent" },
    h.nativeSet(ext),
    h.nativeClear(ext),
  );
  ext.privateBrowsingAllowed = false;
  h.setPolicy(false);
  await h.scopes.clear(
    ext,
    { scope: "incognito_persistent" },
    h.nativeClear(ext),
  );
  assertEquals(
    h.settings.has("floorp.chrome.proxy.incognitoPersistent"),
    false,
  );
});
