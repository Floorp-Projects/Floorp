import { assert, assertEquals, assertRejects } from "@std/assert";
import { runInNewContext } from "node:vm";
import type {
  Addon,
  AddonListener,
  Consent,
  Deferred,
  Extension,
  Manifest,
} from "./chrome_user_scripts_consent_test_types.ts";

const patch = await Deno.readTextFile(
  new URL(
    "../../.github/patches/floorp-runtime/common/chrome-user-scripts-allow.patch",
    import.meta.url,
  ),
);
function addedSource(path: string): string {
  const section = patch.split(/^diff --git /m).find((part) =>
    part.startsWith(`a/${path} `)
  );
  assert(section, `Missing production patch section ${path}`);
  return section.split("\n").filter((line) =>
    line.startsWith("+") && !line.startsWith("+++")
  )
    .map((line) => line.slice(1)).join("\n");
}
const helper = addedSource(
  "toolkit/components/extensions/ChromeExtensionUserScriptsConsent.sys.mjs",
)
  .replace(/^import .*?;\n/gm, "").replaceAll("export const ", "const ");
const flag = "internal:floorpChromeUserScriptsAllowed";
function deferred(): Deferred {
  let resolve!: () => void;
  return {
    promise: new Promise<void>((done) => resolve = done),
    resolve: () => resolve(),
  };
}
function harness(optional = false) {
  const id = "a".repeat(32) + "@chromium-extension";
  const manifest: Manifest = {
    manifest_version: 3,
    [optional ? "optional_permissions" : "permissions"]: ["userScripts"],
    browser_specific_settings: {
      gecko: {
        floorp_chrome_extension: {
          schema_version: 1,
          original_id: "a".repeat(32),
        },
      },
    },
  };
  const extension: Extension = {
    id,
    manifest,
    permissions: new Set(optional ? [] : ["userScripts"]),
    hasPermission(name) {
      return this.permissions.has(name);
    },
  };
  const addon: Addon = {
    id,
    version: "1",
    type: "extension",
    getResourceURI: (path) => ({ spec: "jar:file:///installed.xpi!/" + path }),
  };
  const active = new Map([[id, extension]]);
  const addons = new Map([[id, addon]]);
  const stored = new Set<string>();
  const writes: { added: boolean; permissions: string[] }[] = [];
  let listener!: AddonListener;
  let lookupWait: Deferred | undefined;
  let writeWait: Deferred | undefined;
  const fifo = new Map<string, Promise<unknown>>();
  const native = {
    get: () => Promise.resolve({ permissions: [...stored] }),
    add: async (
      _id: string,
      perms: { permissions: string[] },
      emitter?: Extension,
    ) => {
      const wait = writeWait;
      writeWait = undefined;
      if (wait) await wait.promise;
      writes.push({ added: true, permissions: [...perms.permissions] });
      for (const name of perms.permissions) {
        stored.add(name);
        emitter?.permissions.add(name);
      }
    },
    remove: (
      _id: string,
      perms: { permissions: string[] },
      emitter?: Extension,
    ) => {
      writes.push({ added: false, permissions: [...perms.permissions] });
      for (const name of perms.permissions) {
        stored.delete(name);
        emitter?.permissions.delete(name);
      }
      return Promise.resolve();
    },
  };
  const value = runInNewContext(
    helper + "; ChromeExtensionUserScriptsConsent",
    {
      Error,
      TypeError,
      AddonManager: {
        addAddonListener: (value: AddonListener) => listener = value,
        getAddonByID: async (id: string) => {
          const wait = lookupWait;
          lookupWait = undefined;
          if (wait) await wait.promise;
          return addons.get(id);
        },
      },
      ExtensionPermissions: native,
      ExtensionTaskScheduler: class {
        runWriteTask(id: string, callback: () => unknown) {
          const job = (fifo.get(id) ?? Promise.resolve()).catch(() => {}).then(
            callback,
          );
          fifo.set(id, job);
          return job;
        }
      },
      ExtensionUtils: {
        getChromeExtensionId: (
          candidate: Pick<Extension, "id" | "manifest">,
        ) => {
          const marker = candidate.manifest.browser_specific_settings?.gecko
            .floorp_chrome_extension;
          return marker?.schema_version === 1 &&
              candidate.id === marker.original_id + "@chromium-extension"
            ? marker.original_id
            : null;
        },
      },
      WebExtensionPolicy: {
        getByID: (id: string) =>
          active.has(id) ? { extension: active.get(id) } : null,
      },
      NetUtil: {
        asyncFetch: (
          _options: unknown,
          callback: (stream: { available(): number }, result: number) => void,
        ) => callback({ available: () => JSON.stringify(manifest).length }, 0),
        readInputStreamToString: () => JSON.stringify(manifest),
      },
      Services: { scriptSecurityManager: { getSystemPrincipal: () => ({}) } },
      Ci: {
        nsILoadInfo: { SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL: 1 },
        nsIContentPolicy: { TYPE_OTHER: 1 },
      },
      Components: { isSuccessCode: (result: number) => result === 0 },
    },
  ) as Consent;
  return {
    consent: value,
    id,
    manifest,
    extension,
    addon,
    active,
    addons,
    stored,
    writes,
    replace() {
      listener.onUninstalling(addon);
      active.delete(id);
      addons.delete(id);
      stored.clear();
    },
    waitLookup(wait: Deferred) {
      lookupWait = wait;
    },
    waitWrite(wait: Deferred) {
      writeWait = wait;
    },
  };
}
Deno.test("required declaration remains granted with independent setting default off", async () => {
  const h = harness();
  assertEquals({ ...await h.consent.getStatus(h.id) }, {
    eligible: true,
    allowed: false,
    permissionGranted: true,
  });
  assertEquals(h.consent.isAllowed(h.extension), false);
  await h.consent.setAllowed(h.id, true);
  assert(h.consent.isAllowed(h.extension));
  await h.consent.setAllowed(h.id, false);
  assert(h.extension.permissions.has("userScripts"));
  assertEquals(h.writes, [{ added: true, permissions: [flag] }, {
    added: false,
    permissions: [flag],
  }]);
});
Deno.test("optional-only Allow changes only internal setting and never grants public permission", async () => {
  const h = harness(true);
  const status = await h.consent.setAllowed(h.id, true);
  assertEquals({ ...status }, {
    eligible: true,
    allowed: true,
    permissionGranted: false,
  });
  assertEquals(h.consent.isAllowed(h.extension), false);
  assertEquals([...h.extension.permissions], [flag]);
});
Deno.test("ordinary, mismatched metadata, MV2 and undeclared extensions cannot change setting", async () => {
  for (const mode of ["ordinary", "mismatch", "mv2", "undeclared"]) {
    const h = harness();
    if (mode === "ordinary") delete h.manifest.browser_specific_settings;
    if (mode === "mismatch") {
      h.manifest.browser_specific_settings!.gecko.floorp_chrome_extension
        .original_id = "b".repeat(32);
    }
    if (mode === "mv2") h.manifest.manifest_version = 2;
    if (mode === "undeclared") h.manifest.permissions = [];
    assertEquals(h.consent.isEligible(h.extension), false);
    await assertRejects(
      () => h.consent.setAllowed(h.id, true),
      Error,
      "no Chrome user script setting",
    );
    assertEquals(h.writes.length, 0);
  }
});
Deno.test("disabled addon manifest and permission store retain independent setting", async () => {
  const h = harness();
  h.active.clear();
  await h.consent.setAllowed(h.id, true);
  assertEquals({ ...await h.consent.getStatus(h.id) }, {
    eligible: true,
    allowed: true,
    permissionGranted: true,
  });
  assertEquals(h.extension.permissions.has(flag), false);
});
Deno.test("rapid setting changes execute FIFO instead of racing permission writes", async () => {
  const h = harness(), gate = deferred();
  h.waitWrite(gate);
  const first = h.consent.setAllowed(h.id, true),
    second = h.consent.setAllowed(h.id, false);
  await Promise.resolve();
  await Promise.resolve();
  gate.resolve();
  await Promise.all([first, second]);
  assertEquals(h.writes.map((write) => write.added), [true, false]);
  assertEquals(h.stored.has(flag), false);
});
Deno.test("uninstall during addon lookup prevents a stale setting write", async () => {
  const h = harness(), gate = deferred();
  h.waitLookup(gate);
  const change = h.consent.setAllowed(h.id, true);
  await Promise.resolve();
  await Promise.resolve();
  h.replace();
  gate.resolve();
  await assertRejects(() => change, Error, "extension changed");
  assertEquals(h.writes.length, 0);
});
Deno.test("a queued toggle from an earlier extension generation never changes its replacement", async () => {
  const h = harness(), gate = deferred();
  h.waitWrite(gate);
  const first = h.consent.setAllowed(h.id, true);
  await Promise.resolve();
  await Promise.resolve();
  await Promise.resolve();
  const queued = h.consent.setAllowed(h.id, false);
  const outcomes = Promise.allSettled([first, queued]);
  h.replace();
  h.addons.set(h.id, { ...h.addon, version: "2" });
  h.active.set(h.id, { ...h.extension, permissions: new Set() });
  gate.resolve();
  const results = await outcomes;
  for (const result of results) {
    assertEquals(result.status, "rejected");
    if (result.status === "rejected") {
      assert(String(result.reason).includes("extension changed"));
    }
  }
  assertEquals(h.writes.filter((write) => !write.added).length, 0);
});
const parentAdditions = addedSource(
  "toolkit/components/extensions/parent/ext-userScripts.js",
);
const queuedSource =
  parentAdditions.slice(parentAdditions.indexOf("    const chromeControlled"))
    .split("\n    };")[0] + "\n    };";
Deno.test("parent entry rejects a captured queued call if allow is revoked before its turn", async () => {
  const h = harness();
  h.extension.permissions.add(flag);
  const queue: (() => unknown)[] = [];
  const extension = {
    ...h.extension,
    userScriptsManager: {
      runReadTask: (callback: () => unknown) =>
        new Promise((resolve, reject) =>
          queue.push(() => {
            try {
              resolve(callback());
            } catch (error) {
              reject(error);
            }
          })
        ),
    },
  };
  const run = runInNewContext(
    queuedSource + '; (callback)=>runTask("getScripts",false,callback)',
    {
      extension,
      usm: extension.userScriptsManager,
      context: { unloaded: false },
      ExtensionUtils: { getChromeExtensionId: () => h.id },
      ChromeExtensionUserScriptsConsent: h.consent,
      WebExtensionPolicy: { getByID: () => ({ extension }) },
      ExtensionError: Error,
    },
  ) as (callback: () => unknown) => Promise<unknown>;
  let called = false;
  const pending = run(() => called = true);
  h.extension.permissions.delete(flag);
  queue[0]();
  await assertRejects(() => pending, Error, "not available in this context");
  assertEquals(called, false);
});
