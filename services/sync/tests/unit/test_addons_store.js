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

const prefs = new Preferences();

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
awaitPromise(overrideBuiltIns({ "system": [SYSTEM_ADDON_ID] }));
startupManager();

let engine;
let tracker;
let store;
let reconciler;

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

add_task(async function setup() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Addons").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Tracker.Addons").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.AddonsRepository").level = Log.Level.Trace;

  await Service.engineManager.register(AddonsEngine);
  engine     = Service.engineManager.get("addons");
  tracker    = engine._tracker;
  store      = engine._store;
  reconciler = engine._reconciler;

  reconciler.startListening();

  // Don't flush to disk in the middle of an event listener!
  // This causes test hangs on WinXP.
  reconciler._shouldPersist = false;
});

add_task(async function test_remove() {
  _("Ensure removing add-ons from deleted records works.");

  let addon = installAddon("test_bootstrap1_1");
  let record = createRecordForThisApp(addon.syncGUID, addon.id, true, true);

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  Assert.equal(null, newAddon);
});

add_task(async function test_apply_enabled() {
  _("Ensures that changes to the userEnabled flag apply.");

  let addon = installAddon("test_bootstrap1_1");
  Assert.ok(addon.isActive);
  Assert.ok(!addon.userDisabled);

  _("Ensure application of a disable record works as expected.");
  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  let failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enable record works as expected.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, false));
  failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(!addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enabled state updates don't apply if the ignore pref is set.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  Svc.Prefs.set("addons.ignoreUserEnabledChanges", true);
  failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(!addon.userDisabled);
  records = [];

  uninstallAddon(addon);
  Svc.Prefs.reset("addons.ignoreUserEnabledChanges");
});

add_task(async function test_apply_enabled_appDisabled() {
  _("Ensures that changes to the userEnabled flag apply when the addon is appDisabled.");

  let addon = installAddon("test_install3"); // this addon is appDisabled by default.
  Assert.ok(addon.appDisabled);
  Assert.ok(!addon.isActive);
  Assert.ok(!addon.userDisabled);

  _("Ensure application of a disable record works as expected.");
  store.reconciler.pruneChangesBeforeDate(Date.now() + 10);
  store.reconciler._changes = [];
  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, false, false));
  let failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  _("Ensure enable record works as expected.");
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, false));
  failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);
  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(!addon.userDisabled);
  checkReconcilerUpToDate(addon);
  records = [];

  uninstallAddon(addon);
});

add_task(async function test_ignore_different_appid() {
  _("Ensure that incoming records with a different application ID are ignored.");

  // We test by creating a record that should result in an update.
  let addon = installAddon("test_bootstrap1_1");
  Assert.ok(!addon.userDisabled);

  let record = createRecordForThisApp(addon.syncGUID, addon.id, false, false);
  record.applicationID = "FAKE_ID";

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(!newAddon.userDisabled);

  uninstallAddon(addon);
});

add_task(async function test_ignore_unknown_source() {
  _("Ensure incoming records with unknown source are ignored.");

  let addon = installAddon("test_bootstrap1_1");

  let record = createRecordForThisApp(addon.syncGUID, addon.id, false, false);
  record.source = "DUMMY_SOURCE";

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  Assert.ok(!newAddon.userDisabled);

  uninstallAddon(addon);
});

add_task(async function test_apply_uninstall() {
  _("Ensures that uninstalling an add-on from a record works.");

  let addon = installAddon("test_bootstrap1_1");

  let records = [];
  records.push(createRecordForThisApp(addon.syncGUID, addon.id, true, true));
  let failed = await store.applyIncomingBatch(records);
  Assert.equal(0, failed.length);

  addon = getAddonFromAddonManagerByID(addon.id);
  Assert.equal(null, addon);
});

