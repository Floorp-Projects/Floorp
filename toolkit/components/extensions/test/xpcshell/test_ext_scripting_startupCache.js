/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

const { ExtensionScriptingStore } = ChromeUtils.import(
  "resource://gre/modules/ExtensionScriptingStore.jsm"
);

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_task(async function test_hasPersistedScripts_startup_cache() {
  let extension1 = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["scripting"],
    },
    // Set the startup reason to "APP_STARTUP", used to be able to simulate
    // the behavior expected on calls to `ExtensionScriptingStore.init(extension)`
    // when the addon has not been just installed, but it is being loaded as part
    // of the browser application starting up.
    startupReason: "APP_STARTUP",
    background() {
      browser.test.onMessage.addListener(async (msg, ...args) => {
        switch (msg) {
          case "registerContentScripts":
            await browser.scripting.registerContentScripts(...args);
            break;
          case "unregisterContentScripts":
            await browser.scripting.unregisterContentScripts(...args);
            break;
          default:
            browser.test.fail(`Unexpected test message: ${msg}`);
        }
        browser.test.sendMessage(`${msg}:done`);
      });
    },
    files: {
      "script-1.js": "",
    },
  });

  await extension1.startup();

  info(`Checking StartupCache for ${extension1.id} ${extension1.version}`);
  await assertHasPersistedScriptsCachedFlag(extension1);
  await assertIsPersistentScriptsCachedFlag(extension1, false);

  extension1.sendMessage("registerContentScripts", [
    {
      id: "some-script-id",
      js: ["script-1.js"],
      matches: ["http://*/*/file_sample.html"],
      persistAcrossSessions: true,
    },
  ]);
  await extension1.awaitMessage("registerContentScripts:done");

  await assertIsPersistentScriptsCachedFlag(extension1, true);

  extension1.sendMessage("unregisterContentScripts", {
    ids: ["some-script-id"],
  });
  await extension1.awaitMessage("unregisterContentScripts:done");

  await assertIsPersistentScriptsCachedFlag(extension1, false);

  const store = ExtensionScriptingStore._getStoreForTesting();
  const storeGetAllSpy = sinon.spy(store, "getAll");
  const cleanupSpies = () => {
    storeGetAllSpy.restore();
  };

  // NOTE: ExtensionScriptingStore.initExtension is usually only called once
  // during the extension startup.
  //
  // This test calls the method after startup was completed, which does not
  // happen in practice, but it allows us to simulate what happens under different
  // store and startup cache conditions and more explicitly cover the expectation
  // that store.getAll isn't going to be called more than once internally
  // when the hasPersistedScripts boolean flag wasn't in the StartupCache
  // and had to be recomputed.
  equal(
    extension1.extension.startupReason,
    "APP_STARTUP",
    "Got the expected extension.startupReason"
  );
  await ExtensionScriptingStore.initExtension(extension1.extension);
  equal(storeGetAllSpy.callCount, 0, "Expect store.getAll to not be called");

  Services.obs.notifyObservers(null, "startupcache-invalidate");

  await ExtensionScriptingStore.initExtension(extension1.extension);
  equal(storeGetAllSpy.callCount, 1, "Expect store.getAll to be called once");
  storeGetAllSpy.resetHistory();

  extension1.sendMessage("registerContentScripts", [
    {
      id: "some-script-id",
      js: ["script-1.js"],
      matches: ["http://*/*/file_sample.html"],
      persistAcrossSessions: true,
    },
  ]);
  await extension1.awaitMessage("registerContentScripts:done");
  await assertIsPersistentScriptsCachedFlag(extension1, true);

  // Make sure getAll is only called once when we don't have
  // scripting.hasPersistedScripts flag cached.
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  await ExtensionScriptingStore.initExtension(extension1.extension);
  equal(storeGetAllSpy.callCount, 1, "Expect store.getAll to be called once");

  cleanupSpies();

  const extId = extension1.id;
  const extVersion = extension1.version;
  await assertIsPersistentScriptsCachedFlag(
    { id: extId, version: extVersion },
    true
  );
  await extension1.unload();
  await assertIsPersistentScriptsCachedFlag(
    { id: extId, version: extVersion },
    undefined
  );

  const { StartupCache } = ExtensionParent;
  const allCachedGeneral = StartupCache._data.get("general");
  equal(
    allCachedGeneral.has(extId),
    false,
    "Expect the extension to have been removed from the StartupCache"
  );
});
