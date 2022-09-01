"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

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

const makeExtension = ({ manifest: manifestProps, ...otherProps }) => {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["scripting"],
      ...manifestProps,
    },
    useAddonManager: "permanent",
    ...otherProps,
  });
};

const assertNumScriptsInStore = async (extension, expectedNum) => {
  let scripts = await ExtensionScriptingStore._getStoreForTesting().getByExtensionId(
    extension.id
  );
  Assert.equal(
    scripts.length,
    expectedNum,
    `expected ${expectedNum} script in store`
  );
};

const verifyRegisterContentScripts = async manifestVersion => {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    manifest: {
      manifest_version: manifestVersion,
    },
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        const script = {
          id: "script",
          js: ["script.js"],
          matches: ["http://*/*/file_sample.html"],
          persistAcrossSessions: true,
        };

        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
        return;
      }

      browser.test.assertEq(1, scripts.length, "expected 1 registered script");
      browser.test.sendMessage("script-already-registered");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension, 1);

  await extension.awaitStartup();
  await extension.awaitMessage("script-already-registered");

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
};

add_task(async function test_registerContentScripts_mv2() {
  await verifyRegisterContentScripts(2);
});

add_task(async function test_registerContentScripts_mv3() {
  await verifyRegisterContentScripts(3);
});

const verifyUpdateContentScripts = async manifestVersion => {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    manifest: {
      manifest_version: manifestVersion,
    },
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        const script = {
          id: "script",
          js: ["script.js"],
          matches: ["http://*/*/file_sample.html"],
          persistAcrossSessions: true,
        };

        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
        return;
      }

      browser.test.assertEq(1, scripts.length, "expected 1 registered script");
      await browser.scripting.updateContentScripts([
        { id: scripts[0].id, persistAcrossSessions: false },
      ]);
      browser.test.sendMessage("script-updated");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  // Simulate a new session.
  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension, 1);

  await extension.awaitStartup();
  await extension.awaitMessage("script-updated");
  await assertNumScriptsInStore(extension, 0);

  // Simulate another new session.
  await AddonTestUtils.promiseRestartManager();

  await extension.awaitStartup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
};

add_task(async function test_updateContentScripts() {
  await verifyUpdateContentScripts(2);
});

add_task(async function test_updateContentScripts_mv3() {
  await verifyUpdateContentScripts(3);
});

const verifyUnregisterContentScripts = async manifestVersion => {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    manifest: {
      manifest_version: manifestVersion,
    },
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        const script = {
          id: "script",
          js: ["script.js"],
          matches: ["http://*/*/file_sample.html"],
          persistAcrossSessions: true,
        };

        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
        return;
      }

      browser.test.assertEq(1, scripts.length, "expected 1 registered script");
      await browser.scripting.unregisterContentScripts();
      browser.test.sendMessage("script-unregistered");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  // Simulate a new session.
  await AddonTestUtils.promiseRestartManager();

  // Script should be still persisted...
  await assertNumScriptsInStore(extension, 1);
  await extension.awaitStartup();
  // ...and we should now enter the second branch of the background script.
  await extension.awaitMessage("script-unregistered");
  await assertNumScriptsInStore(extension, 0);

  // Simulate another new session.
  await AddonTestUtils.promiseRestartManager();

  await extension.awaitStartup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
};

add_task(async function test_unregisterContentScripts() {
  await verifyUnregisterContentScripts(2);
});

add_task(async function test_unregisterContentScripts_mv3() {
  await verifyUnregisterContentScripts(3);
});

add_task(async function test_reload_extension() {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    async background() {
      browser.test.onMessage.addListener(msg => {
        browser.test.assertEq("reload-extension", msg, `expected msg: ${msg}`);
        browser.runtime.reload();
      });

      let scripts = await browser.scripting.getRegisteredContentScripts();

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        const script = {
          id: "script",
          js: ["script.js"],
          matches: ["http://*/*/file_sample.html"],
          persistAcrossSessions: true,
        };

        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
        return;
      }

      browser.test.assertEq(1, scripts.length, "expected 1 registered script");
      browser.test.sendMessage("script-already-registered");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  extension.sendMessage("reload-extension");
  // Wait for extension to restart, to make sure reloads works.
  await AddonTestUtils.promiseWebExtensionStartup(extension.id);
  await extension.awaitMessage("script-already-registered");
  await assertNumScriptsInStore(extension, 1);

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
});