add_test(function test_addon_syncability() {
  _("Ensure isAddonSyncable functions properly.");

  Svc.Prefs.set("addons.trustedSourceHostnames",
                "addons.mozilla.org,other.example.com");

  Assert.ok(!store.isAddonSyncable(null));

  let addon = installAddon("test_bootstrap1_1");
  Assert.ok(store.isAddonSyncable(addon));

  let dummy = {};
  const KEYS = ["id", "syncGUID", "type", "scope", "foreignInstall", "isSyncable"];
  for (let k of KEYS) {
    dummy[k] = addon[k];
  }

  Assert.ok(store.isAddonSyncable(dummy));

  dummy.type = "UNSUPPORTED";
  Assert.ok(!store.isAddonSyncable(dummy));
  dummy.type = addon.type;

  dummy.scope = 0;
  Assert.ok(!store.isAddonSyncable(dummy));
  dummy.scope = addon.scope;

  dummy.isSyncable = false;
  Assert.ok(!store.isAddonSyncable(dummy));
  dummy.isSyncable = addon.isSyncable;

  dummy.foreignInstall = true;
  Assert.ok(!store.isAddonSyncable(dummy));
  dummy.foreignInstall = false;

  uninstallAddon(addon);

  Assert.ok(!store.isSourceURITrusted(null));

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
    Assert.ok(store.isSourceURITrusted(Services.io.newURI(uri)));
  }

  for (let uri of untrusted) {
    Assert.ok(!store.isSourceURITrusted(Services.io.newURI(uri)));
  }

  Svc.Prefs.set("addons.trustedSourceHostnames", "");
  for (let uri of trusted) {
    Assert.ok(!store.isSourceURITrusted(Services.io.newURI(uri)));
  }

  Svc.Prefs.set("addons.trustedSourceHostnames", "addons.mozilla.org");
  Assert.ok(store.isSourceURITrusted(Services.io.newURI("https://addons.mozilla.org/foo")));

  Svc.Prefs.reset("addons.trustedSourceHostnames");

  run_next_test();
});

add_test(function test_ignore_hotfixes() {
  _("Ensure that hotfix extensions are ignored.");

  // A hotfix extension is one that has the id the same as the
  // extensions.hotfix.id pref.
  let extensionPrefs = new Preferences("extensions.");

  let addon = installAddon("test_bootstrap1_1");
  Assert.ok(store.isAddonSyncable(addon));

  let dummy = {};
  const KEYS = ["id", "syncGUID", "type", "scope", "foreignInstall", "isSyncable"];
  for (let k of KEYS) {
    dummy[k] = addon[k];
  }

  // Basic sanity check.
  Assert.ok(store.isAddonSyncable(dummy));

  extensionPrefs.set("hotfix.id", dummy.id);
  Assert.ok(!store.isAddonSyncable(dummy));

  // Verify that int values don't throw off checking.
  let prefSvc = Services.prefs.getBranch("extensions.");
  // Need to delete pref before changing type.
  prefSvc.deleteBranch("hotfix.id");
  prefSvc.setIntPref("hotfix.id", 0xdeadbeef);

  Assert.ok(store.isAddonSyncable(dummy));

  uninstallAddon(addon);

  extensionPrefs.reset("hotfix.id");

  run_next_test();
});


add_task(async function test_get_all_ids() {
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
  Assert.ok(store.isAddonSyncable(addon1));
  Assert.ok(store.isAddonSyncable(addon2));
  Assert.ok(store.isAddonSyncable(addon3));

  let ids = await store.getAllIDs();

  Assert.equal("object", typeof(ids));
  Assert.equal(3, Object.keys(ids).length);
  Assert.ok(addon1.syncGUID in ids);
  Assert.ok(addon2.syncGUID in ids);
  Assert.ok(addon3.syncGUID in ids);

  addon1.install.cancel();
  uninstallAddon(addon2);
  uninstallAddon(addon3);
});

add_task(async function test_change_item_id() {
  _("Ensures that changeItemID() works properly.");

  let addon = installAddon("test_bootstrap1_1");

  let oldID = addon.syncGUID;
  let newID = Utils.makeGUID();

  await store.changeItemID(oldID, newID);

  let newAddon = getAddonFromAddonManagerByID(addon.id);
  Assert.notEqual(null, newAddon);
  Assert.equal(newID, newAddon.syncGUID);

  uninstallAddon(newAddon);
});

