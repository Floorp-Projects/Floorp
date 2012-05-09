/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/AddonManager.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-sync/addonsreconciler.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-sync/engines/addons.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-sync/service.js");

let prefs = new Preferences();
prefs.set("extensions.getAddons.get.url",
          "http://localhost:8888/search/guid:%IDS%");

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

add_test(function test_disabled_install_semantics() {
  _("Ensure that syncing a disabled add-on preserves proper state.");

  // This is essentially a test for bug 712542, which snuck into the original
  // add-on sync drop. It ensures that when an add-on is installed that the
  // disabled state and incoming syncGUID is preserved, even on the next sync.

  Svc.Prefs.set("addons.ignoreRepositoryChecking", true);

  const USER       = "foo";
  const PASSWORD   = "password";
  const PASSPHRASE = "abcdeabcdeabcdeabcdeabcdea";
  const ADDON_ID   = "addon1@tests.mozilla.org";

  new SyncTestingInfrastructure(USER, PASSWORD, PASSPHRASE);

  generateNewKeys();

  let contents = {
    meta: {global: {engines: {addons: {version: engine.version,
                                      syncID:  engine.syncID}}}},
    crypto: {},
    addons: {}
  };

  let server = new SyncServer();
  server.registerUser(USER, "password");
  server.createContents(USER, contents);
  server.start();

  let amoServer = new nsHttpServer();
  amoServer.registerFile("/search/guid:addon1%40tests.mozilla.org",
                         do_get_file("addon1-search.xml"));

  let installXPI = ExtensionsTestPath("/addons/test_install1.xpi");
  amoServer.registerFile("/addon1.xpi", do_get_file(installXPI));
  amoServer.start(8888);

  // Insert an existing record into the server.
  let id = Utils.makeGUID();
  let now = Date.now() / 1000;

  let record = encryptPayload({
    id:            id,
    applicationID: Services.appinfo.ID,
    addonID:       ADDON_ID,
    enabled:       false,
    deleted:       false,
    source:        "amo",
  });
  let wbo = new ServerWBO(id, record, now - 2);
  server.insertWBO(USER, "addons", wbo);

  _("Performing sync of add-ons engine.");
  engine._sync();

  // At this point the non-restartless extension should be staged for install.

  // Don't need this server any more.
  let cb = Async.makeSpinningCallback();
  amoServer.stop(cb);
  cb.wait();

  // We ensure the reconciler has recorded the proper ID and enabled state.
  let addon = reconciler.getAddonStateFromSyncGUID(id);
  do_check_neq(null, addon);
  do_check_eq(false, addon.enabled);

  // We fake an app restart and perform another sync, just to make sure things
  // are sane.
  restartManager();

  engine._sync();

  // The client should not upload a new record. The old record should be
  // retained and unmodified.
  let collection = server.getCollection(USER, "addons");
  do_check_eq(1, collection.count());

  let payload = collection.payloads()[0];
  do_check_neq(null, collection.wbo(id));
  do_check_eq(ADDON_ID, payload.addonID);
  do_check_false(payload.enabled);

  server.stop(advance_test);
});

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Engine.Addons").level =
    Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.Store.Addons").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.Tracker.Addons").level =
    Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.AddonsRepository").level =
    Log4Moz.Level.Trace;

  new SyncTestingInfrastructure();

  reconciler.startListening();

  advance_test();
}
