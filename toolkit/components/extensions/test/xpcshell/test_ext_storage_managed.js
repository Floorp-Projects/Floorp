"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  MockRegistry: "resource://testing-common/MockRegistry.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

const MANIFEST = {
  name: "test-storage-managed@mozilla.com",
  description: "",
  type: "storage",
  data: {
    null: null,
    str: "hello",
    obj: {
      a: [2, 3],
      b: true,
    },
  },
};

add_task(async function setup() {
  await ExtensionTestUtils.startAddonManager();

  let tmpDir = FileUtils.getDir("TmpD", ["native-manifests"]);
  tmpDir.createUnique(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  let dirProvider = {
    getFile(property) {
      if (property.endsWith("NativeManifests")) {
        return tmpDir.clone();
      }
    },
  };
  Services.dirsvc.registerProvider(dirProvider);

  let typeSlug = AppConstants.platform === "linux" ? "managed-storage" : "ManagedStorage";
  OS.File.makeDir(OS.Path.join(tmpDir.path, typeSlug));

  let path = OS.Path.join(tmpDir.path, typeSlug, `${MANIFEST.name}.json`);
  await OS.File.writeAtomic(path, JSON.stringify(MANIFEST));

  let registry;
  if (AppConstants.platform === "win") {
    registry = new MockRegistry();
    registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                      `Software\\\Mozilla\\\ManagedStorage\\${MANIFEST.name}`,
                      "", path);
  }

  registerCleanupFunction(() => {
    Services.dirsvc.unregisterProvider(dirProvider);
    tmpDir.remove(true);
    if (registry) {
      registry.shutdown();
    }
  });
});

add_task(async function test_storage_managed() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: MANIFEST.name}},
      permissions: ["storage"],
    },

    async background() {
      await browser.test.assertRejects(
        browser.storage.managed.set({a: 1}),
        /storage.managed is read-only/,
        "browser.storage.managed.set() rejects because it's read only");

      await browser.test.assertRejects(
        browser.storage.managed.remove("str"),
        /storage.managed is read-only/,
        "browser.storage.managed.remove() rejects because it's read only");

      await browser.test.assertRejects(
        browser.storage.managed.clear(),
        /storage.managed is read-only/,
        "browser.storage.managed.clear() rejects because it's read only");

      browser.test.sendMessage("results", await Promise.all([
        browser.storage.managed.get(),
        browser.storage.managed.get("str"),
        browser.storage.managed.get(["null", "obj"]),
        browser.storage.managed.get({str: "a", num: 2}),
      ]));
    },
  });

  await extension.startup();
  deepEqual(await extension.awaitMessage("results"), [
    MANIFEST.data,
    {str: "hello"},
    {null: null, obj: MANIFEST.data.obj},
    {str: "hello", num: 2},
  ]);
  await extension.unload();
});

add_task(async function test_manifest_not_found() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["storage"],
    },

    async background() {
      await browser.test.assertRejects(
        browser.storage.managed.get({a: 1}),
        /Managed storage manifest not found/,
        "browser.storage.managed.get() rejects when without manifest");

      browser.test.notifyPass();
    },
  });

  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});
