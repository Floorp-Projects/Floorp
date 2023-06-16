/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

add_task(async function setup() {
  Services.prefs.setBoolPref(
    "extensions.webextOptionalPermissionPrompts",
    false
  );
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("extensions.webextOptionalPermissionPrompts");
  });
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_management_permission() {
  async function background() {
    const permObj = { permissions: ["management"] };

    let hasPerm = await browser.permissions.contains(permObj);
    browser.test.assertTrue(!hasPerm, "does not have management permission");
    browser.test.assertTrue(
      !!browser.management,
      "management namespace exists"
    );
    // These require permission
    let requires_permission = [
      "getAll",
      "get",
      "install",
      "setEnabled",
      "onDisabled",
      "onEnabled",
      "onInstalled",
      "onUninstalled",
    ];

    async function testAvailable() {
      // These are always available regardless of permission.
      for (let fn of ["getSelf", "uninstallSelf"]) {
        browser.test.assertTrue(
          !!browser.management[fn],
          `management.${fn} exists`
        );
      }

      let hasPerm = await browser.permissions.contains(permObj);
      for (let fn of requires_permission) {
        browser.test.assertEq(
          hasPerm,
          !!browser.management[fn],
          `management.${fn} does not exist`
        );
      }
    }

    await testAvailable();

    browser.test.onMessage.addListener(async msg => {
      browser.test.log("test with permission");

      // get permission
      await browser.permissions.request(permObj);
      let hasPerm = await browser.permissions.contains(permObj);
      browser.test.assertTrue(
        hasPerm,
        "management permission.request accepted"
      );
      await testAvailable();

      browser.management.onInstalled.addListener(() => {
        browser.test.fail("onInstalled listener invoked");
      });

      browser.test.log("test without permission");
      // remove permission
      await browser.permissions.remove(permObj);
      hasPerm = await browser.permissions.contains(permObj);
      browser.test.assertFalse(
        hasPerm,
        "management permission.request removed"
      );
      await testAvailable();

      browser.test.sendMessage("done");
    });

    browser.test.sendMessage("started");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: "management@test",
        },
      },
      optional_permissions: ["management"],
    },
    background,
    useAddonManager: "temporary",
  });

  await extension.startup();
  await extension.awaitMessage("started");
  await withHandlingUserInput(extension, async () => {
    extension.sendMessage("request");
  });
  await extension.awaitMessage("done");

  // Verify the onInstalled listener does not get used.
  // The listener will make the test fail if fired.
  let ext2 = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: "on-installed@test",
        },
      },
      optional_permissions: ["management"],
    },
    useAddonManager: "temporary",
  });
  await ext2.startup();
  await ext2.unload();

  await extension.unload();
});

add_task(async function test_management_getAll() {
  const id1 = "get_all_test1@tests.mozilla.com";
  const id2 = "get_all_test2@tests.mozilla.com";

  function getManifest(id) {
    return {
      browser_specific_settings: {
        gecko: {
          id,
        },
      },
      name: id,
      version: "1.0",
      short_name: id,
      permissions: ["management"],
    };
  }

  async function background() {
    browser.test.onMessage.addListener(async (msg, id) => {
      let addon = await browser.management.get(id);
      browser.test.sendMessage("addon", addon);
    });

    let addons = await browser.management.getAll();
    browser.test.assertEq(
      2,
      addons.length,
      "management.getAll returned correct number of add-ons."
    );
    browser.test.sendMessage("addons", addons);
  }

  let extension1 = ExtensionTestUtils.loadExtension({
    manifest: getManifest(id1),
    useAddonManager: "temporary",
  });

  let extension2 = ExtensionTestUtils.loadExtension({
    manifest: getManifest(id2),
    background,
    useAddonManager: "temporary",
  });

  await extension1.startup();
  await extension2.startup();

  let addons = await extension2.awaitMessage("addons");
  for (let id of [id1, id2]) {
    let addon = addons.find(a => {
      return a.id === id;
    });
    equal(
      addon.name,
      id,
      `The extension with id ${id} was returned by getAll.`
    );
    equal(addon.shortName, id, "Additional extension metadata was correct");
  }

  extension2.sendMessage("getAddon", id1);
  let addon = await extension2.awaitMessage("addon");
  equal(addon.name, id1, `The extension with id ${id1} was returned by get.`);
  equal(addon.shortName, id1, "Additional extension metadata was correct");

  extension2.sendMessage("getAddon", id2);
  addon = await extension2.awaitMessage("addon");
  equal(addon.name, id2, `The extension with id ${id2} was returned by get.`);
  equal(addon.shortName, id2, "Additional extension metadata was correct");

  await extension2.unload();
  await extension1.unload();
});

