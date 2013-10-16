/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://services-sync/engines/addons.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

loadAddonTestFunctions();
startupManager();
Svc.Prefs.set("addons.ignoreRepositoryChecking", true);
Svc.Prefs.set("engine.addons", true);

Service.engineManager.register(AddonsEngine);
let engine     = Service.engineManager.get("addons");
let reconciler = engine._reconciler;
let store      = engine._store;
let tracker    = engine._tracker;

// Don't write out by default.
tracker.persistChangedIDs = false;

const addon1ID = "addon1@tests.mozilla.org";

function cleanup_and_advance() {
  Svc.Obs.notify("weave:engine:stop-tracking");
  tracker.observe(null, "weave:engine:stop-tracking");

  tracker.resetScore();
  tracker.clearChangedIDs();

  reconciler._addons = {};
  reconciler._changes = [];
  let cb = Async.makeSpinningCallback();
  reconciler.saveState(null, cb);
  cb.wait();

  run_next_test();
}

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Addons").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.AddonsReconciler").level =
    Log.Level.Trace;

  cleanup_and_advance();
}

add_test(function test_empty() {
  _("Verify the tracker is empty to start with.");

  do_check_eq(0, Object.keys(tracker.changedIDs).length);
  do_check_eq(0, tracker.score);

  cleanup_and_advance();
});

add_test(function test_not_tracking() {
  _("Ensures the tracker doesn't do anything when it isn't tracking.");

  let addon = installAddon("test_bootstrap1_1");
  uninstallAddon(addon);

  do_check_eq(0, Object.keys(tracker.changedIDs).length);
  do_check_eq(0, tracker.score);

  cleanup_and_advance();
});

add_test(function test_track_install() {
  _("Ensure that installing an add-on notifies tracker.");

  reconciler.startListening();

  Svc.Obs.notify("weave:engine:start-tracking");

  do_check_eq(0, tracker.score);
  let addon = installAddon("test_bootstrap1_1");
  let changed = tracker.changedIDs;

  do_check_eq(1, Object.keys(changed).length);
  do_check_true(addon.syncGUID in changed);
  do_check_eq(SCORE_INCREMENT_XLARGE, tracker.score);

  uninstallAddon(addon);
  cleanup_and_advance();
});

add_test(function test_track_uninstall() {
  _("Ensure that uninstalling an add-on notifies tracker.");

  reconciler.startListening();

  let addon = installAddon("test_bootstrap1_1");
  let guid = addon.syncGUID;
  do_check_eq(0, tracker.score);

  Svc.Obs.notify("weave:engine:start-tracking");

  uninstallAddon(addon);
  let changed = tracker.changedIDs;
  do_check_eq(1, Object.keys(changed).length);
  do_check_true(guid in changed);
  do_check_eq(SCORE_INCREMENT_XLARGE, tracker.score);

  cleanup_and_advance();
});

add_test(function test_track_user_disable() {
  _("Ensure that tracker sees disabling of add-on");

  reconciler.startListening();

  let addon = installAddon("test_bootstrap1_1");
  do_check_false(addon.userDisabled);
  do_check_false(addon.appDisabled);
  do_check_true(addon.isActive);

  Svc.Obs.notify("weave:engine:start-tracking");
  do_check_eq(0, tracker.score);

  let cb = Async.makeSyncCallback();

  let listener = {
    onDisabled: function(disabled) {
      _("onDisabled");
      if (disabled.id == addon.id) {
        AddonManager.removeAddonListener(listener);
        cb();
      }
    },
    onDisabling: function(disabling) {
      _("onDisabling add-on");
    }
  };
  AddonManager.addAddonListener(listener);

  _("Disabling add-on");
  addon.userDisabled = true;
  _("Disabling started...");
  Async.waitForSyncCallback(cb);

  let changed = tracker.changedIDs;
  do_check_eq(1, Object.keys(changed).length);
  do_check_true(addon.syncGUID in changed);
  do_check_eq(SCORE_INCREMENT_XLARGE, tracker.score);

  uninstallAddon(addon);
  cleanup_and_advance();
});

add_test(function test_track_enable() {
  _("Ensure that enabling a disabled add-on notifies tracker.");

  reconciler.startListening();

  let addon = installAddon("test_bootstrap1_1");
  addon.userDisabled = true;
  store._sleep(0);

  do_check_eq(0, tracker.score);

  Svc.Obs.notify("weave:engine:start-tracking");
  addon.userDisabled = false;
  store._sleep(0);

  let changed = tracker.changedIDs;
  do_check_eq(1, Object.keys(changed).length);
  do_check_true(addon.syncGUID in changed);
  do_check_eq(SCORE_INCREMENT_XLARGE, tracker.score);

  uninstallAddon(addon);
  cleanup_and_advance();
});
