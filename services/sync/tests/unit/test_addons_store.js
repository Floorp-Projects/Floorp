/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://services-sync/addonutils.js");
Cu.import("resource://services-sync/engines/addons.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://testing-common/services/sync/utils.js");
Cu.import("resource://gre/modules/FileUtils.jsm");

const HTTP_PORT = 8888;

var prefs = new Preferences();

prefs.set("extensions.getAddons.get.url", "http://localhost:8888/search/guid:%IDS%");
prefs.set("extensions.install.requireSecureOrigin", false);

const SYSTEM_ADDON_ID = "system1@tests.mozilla.org";
let systemAddonFile;

// The system add-on must be installed before AddonManager is started.
function loadSystemAddon() {
  let addonFilename = SYSTEM_ADDON_ID + ".xpi";
  const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
  do_get_file(ExtensionsTestPath("/data/system_addons/system1_1.xpi")).copyTo(distroDir, addonFilename);
  systemAddonFile = FileUtils.File(distroDir.path);
  systemAddonFile.append(addonFilename);
  systemAddonFile.lastModifiedTime = Date.now();
  // As we're not running in application, we need to setup the features directory
  // used by system add-ons.
  registerDirectory("XREAppFeat", distroDir);
}

loadAddonTestFunctions();
loadSystemAddon();
startupManager();

Service.engineManager.register(AddonsEngine);
var engine     = Service.engineManager.get("addons");
var tracker    = engine._tracker;
var store      = engine._store;
var reconciler = engine._reconciler;

/**
 * Create a AddonsRec for this application with the fields specified.
 *
 * @param  id       Sync GUID of record
 * @param  addonId  ID of add-on
 * @param  enabled  Boolean whether record is enabled
 * @param  deleted  Boolean whether record was deleted
 */
function createRecordForThisApp(id, addonId, enabled, deleted) {
  return {
    id,
    addonID:       addonId,
    enabled,
    deleted:       !!deleted,
    applicationID: Services.appinfo.ID,
    source:        "amo"
  };
}

function createAndStartHTTPServer(port) {
  try {
    let server = new HttpServer();

    let bootstrap1XPI = ExtensionsTestPath("/addons/test_bootstrap1_1.xpi");

    server.registerFile("/search/guid:bootstrap1%40tests.mozilla.org",
                        do_get_file("bootstrap1-search.xml"));
    server.registerFile("/bootstrap1.xpi", do_get_file(bootstrap1XPI));

    server.registerFile("/search/guid:missing-xpi%40tests.mozilla.org",
                        do_get_file("missing-xpi-search.xml"));

    server.registerFile("/search/guid:system1%40tests.mozilla.org",
                        do_get_file("systemaddon-search.xml"));
    server.registerFile("/system.xpi", systemAddonFile);

    server.start(port);

    return server;
  } catch (ex) {
    _("Got exception starting HTTP server on port " + port);
    _("Error: " + Log.exceptionStr(ex));
    do_throw(ex);
  }
  return null; /* not hit, but keeps eslint happy! */
}

// A helper function to ensure that the reconciler's current view of the addon
// is the same as the addon itself. If it's not, then the reconciler missed a
// change, and is likely to re-upload the addon next sync because of the change
// it missed.
function checkReconcilerUpToDate(addon) {
  let stateBefore = Object.assign({}, store.reconciler.addons[addon.id]);
  store.reconciler.rectifyStateFromAddon(addon);
  let stateAfter = store.reconciler.addons[addon.id];
  deepEqual(stateBefore, stateAfter);
}

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Addons").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Tracker.Addons").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.AddonsRepository").level =
    Log.Level.Trace;

  reconciler.startListening();

  // Don't flush to disk in the middle of an event listener!
  // This causes test hangs on WinXP.
  reconciler._shouldPersist = false;

  run_next_test();
}

add_test(function test_remove() {
  _("Ensure removing add-ons from deleted records works.");

  let addon = installAddon("test_bootstrap1_1");
  let record = createRecordForThisApp(addon.syncGUID, addon.id, true, true);

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  do_check_eq(null, newAddon);

  run_next_test();
});