add_task(async function test_disable_and_reenable_extension() {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        const script = {
          id: "script",
          js: ["script.js"],
          matches: ["http://*/*/file_sample.html"],
          persistAcrossSessions: true,
        };

        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
        return;
      }

      browser.test.assertEq(1, scripts.length, "expected 1 registered script");
      browser.test.sendMessage("script-already-registered");
    },
    files: {
      "script.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 1);

  // Disable...
  await extension.addon.disable();
  // then re-enable the extension.
  await extension.addon.enable();

  await extension.awaitMessage("script-already-registered");
  await assertNumScriptsInStore(extension, 1);

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
});

add_task(async function test_updateContentScripts_persistAcrossSessions_true() {
  await AddonTestUtils.promiseStartupManager();

  let extension = makeExtension({
    async background() {
      const script = {
        id: "script",
        js: ["script-1.js"],
        matches: ["http://*/*/file_sample.html"],
        persistAcrossSessions: false,
      };

      const scripts = await browser.scripting.getRegisteredContentScripts();

      browser.test.onMessage.addListener(async msg => {
        switch (msg) {
          case "persist-script":
            await browser.scripting.updateContentScripts([
              { id: script.id, persistAcrossSessions: true },
            ]);
            browser.test.sendMessage(`${msg}-done`);
            break;

          case "add-new-js":
            await browser.scripting.updateContentScripts([
              { id: script.id, js: ["script-1.js", "script-2.js"] },
            ]);
            browser.test.sendMessage(`${msg}-done`);
            break;

          case "verify-script":
            // We expect a single registered script, which is the one declared
            // above but at this point we should have 2 JS files and the
            // `persistAcrossSessions` option set to `true`.
            browser.test.assertEq(
              JSON.stringify([
                {
                  id: script.id,
                  allFrames: false,
                  matches: script.matches,
                  runAt: "document_idle",
                  persistAcrossSessions: true,
                  js: ["script-1.js", "script-2.js"],
                },
              ]),
              JSON.stringify(scripts),
              "expected scripts"
            );
            browser.test.sendMessage(`${msg}-done`);
            break;

          default:
            browser.test.fail(`unexpected message: ${msg}`);
        }
      });

      // Only register the content script if it wasn't registered before. Since
      // there is only one script, we don't check its ID.
      if (!scripts.length) {
        await browser.scripting.registerContentScripts([script]);
        browser.test.sendMessage("script-registered");
      } else {
        browser.test.sendMessage("script-already-registered");
      }
    },
    files: {
      "script-1.js": "",
      "script-2.js": "",
    },
  });

  await extension.startup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 0);

  // Simulate a new session.
  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension, 0);

  // We expect the script to be registered again because it isn't persisted.
  await extension.awaitStartup();
  await extension.awaitMessage("script-registered");
  await assertNumScriptsInStore(extension, 0);

  // We now tell the background script to update the script to persist it
  // across sessions.
  extension.sendMessage("persist-script");
  await extension.awaitMessage("persist-script-done");

  // Simulate another new session. We expect the content script to be already
  // registered since it was persisted in the previous (simulated) session.
  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension, 1);

  await extension.awaitStartup();
  await extension.awaitMessage("script-already-registered");
  await assertNumScriptsInStore(extension, 1);

  // We tell the background script to update the content script with a new JS
  // file and we don't change the `persistAcrossSessions` option.
  extension.sendMessage("add-new-js");
  await extension.awaitMessage("add-new-js-done");

  // Simulate another new session. We expect the content script to have 2 JS
  // files and to be registered since it was persisted in the previous
  // (simulated) session and we didn't update the option.
  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension, 1);

  await extension.awaitStartup();
  await extension.awaitMessage("script-already-registered");
  await assertNumScriptsInStore(extension, 1);

  // Let's verify that the script fetched by the background script is the one
  // we expect at this point: it should have two JS files.
  extension.sendMessage("verify-script");
  await extension.awaitMessage("verify-script-done");

  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension, 0);
});

