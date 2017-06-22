/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");
/* globals extensionStorageSync */

Service.engineManager.register(ExtensionStorageEngine);
const engine = Service.engineManager.get("extension-storage");
do_get_profile();   // so we can use FxAccounts
loadWebExtensionTestFunctions();

add_task(async function test_changing_extension_storage_changes_score() {
  const tracker = engine._tracker;
  const extension = {id: "my-extension-id"};
  Svc.Obs.notify("weave:engine:start-tracking");
  await withSyncContext(async function(context) {
    await extensionStorageSync.set(extension, {"a": "b"}, context);
  });
  do_check_eq(tracker.score, SCORE_INCREMENT_MEDIUM);

  tracker.resetScore();
  await withSyncContext(async function(context) {
    await extensionStorageSync.remove(extension, "a", context);
  });
  do_check_eq(tracker.score, SCORE_INCREMENT_MEDIUM);

  Svc.Obs.notify("weave:engine:stop-tracking");
});

function run_test() {
  run_next_test();
}