add_test(function test_apply_enabled() {
  _("Ensures that changes to the userEnabled flag apply.");

  let addon = installAddon("test_bootstrap1_1");
  do_check_true(addon.isActive);
  do_check_false(addon.userDisabled);

  _("Ensure application of a disable record works as expected.");
  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  let failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_true(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enable record works as expected.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, false));
  failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_false(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enabled state updates don't apply if the ignore pref is set.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  Svc.Prefs.set("addons.ignoreUserEnabledChanges", true);
  failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_false(addon.userDisabled);
  records = [];

  uninstallAddon(addon);
  Svc.Prefs.reset("addons.ignoreUserEnabledChanges");
  run_next_test();
});

add_test(function test_apply_enabled_appDisabled() {
  _("Ensures that changes to the userEnabled flag apply when the addon is appDisabled.");

  let addon = installAddon("test_install3"); // this addon is appDisabled by default.
  do_check_true(addon.appDisabled);
  do_check_false(addon.isActive);
  do_check_false(addon.userDisabled);

  _("Ensure application of a disable record works as expected.");
  store.reconciler.pruneChangesBeforeDate(Date.now() + 10);
  store.reconciler._changes = [];
  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  let failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_true(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enable record works as expected.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, false));
  failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_false(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  uninstallAddon(addon);
  run_next_test();
});

add_test(function test_ignore_different_appid() {
  _("Ensure that incoming records with a different application ID are ignored.");

  // We test by creating a record that should result in an update.
  let addon = installAddon("test_bootstrap1_1");
  do_check_false(addon.userDisabled);

  let record = createRecordForThisApp(addon.syncGUID, addon.id, false, false);
  record.applicationID = "FAKE_ID";

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  do_check_false(newAddon.userDisabled);

  uninstallAddon(addon);

  run_next_test();
});

add_test(function test_ignore_unknown_source() {
  _("Ensure incoming records with unknown source are ignored.");

  let addon = installAddon("test_bootstrap1_1");

  let record = createRecordForThisApp(addon.syncGUID, addon.id, false, false);
  record.source = "DUMMY_SOURCE";

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  do_check_false(newAddon.userDisabled);

  uninstallAddon(addon);

  run_next_test();
});

add_test(function test_apply_uninstall() {
  _("Ensures that uninstalling an add-on from a record works.");

  let addon = installAddon("test_bootstrap1_1");

  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, true));
  let failed = store.applyIncomingBatch(records);
  do_check_eq(0, failed.length);

  addon = getAddonFromAddonManagerByID(addon.id);
  do_check_eq(null, addon);

  run_next_test();
});

add_test(function test_addon_syncability() {
  _("Ensure isAddonSyncable functions properly.");

  Svc.Prefs.set("addons.trustedSourceHostnames",
                "addons.mozilla.org,other.example.com");

  do_check_false(store.isAddonSyncable(null));

  let addon = installAddon("test_bootstrap1_1");
  do_check_true(store.isAddonSyncable(addon));

  let dummy = {};
  const KEYS = ["id", "syncGUID", "type", "scope", "foreignInstall", "isSyncable"];
  for (let k of KEYS) {
    dummy[k] = addon[k];
  }

  do_check_true(store.isAddonSyncable(dummy));

  dummy.type = "UNSUPPORTED";
  do_check_false(store.isAddonSyncable(dummy));
  dummy.type = addon.type;

  dummy.scope = 0;
  do_check_false(store.isAddonSyncable(dummy));
  dummy.scope = addon.scope;

  dummy.isSyncable = false;
  do_check_false(store.isAddonSyncable(dummy));
  dummy.isSyncable = addon.isSyncable;

  dummy.foreignInstall = true;
  do_check_false(store.isAddonSyncable(dummy));
  dummy.foreignInstall = false;

  uninstallAddon(addon);

  do_check_false(store.isSourceURITrusted(null));

  function createURI(s) {
    let service = Components.classes["@mozilla.org/network/io-service;1"]
                  .getService(Components.interfaces.nsIIOService);
    return service.newURI(s);
  }

  let trusted = [
    "https://addons.mozilla.org/foo",
    "https://other.example.com/foo"
  ];

  let untrusted = [
    "http://addons.mozilla.org/foo",     // non-https
    "ftps://addons.mozilla.org/foo",     // non-https
    "https://untrusted.example.com/foo", // non-trusted hostname`
  ];

  for (let uri of trusted) {
    do_check_true(store.isSourceURITrusted(createURI(uri)));
  }

  for (let uri of untrusted) {
    do_check_false(store.isSourceURITrusted(createURI(uri)));
  }

  Svc.Prefs.set("addons.trustedSourceHostnames", "");
  for (let uri of trusted) {
    do_check_false(store.isSourceURITrusted(createURI(uri)));
  }

  Svc.Prefs.set("addons.trustedSourceHostnames", "addons.mozilla.org");
  do_check_true(store.isSourceURITrusted(createURI("https://addons.mozilla.org/foo")));

  Svc.Prefs.reset("addons.trustedSourceHostnames");

  run_next_test();
});