add_task(
  { pref_set: [["extensions.eventPages.enabled", true]] },
  async function test_management_event_page() {
    let extension = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        permissions: ["management"],
        background: { persistent: false },
      },
      background() {
        browser.management.onInstalled.addListener(details => {
          browser.test.sendMessage("onInstalled", details);
        });
        browser.management.onUninstalled.addListener(details => {
          browser.test.sendMessage("onUninstalled", details);
        });
        browser.management.onEnabled.addListener(() => {
          browser.test.sendMessage("onEnabled");
        });
        browser.management.onDisabled.addListener(() => {
          browser.test.sendMessage("onDisabled");
        });
      },
    });

    await extension.startup();
    let events = ["onInstalled", "onUninstalled", "onEnabled", "onDisabled"];
    for (let event of events) {
      assertPersistentListeners(extension, "management", event, {
        primed: false,
      });
    }

    await extension.terminateBackground();
    for (let event of events) {
      assertPersistentListeners(extension, "management", event, {
        primed: true,
      });
    }

    let testExt = ExtensionTestUtils.loadExtension({
      useAddonManager: "permanent",
      manifest: {
        browser_specific_settings: { gecko: { id: "test-ext@mochitest" } },
      },
      background() {},
    });
    await testExt.startup();

    let details = await extension.awaitMessage("onInstalled");
    equal(testExt.id, details.id, "got onInstalled event");

    await AddonTestUtils.promiseRestartManager();
    await extension.awaitStartup();
    await testExt.awaitStartup();

    for (let event of events) {
      assertPersistentListeners(extension, "management", event, {
        primed: true,
      });
    }

    // Test uninstalling an addon wakes up the watching extension.
    let uninstalled = testExt.unload();

    details = await extension.awaitMessage("onUninstalled");
    equal(testExt.id, details.id, "got onUninstalled event");

    await extension.unload();
    await uninstalled;
  }
);

// Sanity check that Addon listeners are removed on context close.
add_task(
  {
    // __AddonManagerInternal__ is exposed for debug builds only.
    skip_if: () => !AppConstants.DEBUG,
  },
  async function test_management_unregister_listener() {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        permissions: ["management"],
      },
      files: {
        "extpage.html": `<!DOCTYPE html><script src="extpage.js"></script>`,
        "extpage.js": function () {
          browser.management.onInstalled.addListener(() => {});
        },
      },
    });

    await extension.startup();

    const page = await ExtensionTestUtils.loadContentPage(
      `moz-extension://${extension.uuid}/extpage.html`
    );

    const { AddonManager } = ChromeUtils.importESModule(
      "resource://gre/modules/AddonManager.sys.mjs"
    );
    function assertManagementAPIAddonListener(expect) {
      let found = false;
      for (const addonListener of AddonManager.__AddonManagerInternal__
        ?.addonListeners || []) {
        if (
          Object.getPrototypeOf(addonListener).constructor.name ===
          "ManagementAddonListener"
        ) {
          found = true;
        }
      }
      equal(
        found,
        expect,
        `${
          expect ? "Should" : "Should not"
        } have found an AOM addonListener registered by the management API`
      );
    }

    assertManagementAPIAddonListener(true);
    await page.close();
    assertManagementAPIAddonListener(false);
    await extension.unload();
  }
);
