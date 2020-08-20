/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(
  /Message manager disconnected/
);

add_task(async function test_management_themes() {
  const TEST_ID = "test_management_themes@tests.mozilla.com";

  let theme = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Simple theme test",
      version: "1.0",
      description: "test theme",
      theme: {
        images: {
          theme_frame: "image1.png",
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
    useAddonManager: "temporary",
  });

  async function background(TEST_ID) {
    browser.management.onInstalled.addListener(info => {
      if (info.name == TEST_ID) {
        return;
      }
      browser.test.log(`${info.name} was installed`);
      browser.test.assertEq(info.type, "theme", "addon is theme");
      browser.test.sendMessage("onInstalled", info.name);
    });
    browser.management.onDisabled.addListener(info => {
      browser.test.log(`${info.name} was disabled`);
      browser.test.assertEq(info.type, "theme", "addon is theme");
      browser.test.sendMessage("onDisabled", info.name);
    });
    browser.management.onEnabled.addListener(info => {
      browser.test.log(`${info.name} was enabled`);
      browser.test.assertEq(info.type, "theme", "addon is theme");
      browser.test.sendMessage("onEnabled", info.name);
    });
    browser.management.onUninstalled.addListener(info => {
      browser.test.log(`${info.name} was uninstalled`);
      browser.test.assertEq(info.type, "theme", "addon is theme");
      browser.test.sendMessage("onUninstalled", info.name);
    });

    async function getAddon(type) {
      let addons = await browser.management.getAll();
      let themes = addons.filter(addon => addon.type === "theme");
      // We get the 3 built-in themes plus the lwt and our addon.
      browser.test.assertEq(4, themes.length, "got expected addons");
      // We should also get our test extension.
      let testExtension = addons.find(addon => {
        return addon.id === TEST_ID;
      });
      browser.test.assertTrue(
        !!testExtension,
        `The extension with id ${TEST_ID} was returned by getAll.`
      );
      let found;
      for (let addon of themes) {
        browser.test.assertEq(addon.type, "theme", "addon is theme");
        if (type == "theme" && addon.id.includes("temporary-addon")) {
          found = addon;
        } else if (type == "enabled" && addon.enabled) {
          found = addon;
        }
      }
      return found;
    }

    browser.test.onMessage.addListener(async msg => {
      let theme = await getAddon("theme");
      browser.test.assertEq(
        theme.description,
        "test theme",
        "description is correct"
      );
      browser.test.assertTrue(theme.enabled, "theme is enabled");
      await browser.management.setEnabled(theme.id, false);

      theme = await getAddon("theme");

      browser.test.assertTrue(!theme.enabled, "theme is disabled");
      let addon = getAddon("enabled");
      browser.test.assertTrue(addon, "another theme was enabled");

      await browser.management.setEnabled(theme.id, true);
      theme = await getAddon("theme");
      addon = await getAddon("enabled");
      browser.test.assertEq(theme.id, addon.id, "theme is enabled");

      browser.test.sendMessage("done");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: TEST_ID,
        },
      },
      name: TEST_ID,
      permissions: ["management"],
    },
    background: `(${background})("${TEST_ID}")`,
    useAddonManager: "temporary",
  });
  await extension.startup();

  await theme.startup();
  is(
    await extension.awaitMessage("onInstalled"),
    "Simple theme test",
    "webextension theme installed"
  );
  is(await extension.awaitMessage("onDisabled"), "Default", "default disabled");

  extension.sendMessage("test");
  is(await extension.awaitMessage("onEnabled"), "Default", "default enabled");
  is(
    await extension.awaitMessage("onDisabled"),
    "Simple theme test",
    "addon disabled"
  );
  is(
    await extension.awaitMessage("onEnabled"),
    "Simple theme test",
    "addon enabled"
  );
  is(await extension.awaitMessage("onDisabled"), "Default", "default disabled");
  await extension.awaitMessage("done");

  await Promise.all([theme.unload(), extension.awaitMessage("onUninstalled")]);

  is(await extension.awaitMessage("onEnabled"), "Default", "default enabled");
  await extension.unload();
});