add_test(function test_ignore_hotfixes() {
  _("Ensure that hotfix extensions are ignored.");

  // A hotfix extension is one that has the id the same as the
  // extensions.hotfix.id pref.
  let extensionPrefs = new Preferences("extensions.");

  let addon = installAddon("test_bootstrap1_1");
  do_check_true(store.isAddonSyncable(addon));

  let dummy = {};
  const KEYS = ["id", "syncGUID", "type", "scope", "foreignInstall", "isSyncable"];
  for (let k of KEYS) {
    dummy[k] = addon[k];
  }

  // Basic sanity check.
  do_check_true(store.isAddonSyncable(dummy));

  extensionPrefs.set("hotfix.id", dummy.id);
  do_check_false(store.isAddonSyncable(dummy));

  // Verify that int values don't throw off checking.
  let prefSvc = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefService)
                .getBranch("extensions.");
  // Need to delete pref before changing type.
  prefSvc.deleteBranch("hotfix.id");
  prefSvc.setIntPref("hotfix.id", 0xdeadbeef);

  do_check_true(store.isAddonSyncable(dummy));

  uninstallAddon(addon);

  extensionPrefs.reset("hotfix.id");

  run_next_test();
});


add_test(function test_get_all_ids() {
  _("Ensures that getAllIDs() returns an appropriate set.");

  _("Installing two addons.");
  // XXX - this test seems broken - at this point, before we've installed the
  // addons below, store.getAllIDs() returns all addons installed by previous
  // tests, even though those tests uninstalled the addon.
  // So if any tests above ever add a new addon ID, they are going to need to
  // be added here too.
  // do_check_eq(0, Object.keys(store.getAllIDs()).length);
  let addon1 = installAddon("test_install1");
  let addon2 = installAddon("test_bootstrap1_1");
  let addon3 = installAddon("test_install3");

  _("Ensure they're syncable.");
  do_check_true(store.isAddonSyncable(addon1));
  do_check_true(store.isAddonSyncable(addon2));
  do_check_true(store.isAddonSyncable(addon3));

  let ids = store.getAllIDs();

  do_check_eq("object", typeof(ids));
  do_check_eq(3, Object.keys(ids).length);
  do_check_true(addon1.syncGUID in ids);
  do_check_true(addon2.syncGUID in ids);
  do_check_true(addon3.syncGUID in ids);

  addon1.install.cancel();
  uninstallAddon(addon2);
  uninstallAddon(addon3);

  run_next_test();
});

add_test(function test_change_item_id() {
  _("Ensures that changeItemID() works properly.");

  let addon = installAddon("test_bootstrap1_1");

  let oldID = addon.syncGUID;
  let newID = Utils.makeGUID();

  store.changeItemID(oldID, newID);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  do_check_neq(null, newAddon);
  do_check_eq(newID, newAddon.syncGUID);

  uninstallAddon(newAddon);

  run_next_test();
});

