/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("webextensions.storage.sync.kinto", true);

const { ExtensionStorageEngine } = ChromeUtils.import(
  "resource://services-sync/engines/extension-storage.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");
const { extensionStorageSyncKinto: extensionStorageSync } = ChromeUtils.import(
  "resource://gre/modules/ExtensionStorageSyncKinto.jsm"
);

let engine;

add_task(async function setup() {
  await Service.engineManager.register(ExtensionStorageEngine);
  engine = Service.engineManager.get("extension-storage");
  do_get_profile(); // so we can use FxAccounts
  loadWebExtensionTestFunctions();
});

add_task(async function test_changing_extension_storage_changes_score() {
  const tracker = engine._tracker;
  const extension = { id: "my-extension-id" };
  tracker.start();
  await withSyncContext(async function(context) {
    await extensionStorageSync.set(extension, { a: "b" }, context);
  });
  Assert.equal(tracker.score, SCORE_INCREMENT_MEDIUM);

  tracker.resetScore();
  await withSyncContext(async function(context) {
    await extensionStorageSync.remove(extension, "a", context);
  });
  Assert.equal(tracker.score, SCORE_INCREMENT_MEDIUM);

  await tracker.stop();
});