add_task(async function test_create() {
  _("Ensure creating/installing an add-on from a record works.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  let addon = installAddon("test_bootstrap1_1");
  let id = addon.id;
  uninstallAddon(addon);

  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(0, failed.length);

  let newAddon = getAddonFromAddonManagerByID(id);
  Assert.notEqual(null, newAddon);
  Assert.equal(guid, newAddon.syncGUID);
  Assert.ok(!newAddon.userDisabled);

  uninstallAddon(newAddon);

  await promiseStopServer(server);
});

add_task(async function test_create_missing_search() {
  _("Ensures that failed add-on searches are handled gracefully.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  // The handler for this ID is not installed, so a search should 404.
  const id = "missing@tests.mozilla.org";
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(1, failed.length);
  Assert.equal(guid, failed[0]);

  let addon = getAddonFromAddonManagerByID(id);
  Assert.equal(null, addon);

  await promiseStopServer(server);
});

add_task(async function test_create_bad_install() {
  _("Ensures that add-ons without a valid install are handled gracefully.");

  let server = createAndStartHTTPServer(HTTP_PORT);

  // The handler returns a search result but the XPI will 404.
  const id = "missing-xpi@tests.mozilla.org";
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, id, true, false);

  /* let failed = */ await store.applyIncomingBatch([record]);
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
  Assert.equal(null, addon);

  await promiseStopServer(server);
});

add_task(async function test_ignore_system() {
  _("Ensure we ignore system addons");
  // Our system addon should not appear in getAllIDs
  await engine._refreshReconcilerState();
  let num = 0;
  let ids = await store.getAllIDs();
  for (let guid in ids) {
    num += 1;
    let addon = reconciler.getAddonStateFromSyncGUID(guid);
    Assert.notEqual(addon.id, SYSTEM_ADDON_ID);
  }
  Assert.ok(num > 1, "should have seen at least one.");
});

add_task(async function test_incoming_system() {
  _("Ensure we handle incoming records that refer to a system addon");
  // eg, loop initially had a normal addon but it was then "promoted" to be a
  // system addon but wanted to keep the same ID. The server record exists due
  // to this.

  // before we start, ensure the system addon isn't disabled.
  Assert.ok(!getAddonFromAddonManagerByID(SYSTEM_ADDON_ID).userDisabled);

  // Now simulate an incoming record with the same ID as the system addon,
  // but flagged as disabled - it should not be applied.
  let server = createAndStartHTTPServer(HTTP_PORT);
  // We make the incoming record flag the system addon as disabled - it should
  // be ignored.
  let guid = Utils.makeGUID();
  let record = createRecordForThisApp(guid, SYSTEM_ADDON_ID, false, false);

  let failed = await store.applyIncomingBatch([record]);
  Assert.equal(0, failed.length);

  // The system addon should still not be userDisabled.
  Assert.ok(!getAddonFromAddonManagerByID(SYSTEM_ADDON_ID).userDisabled);

  await promiseStopServer(server);
});

add_task(async function test_wipe() {
  _("Ensures that wiping causes add-ons to be uninstalled.");

  let addon1 = installAddon("test_bootstrap1_1");

  await store.wipe();

  let addon = getAddonFromAddonManagerByID(addon1.id);
  Assert.equal(null, addon);
});

add_task(async function test_wipe_and_install() {
  _("Ensure wipe followed by install works.");

  // This tests the reset sync flow where remote data is replaced by local. The
  // receiving client will see a wipe followed by a record which should undo
  // the wipe.
  let installed = installAddon("test_bootstrap1_1");

  let record = createRecordForThisApp(installed.syncGUID, installed.id, true,
                                      false);

  await store.wipe();

  let deleted = getAddonFromAddonManagerByID(installed.id);
  Assert.equal(null, deleted);

  // Re-applying the record can require re-fetching the XPI.
  let server = createAndStartHTTPServer(HTTP_PORT);

  await store.applyIncoming(record);

  let fetched = getAddonFromAddonManagerByID(record.addonID);
  Assert.ok(!!fetched);

  await promiseStopServer(server);
});

add_test(function cleanup() {
  // There's an xpcom-shutdown hook for this, but let's give this a shot.
  reconciler.stopListening();
  run_next_test();
});
