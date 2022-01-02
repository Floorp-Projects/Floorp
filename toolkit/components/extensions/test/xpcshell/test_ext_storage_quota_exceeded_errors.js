/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "ExtensionStorageIDB",
  "resource://gre/modules/ExtensionStorageIDB.jsm"
);

const LEAVE_STORAGE_PREF = "extensions.webextensions.keepStorageOnUninstall";
const LEAVE_UUID_PREF = "extensions.webextensions.keepUuidOnUninstall";

AddonTestUtils.init(this);

add_task(async function setup() {
  // Ensure that the IDB backend is enabled.
  Services.prefs.setBoolPref("ExtensionStorageIDB.BACKEND_ENABLED_PREF", true);

  Services.prefs.setBoolPref("dom.quotaManager.testing", true);
  Services.prefs.setIntPref(
    "dom.quotaManager.temporaryStorage.fixedLimit",
    100
  );
  await promiseQuotaManagerServiceReset();

  await ExtensionTestUtils.startAddonManager();
});

add_task(async function test_storage_local_set_quota_exceeded_error() {
  const EXT_ID = "test-quota-exceeded@mochi.test";

  const extensionDef = {
    manifest: {
      permissions: ["storage"],
      applications: { gecko: { id: EXT_ID } },
    },
    async background() {
      const data = new Array(1000 * 1024).fill("x").join("");
      await browser.test.assertRejects(
        browser.storage.local.set({ data }),
        /QuotaExceededError/,
        "Got a rejection with the expected error message"
      );
      browser.test.sendMessage("data-stored");
    },
  };

  Services.prefs.setBoolPref(LEAVE_STORAGE_PREF, true);
  Services.prefs.setBoolPref(LEAVE_UUID_PREF, true);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(LEAVE_STORAGE_PREF);
    Services.prefs.clearUserPref(LEAVE_UUID_PREF);
  });

  const extension = ExtensionTestUtils.loadExtension(extensionDef);

  // Run test on a test extension being migrated to the IDB backend.
  await extension.startup();
  await extension.awaitMessage("data-stored");

  ok(
    ExtensionStorageIDB.isMigratedExtension(extension),
    "The extension has been successfully migrated to the IDB backend"
  );
  await extension.unload();

  // Run again on a test extension already already migrated to the IDB backend.
  const extensionUpdated = ExtensionTestUtils.loadExtension(extensionDef);
  await extensionUpdated.startup();
  ok(
    ExtensionStorageIDB.isMigratedExtension(extension),
    "The extension has been successfully migrated to the IDB backend"
  );
  await extensionUpdated.awaitMessage("data-stored");

  await extensionUpdated.unload();

  Services.prefs.clearUserPref("dom.quotaManager.temporaryStorage.fixedLimit");
  await promiseQuotaManagerServiceClear();
});
