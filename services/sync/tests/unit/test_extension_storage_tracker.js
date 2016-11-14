/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/extension-storage.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://gre/modules/ExtensionStorageSync.jsm");

Service.engineManager.register(ExtensionStorageEngine);
const engine = Service.engineManager.get("extension-storage");
do_get_profile();   // so we can use FxAccounts
loadWebExtensionTestFunctions();

add_task(function* test_changing_extension_storage_changes_score() {
  const tracker = engine._tracker;
  const extension = {id: "my-extension-id"};
  Svc.Obs.notify("weave:engine:start-tracking");
  yield* withSyncContext(function*(context) {
    yield ExtensionStorageSync.set(extension, {"a": "b"}, context);
  });
  do_check_eq(tracker.score, SCORE_INCREMENT_MEDIUM);

  tracker.resetScore();
  yield* withSyncContext(function*(context) {
    yield ExtensionStorageSync.remove(extension, "a", context);
  });
  do_check_eq(tracker.score, SCORE_INCREMENT_MEDIUM);

  Svc.Obs.notify("weave:engine:stop-tracking");
});

function run_test() {
  run_next_test();
}
