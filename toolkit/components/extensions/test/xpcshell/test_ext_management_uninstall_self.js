/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { Prompter } = ChromeUtils.import("resource://gre/modules/Prompter.jsm");

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const id = "uninstall_self_test@tests.mozilla.com";

const manifest = {
  applications: {
    gecko: {
      id,
    },
  },
  name: "test extension name",
  version: "1.0",
};

const waitForUninstalled = () =>
  new Promise(resolve => {
    const listener = {
      onUninstalled: async addon => {
        equal(addon.id, id, "The expected add-on has been uninstalled");
        let checkedAddon = await AddonManager.getAddonByID(addon.id);
        equal(checkedAddon, null, "Add-on no longer exists");
        AddonManager.removeAddonListener(listener);
        resolve();
      },
    };
    AddonManager.addAddonListener(listener);
  });

AddonTestUtils.init(this);

let confirmExStub;

add_task(async function setup() {
  confirmExStub = sinon.stub(Prompter.prototype, "confirmEx");

  registerCleanupFunction(() => {
    sinon.restore();
  });
  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_management_uninstall_no_prompt() {
  function background() {
    browser.test.onMessage.addListener(msg => {
      browser.management.uninstallSelf();
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    useAddonManager: "temporary",
  });

  await extension.startup();
  let addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");
  extension.sendMessage("uninstall");
  await waitForUninstalled();
  Services.obs.notifyObservers(extension.extension.file, "flush-cache-entry");
});

add_task(async function test_management_uninstall_prompt_uninstall() {
  confirmExStub.reset();
  confirmExStub.returns(0);

  function background() {
    browser.test.onMessage.addListener(msg => {
      browser.management.uninstallSelf({ showConfirmDialog: true });
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    useAddonManager: "temporary",
  });

  await extension.startup();
  let addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");
  extension.sendMessage("uninstall");
  await waitForUninstalled();

  // Test localization strings
  ok(confirmExStub.calledOnce, "Should have only been called once");
  equal(confirmExStub.args[0][1], `Uninstall ${manifest.name}`);
  equal(
    confirmExStub.args[0][2],
    `The extension “${manifest.name}” is requesting to be uninstalled. What would you like to do?`
  );
  equal(confirmExStub.args[0][4], "Uninstall");
  equal(confirmExStub.args[0][5], "Keep Installed");
  Services.obs.notifyObservers(extension.extension.file, "flush-cache-entry");
});

add_task(async function test_management_uninstall_prompt_keep() {
  confirmExStub.reset();
  confirmExStub.returns(1);

  function background() {
    browser.test.onMessage.addListener(async msg => {
      await browser.test.assertRejects(
        browser.management.uninstallSelf({ showConfirmDialog: true }),
        "User cancelled uninstall of extension",
        "Expected rejection when user declines uninstall"
      );

      browser.test.sendMessage("uninstall-rejected");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    useAddonManager: "temporary",
  });

  await extension.startup();

  let addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");

  extension.sendMessage("uninstall");
  await extension.awaitMessage("uninstall-rejected");

  addon = await AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on remains installed");

  await extension.unload();
});
