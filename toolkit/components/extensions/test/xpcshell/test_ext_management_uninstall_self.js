/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://testing-common/AddonTestUtils.jsm");
Cu.import("resource://testing-common/MockRegistrar.jsm");

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

const waitForUninstalled = () => new Promise(resolve => {
  const listener = {
    onUninstalled: (addon) => {
      equal(addon.id, id, "The expected add-on has been uninstalled");
      AddonManager.getAddonByID(addon.id, checkedAddon => {
        equal(checkedAddon, null, "Add-on no longer exists");
        AddonManager.removeAddonListener(listener);
        resolve();
      });
    },
  };
  AddonManager.addAddonListener(listener);
});

let promptService = {
  _response: null,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPromptService]),
  confirmEx: function(...args) {
    this._confirmExArgs = args;
    return this._response;
  },
};

add_task(function* setup() {
  let fakePromptService = MockRegistrar.register("@mozilla.org/embedcomp/prompt-service;1", promptService);
  do_register_cleanup(() => {
    MockRegistrar.unregister(fakePromptService);
  });
  yield ExtensionTestUtils.startAddonManager();
});

add_task(function* test_management_uninstall_no_prompt() {
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

  yield extension.startup();
  let addon = yield AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");
  extension.sendMessage("uninstall");
  yield waitForUninstalled();
  yield extension.markUnloaded();
  Services.obs.notifyObservers(extension.extension.file, "flush-cache-entry", null);
});

add_task(function* test_management_uninstall_prompt_uninstall() {
  promptService._response = 0;

  function background() {
    browser.test.onMessage.addListener(msg => {
      browser.management.uninstallSelf({showConfirmDialog: true});
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    useAddonManager: "temporary",
  });

  yield extension.startup();
  let addon = yield AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");
  extension.sendMessage("uninstall");
  yield waitForUninstalled();
  yield extension.markUnloaded();

  // Test localization strings
  equal(promptService._confirmExArgs[1], `Uninstall ${manifest.name}`);
  equal(promptService._confirmExArgs[2],
        `The extension “${manifest.name}” is requesting to be uninstalled. What would you like to do?`);
  equal(promptService._confirmExArgs[4], "Uninstall");
  equal(promptService._confirmExArgs[5], "Keep Installed");
  Services.obs.notifyObservers(extension.extension.file, "flush-cache-entry", null);
});

add_task(function* test_management_uninstall_prompt_keep() {
  promptService._response = 1;

  function background() {
    browser.test.onMessage.addListener(async msg => {
      await browser.test.assertRejects(
        browser.management.uninstallSelf({showConfirmDialog: true}),
        "User cancelled uninstall of extension",
        "Expected rejection when user declines uninstall");

      browser.test.sendMessage("uninstall-rejected");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest,
    background,
    useAddonManager: "temporary",
  });

  yield extension.startup();
  let addon = yield AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on is installed");
  extension.sendMessage("uninstall");
  yield extension.awaitMessage("uninstall-rejected");
  addon = yield AddonManager.getAddonByID(id);
  notEqual(addon, null, "Add-on remains installed");
  yield extension.unload();
  Services.obs.notifyObservers(extension.extension.file, "flush-cache-entry", null);
});