add_test(function test_create() {
  _("Ensure creating/installing an add-on from a record works.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  let addon = installAddon("test_bootstrap1_1");
  let id = addon.id;
  uninstallAddon(addon);

  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(id);
  do_check_neq(null, newAddon);
  do_check_eq(guid, newAddon.syncGUID);
  do_check_false(newAddon.userDisabled);

  uninstallAddon(newAddon);

  server.stop(run_next_test);
});

add_test(function test_create_missing_search() {
  _("Ensures that failed add-on searches are handled gracefully.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  // The handler for this ID is not installed, so a search should 404.
  const id = "missing@tests.mozilla.org";
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(1, failed.length);
  do_check_eq(guid, failed[0]);

  let addon = getAddonFromAddonManagerByID(id);
  do_check_eq(null, addon);

  server.stop(run_next_test);
});

add_test(function test_create_bad_install() {
  _("Ensures that add-ons without a valid install are handled gracefully.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  // The handler returns a search result but the XPI will 404.
  const id = "missing-xpi@tests.mozilla.org";
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  /* let failed = */ store.applyIncomingBatch([record]);
  // This addon had no source URI so was skipped - but it's not treated as
  // failure.
  // XXX - this test isn't testing what we thought it was. Previously the addon
  // was not being installed due to requireSecureURL checking *before* we'd
  // attempted to get the XPI.
  // With requireSecureURL disabled we do see a download failure, but the addon
  // *does* get added to |failed|.
  // FTR: onDownloadFailed() is called with ERROR_NETWORK_FAILURE, so it's going
  // to be tricky to distinguish a 404 from other transient network errors
  // where we do want the addon to end up in |failed|.
  // This is being tracked in bug 1284778.
  // do_check_eq(0, failed.length);

  let addon = getAddonFromAddonManagerByID(id);
  do_check_eq(null, addon);

  server.stop(run_next_test);
});

add_test(function test_ignore_system() {
  _("Ensure we ignore system addons");
  // Our system addon should not appear in getAllIDs
  engine._refreshReconcilerState();
  let num = 0;
  for (let guid in store.getAllIDs()) {
    num += 1;
    let addon = reconciler.getAddonStateFromSyncGUID(guid);
    do_check_neq(addon.id, SYSTEM_ADDON_ID);
  }
  do_check_true(num > 1, "should have seen at least one.")
  run_next_test();
});

add_test(function test_incoming_system() {
  _("Ensure we handle incoming records that refer to a system addon");
  // eg, loop initially had a normal addon but it was then "promoted" to be a
  // system addon but wanted to keep the same ID. The server record exists due
  // to this.

  // before we start, ensure the system addon isn't disabled.
  do_check_false(getAddonFromAddonManagerByID(SYSTEM_ADDON_ID).userDisabled);

  // Now simulate an incoming record with the same ID as the system addon,
  // but flagged as disabled - it should not be applied.
  let server = createAndStartHTTPServer(HTTP_PORT);
  // We make the incoming record flag the system addon as disabled - it should
  // be ignored.
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, SYSTEM_ADDON_ID, false, false);

  let failed = store.applyIncomingBatch([record]);
  do_check_eq(0, failed.length);

  // The system addon should still not be userDisabled.
  do_check_false(getAddonFromAddonManagerByID(SYSTEM_ADDON_ID).userDisabled);

  server.stop(run_next_test);
});

add_test(function test_wipe() {
  _("Ensures that wiping causes add-ons to be uninstalled.");

  let addon1 = installAddon("test_bootstrap1_1");

  store.wipe();

  let addon = getAddonFromAddonManagerByID(addon1.id);
  do_check_eq(null, addon);

  run_next_test();
});

add_test(function test_wipe_and_install() {
  _("Ensure wipe followed by install works.");

  // This tests the reset sync flow where remote data is replaced by local. The
  // receiving client will see a wipe followed by a record which should undo
  // the wipe.
  let installed = installAddon("test_bootstrap1_1");

  let record = createRecordForThisApp(installed.syncGUID, installed.id, true,
                                      false);

  store.wipe();

  let deleted = getAddonFromAddonManagerByID(installed.id);
  do_check_null(deleted);

  // Re-applying the record can require re-fetching the XPI.
  let server = createAndStartHTTPServer(HTTP_PORT);

  store.applyIncoming(record);

  let fetched = getAddonFromAddonManagerByID(record.addonID);
  do_check_true(!!fetched);

  server.stop(run_next_test);
});

add_test(function cleanup() {
  // There's an xpcom-shutdown hook for this, but let's give this a shot.
  reconciler.stopListening();
  run_next_test();
});
