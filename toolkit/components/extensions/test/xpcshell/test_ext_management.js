/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();

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
      applications: {
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
      applications: {
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
      applications: {
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