add_task(async function test_multiple_extensions_and_scripts() {
  await AddonTestUtils.promiseStartupManager();

  let extension1 = makeExtension({
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      if (!scripts.length) {
        await browser.scripting.registerContentScripts([
          {
            id: "0",
            js: ["script-1.js"],
            matches: ["http://*/*/file_sample.html"],
            // We should persist this script by default.
          },
          {
            id: "/",
            js: ["script-2.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: true,
          },
          {
            id: "3",
            js: ["script-3.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          },
        ]);
        browser.test.sendMessage("scripts-registered");
        return;
      }

      browser.test.assertEq(2, scripts.length, "expected 2 registered scripts");
      browser.test.sendMessage("scripts-already-registered");
    },
    files: {
      "script-1.js": "",
      "script-2.js": "",
      "script-3.js": "",
    },
  });

  let extension2 = makeExtension({
    async background() {
      let scripts = await browser.scripting.getRegisteredContentScripts();

      if (!scripts.length) {
        await browser.scripting.registerContentScripts([
          {
            id: "1",
            js: ["script-1.js"],
            matches: ["http://*/*/file_sample.html"],
            // We should persist this script by default.
          },
          {
            id: "2",
            js: ["script-2.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          },
          {
            id: "\uFFFD 🍕 Boö",
            js: ["script-3.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: true,
          },
        ]);
        browser.test.sendMessage("scripts-registered");
        return;
      }

      browser.test.assertEq(2, scripts.length, "expected 2 registered scripts");
      browser.test.assertEq(
        JSON.stringify(["script-1.js"]),
        JSON.stringify(scripts[0].js),
        "expected a single 'script-1.js' js file"
      );
      browser.test.assertEq(
        "\uFFFD 🍕 Boö",
        scripts[1].id,
        "expected correct ID"
      );
      browser.test.sendMessage("scripts-already-registered");
    },
    files: {
      "script-1.js": "",
      "script-2.js": "",
      "script-3.js": "",
    },
  });

  await Promise.all([extension1.startup(), extension2.startup()]);

  await Promise.all([
    extension1.awaitMessage("scripts-registered"),
    extension2.awaitMessage("scripts-registered"),
  ]);
  await assertNumScriptsInStore(extension1, 2);
  await assertNumScriptsInStore(extension2, 2);

  await AddonTestUtils.promiseRestartManager();
  await assertNumScriptsInStore(extension1, 2);
  await assertNumScriptsInStore(extension2, 2);

  await Promise.all([extension1.awaitStartup(), extension2.awaitStartup()]);
  await Promise.all([
    extension1.awaitMessage("scripts-already-registered"),
    extension2.awaitMessage("scripts-already-registered"),
  ]);

  await Promise.all([extension1.unload(), extension2.unload()]);
  await AddonTestUtils.promiseShutdownManager();
  await assertNumScriptsInStore(extension1, 0);
  await assertNumScriptsInStore(extension2, 0);
});

add_task(async function test_persisted_scripts_cleared_on_addon_updates() {
  await AddonTestUtils.promiseStartupManager();

  function background() {
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
  }

  async function registerContentScript(ext, scriptFileName) {
    ext.sendMessage("registerContentScripts", [
      {
        id: scriptFileName,
        js: [scriptFileName],
        matches: ["http://*/*/file_sample.html"],
        persistAcrossSessions: true,
      },
    ]);
    await ext.awaitMessage("registerContentScripts:done");
  }

  let extension1Data = {
    manifest: {
      manifest_version: 2,
      permissions: ["scripting"],
      version: "1.0",
      browser_specific_settings: {
        // Set an explicit extension id so that extension.upgrade
        // will trigger the extension to be started with the expected
        // "ADDON_UPGRADE" / "ADDON_DOWNGRADE" extension.startupReason.
        gecko: { id: "extension1@mochi.test" },
      },
    },
    useAddonManager: "permanent",
    background,
    files: {
      "script-1.js": "",
    },
  };

  let extension1 = ExtensionTestUtils.loadExtension(extension1Data);

  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["scripting"],
      browser_specific_settings: {
        gecko: { id: "extension2@mochi.test" },
      },
    },
    useAddonManager: "permanent",
    background,
    files: {
      "script-2.js": "",
    },
  });

  await extension1.startup();
  await assertIsPersistentScriptsCachedFlag(extension1, false);
  await assertNumScriptsInStore(extension1, 0);

  await extension2.startup();
  await assertIsPersistentScriptsCachedFlag(extension2, false);
  await assertNumScriptsInStore(extension2, 0);

  await registerContentScript(extension1, "script-1.js");
  await assertIsPersistentScriptsCachedFlag(extension1, true);
  await assertNumScriptsInStore(extension1, 1);

  await registerContentScript(extension2, "script-2.js");
  await assertIsPersistentScriptsCachedFlag(extension2, true);
  await assertNumScriptsInStore(extension2, 1);

  info("Verify that scripts are still registered on a browser startup");
  await AddonTestUtils.promiseRestartManager();
  await extension1.awaitStartup();
  await extension2.awaitStartup();
  equal(
    extension1.extension.startupReason,
    "APP_STARTUP",
    "Got the expected startupReason on AOM restart"
  );

  await assertIsPersistentScriptsCachedFlag(extension1, true);
  await assertIsPersistentScriptsCachedFlag(extension2, true);
  await assertNumScriptsInStore(extension2, 1);
  await assertNumScriptsInStore(extension2, 1);

  async function testOnAddonUpdates(
    extensionUpdateData,
    expectedStartupReason
  ) {
    await extension1.upgrade(extensionUpdateData);
    equal(
      extension1.extension.startupReason,
      expectedStartupReason,
      "Got the expected startupReason on upgrade"
    );

    await assertNumScriptsInStore(extension1, 0);
    await assertIsPersistentScriptsCachedFlag(extension1, false);
    await assertNumScriptsInStore(extension2, 1);
    await assertIsPersistentScriptsCachedFlag(extension2, true);
  }

  info("Verify that scripts are cleared on upgrade");
  await testOnAddonUpdates(
    {
      ...extension1Data,
      manifest: {
        ...extension1Data.manifest,
        version: "2.0",
      },
    },
    "ADDON_UPGRADE"
  );

  await registerContentScript(extension1, "script-1.js");
  await assertNumScriptsInStore(extension1, 1);

  info("Verify that scripts are cleared on downgrade");
  await testOnAddonUpdates(extension1Data, "ADDON_DOWNGRADE");

  await registerContentScript(extension1, "script-1.js");
  await assertNumScriptsInStore(extension1, 1);

  info("Verify that scripts are cleared on upgrade to same version");
  await testOnAddonUpdates(extension1Data, "ADDON_UPGRADE");

  await extension1.unload();
  await extension2.unload();

  await assertNumScriptsInStore(extension1, 0);
  await assertIsPersistentScriptsCachedFlag(extension1, undefined);
  await assertNumScriptsInStore(extension2, 0);
  await assertIsPersistentScriptsCachedFlag(extension2, undefined);

  info("Verify stale persisted scripts cleared on re-install");
  // Inject a stale persisted script into the store.
  await ExtensionScriptingStore._getStoreForTesting().writeMany(extension1.id, [
    {
      id: "script-1.js",
      allFrames: false,
      matches: ["http://*/*/file_sample.html"],
      runAt: "document_idle",
      persistAcrossSessions: true,
      js: ["script-1.js"],
    },
  ]);
  await assertNumScriptsInStore(extension1, 1);
  const extension1Reinstalled = ExtensionTestUtils.loadExtension(
    extension1Data
  );
  await extension1Reinstalled.startup();
  equal(
    extension1Reinstalled.extension.startupReason,
    "ADDON_INSTALL",
    "Got the expected startupReason on re-install"
  );
  await assertNumScriptsInStore(extension1Reinstalled, 0);
  await assertIsPersistentScriptsCachedFlag(extension1Reinstalled, false);
  await extension1Reinstalled.unload();
  await assertNumScriptsInStore(extension1Reinstalled, 0);
  await assertIsPersistentScriptsCachedFlag(extension1Reinstalled, undefined);

  await AddonTestUtils.promiseShutdownManager();
});
