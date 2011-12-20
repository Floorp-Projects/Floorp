/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/addonsreconciler.js");
Cu.import("resource://services-sync/async.js");
Cu.import("resource://services-sync/engines/addons.js");

loadAddonTestFunctions();
startupManager();

Engines.register(AddonsEngine);
let engine = Engines.get("addons");
let reconciler = engine._reconciler;
let tracker = engine._tracker;

function advance_test() {
  reconciler._addons = {};
  reconciler._changes = [];

  let cb = Async.makeSpinningCallback();
  reconciler.saveState(null, cb);
  cb.wait();

  Svc.Prefs.reset("addons.ignoreRepositoryChecking");

  run_next_test();
}

// This is a basic sanity test for the unit test itself. If this breaks, the
// add-ons API likely changed upstream.
add_test(function test_addon_install() {
  _("Ensure basic add-on APIs work as expected.");

  let install = getAddonInstall("test_bootstrap1_1");
  do_check_neq(install, null);
  do_check_eq(install.type, "extension");
  do_check_eq(install.name, "Test Bootstrap 1");

  advance_test();
});

add_test(function test_find_dupe() {
  _("Ensure the _findDupe() implementation is sane.");

  // This gets invoked at the top of sync, which is bypassed by this
  // test, so we do it manually.
  engine._refreshReconcilerState();

  let addon = installAddon("test_bootstrap1_1");

  let record = {
    id:            Utils.makeGUID(),
    addonID:       addon.id,
    enabled:       true,
    applicationID: Services.appinfo.ID,
    source:        "amo"
  };

  let dupe = engine._findDupe(record);
  do_check_eq(addon.syncGUID, dupe);

  record.id = addon.syncGUID;
  dupe = engine._findDupe(record);
  do_check_eq(null, dupe);

  uninstallAddon(addon);
  advance_test();
});

add_test(function test_get_changed_ids() {
  _("Ensure getChangedIDs() has the appropriate behavior.");

  _("Ensure getChangedIDs() returns an empty object by default.");
  let changes = engine.getChangedIDs();
  do_check_eq("object", typeof(changes));
  do_check_eq(0, Object.keys(changes).length);

  _("Ensure tracker changes are populated.");
  let now = new Date();
  let changeTime = now.getTime() / 1000;
  let guid1 = Utils.makeGUID();
  tracker.addChangedID(guid1, changeTime);

  changes = engine.getChangedIDs();
  do_check_eq("object", typeof(changes));
  do_check_eq(1, Object.keys(changes).length);
  do_check_true(guid1 in changes);
  do_check_eq(changeTime, changes[guid1]);

  tracker.clearChangedIDs();

  _("Ensure reconciler changes are populated.");
  Svc.Prefs.set("addons.ignoreRepositoryChecking", true);
  let addon = installAddon("test_bootstrap1_1");
  tracker.clearChangedIDs(); // Just in case.
  changes = engine.getChangedIDs();
  do_check_eq("object", typeof(changes));
  do_check_eq(1, Object.keys(changes).length);
  do_check_true(addon.syncGUID in changes);
  do_check_true(changes[addon.syncGUID] > changeTime);

  let oldTime = changes[addon.syncGUID];
  let guid2 = addon.syncGUID;
  uninstallAddon(addon);
  changes = engine.getChangedIDs();
  do_check_eq(1, Object.keys(changes).length);
  do_check_true(guid2 in changes);
  do_check_true(changes[guid2] > oldTime);

  _("Ensure non-syncable add-ons aren't picked up by reconciler changes.");
  reconciler._addons  = {};
  reconciler._changes = [];
  let record = {
    id:             "DUMMY",
    guid:           Utils.makeGUID(),
    enabled:        true,
    installed:      true,
    modified:       new Date(),
    type:           "UNSUPPORTED",
    scope:          0,
    foreignInstall: false
  };
  reconciler.addons["DUMMY"] = record;
  reconciler._addChange(record.modified, CHANGE_INSTALLED, record);

  changes = engine.getChangedIDs();
  _(JSON.stringify(changes));
  do_check_eq(0, Object.keys(changes).length);

  advance_test();
});

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Addons").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.AddonsRepository").level =
    Log4Moz.Level.Trace;

  reconciler.startListening();

  advance_test();
}
