/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://services-common/async.js");
ChromeUtils.import("resource://services-sync/addonsreconciler.js");
ChromeUtils.import("resource://services-sync/engines/addons.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences", "resource://gre/modules/Preferences.jsm");

const prefs = new Preferences();
prefs.set("extensions.getAddons.get.url",
          "http://localhost:8888/search/guid:%IDS%");
prefs.set("extensions.install.requireSecureOrigin", false);

let engine;
let syncID;
let reconciler;
let tracker;

AddonTestUtils.init(this);

const ADDONS = {
  test_install1: {
    "install.rdf": {
      id: "addon1@tests.mozilla.org",
      version: "1.0",
      name: "Test 1",
      description: "Test Description",
      bootstrap: true,

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
    "icon.png": "Fake icon image",
    "icon64.png": "Fake icon image",
  },
  test_bootstrap1_1: {
    "install.rdf": {
      id: "bootstrap1@tests.mozilla.org",
      version: "1.0",
      bootstrap: "true",
      multiprocessCompatible: "true",
      name: "Test Bootstrap 1",
      description: "Test Description",

      iconURL: "chrome://foo/skin/icon.png",
      aboutURL: "chrome://foo/content/about.xul",
      optionsURL: "chrome://foo/content/options.xul",

      targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
          minVersion: "1",
          maxVersion: "1"}],
    },
  },
};

const XPIS = {};
for (let [name, files] of Object.entries(ADDONS)) {
  XPIS[name] = AddonTestUtils.createTempXPIFile(files);
}

async function resetReconciler() {
  reconciler._addons = {};
  reconciler._changes = [];

  await reconciler.saveState();

  await tracker.clearChangedIDs();
}

add_task(async function setup() {
  AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  AddonTestUtils.overrideCertDB();
  await AddonTestUtils.promiseStartupManager();

  await Service.engineManager.register(AddonsEngine);
  engine = Service.engineManager.get("addons");
  syncID = await engine.resetLocalSyncID();
  reconciler = engine._reconciler;
  tracker = engine._tracker;

  reconciler.startListening();

  // Don't flush to disk in the middle of an event listener!
  // This causes test hangs on WinXP.
  reconciler._shouldPersist = false;

  await resetReconciler();
});

// This is a basic sanity test for the unit test itself. If this breaks, the
// add-ons API likely changed upstream.
add_task(async function test_addon_install() {
  _("Ensure basic add-on APIs work as expected.");

  let install = await AddonManager.getInstallForFile(XPIS.test_bootstrap1_1);
  Assert.notEqual(install, null);
  Assert.equal(install.type, "extension");
  Assert.equal(install.name, "Test Bootstrap 1");

  await resetReconciler();
});

add_task(async function test_find_dupe() {
  _("Ensure the _findDupe() implementation is sane.");

  // This gets invoked at the top of sync, which is bypassed by this
  // test, so we do it manually.
  await engine._refreshReconcilerState();

  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);

  let record = {
    id:            Utils.makeGUID(),
    addonID:       addon.id,
    enabled:       true,
    applicationID: Services.appinfo.ID,
    source:        "amo",
  };

  let dupe = await engine._findDupe(record);
  Assert.equal(addon.syncGUID, dupe);

  record.id = addon.syncGUID;
  dupe = await engine._findDupe(record);
  Assert.equal(null, dupe);

  await uninstallAddon(addon, reconciler);
  await resetReconciler();
});

add_task(async function test_get_changed_ids() {
  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function() {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  _("Ensure getChangedIDs() has the appropriate behavior.");

  _("Ensure getChangedIDs() returns an empty object by default.");
  let changes = await engine.getChangedIDs();
  Assert.equal("object", typeof(changes));
  Assert.equal(0, Object.keys(changes).length);

  _("Ensure tracker changes are populated.");
  let now = new Date();
  let changeTime = now.getTime() / 1000;
  let guid1 = Utils.makeGUID();
  await tracker.addChangedID(guid1, changeTime);

  changes = await engine.getChangedIDs();
  Assert.equal("object", typeof(changes));
  Assert.equal(1, Object.keys(changes).length);
  Assert.ok(guid1 in changes);
  Assert.equal(changeTime, changes[guid1]);

  await tracker.clearChangedIDs();

  _("Ensure reconciler changes are populated.");
  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  await tracker.clearChangedIDs(); // Just in case.
  changes = await engine.getChangedIDs();
  Assert.equal("object", typeof(changes));
  Assert.equal(1, Object.keys(changes).length);
  Assert.ok(addon.syncGUID in changes);
  _("Change time: " + changeTime + ", addon change: " + changes[addon.syncGUID]);
  Assert.ok(changes[addon.syncGUID] >= changeTime);

  let oldTime = changes[addon.syncGUID];
  let guid2 = addon.syncGUID;
  await uninstallAddon(addon, reconciler);
  changes = await engine.getChangedIDs();
  Assert.equal(1, Object.keys(changes).length);
  Assert.ok(guid2 in changes);
  Assert.ok(changes[guid2] > oldTime);

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
    foreignInstall: false,
  };
  reconciler.addons.DUMMY = record;
  await reconciler._addChange(record.modified, CHANGE_INSTALLED, record);

  changes = await engine.getChangedIDs();
  _(JSON.stringify(changes));
  Assert.equal(0, Object.keys(changes).length);

  await resetReconciler();
});

add_task(async function test_disabled_install_semantics() {
  _("Ensure that syncing a disabled add-on preserves proper state.");

  // This is essentially a test for bug 712542, which snuck into the original
  // add-on sync drop. It ensures that when an add-on is installed that the
  // disabled state and incoming syncGUID is preserved, even on the next sync.
  const USER       = "foo";
  const PASSWORD   = "password";
  const ADDON_ID   = "addon1@tests.mozilla.org";

  let server = new SyncServer();
  server.start();
  await SyncTestingInfrastructure(server, USER, PASSWORD);

  await generateNewKeys(Service.collectionKeys);

  let contents = {
    meta: {global: {engines: {addons: {version: engine.version, syncID}}}},
    crypto: {},
    addons: {},
  };

  server.registerUser(USER, "password");
  server.createContents(USER, contents);

  let amoServer = new HttpServer();
  amoServer.registerFile("/search/guid:addon1%40tests.mozilla.org",
                         do_get_file("addon1-search.json"));

  amoServer.registerFile("/addon1.xpi", XPIS.test_install1);
  amoServer.start(8888);

  // Insert an existing record into the server.
  let id = Utils.makeGUID();
  let now = Date.now() / 1000;

  let record = encryptPayload({
    id,
    applicationID: Services.appinfo.ID,
    addonID:       ADDON_ID,
    enabled:       false,
    deleted:       false,
    source:        "amo",
  });
  let wbo = new ServerWBO(id, record, now - 2);
  server.insertWBO(USER, "addons", wbo);

  _("Performing sync of add-ons engine.");
  await engine._sync();

  // At this point the non-restartless extension should be staged for install.

  // Don't need this server any more.
  await promiseStopServer(amoServer);

  // We ensure the reconciler has recorded the proper ID and enabled state.
  let addon = reconciler.getAddonStateFromSyncGUID(id);
  Assert.notEqual(null, addon);
  Assert.equal(false, addon.enabled);

  // We fake an app restart and perform another sync, just to make sure things
  // are sane.
  await AddonTestUtils.promiseRestartManager();

  let collection = server.getCollection(USER, "addons");
  engine.lastModified = collection.timestamp;
  await engine._sync();

  // The client should not upload a new record. The old record should be
  // retained and unmodified.
  Assert.equal(1, collection.count());

  let payload = collection.payloads()[0];
  Assert.notEqual(null, collection.wbo(id));
  Assert.equal(ADDON_ID, payload.addonID);
  Assert.ok(!payload.enabled);

  await promiseStopServer(server);
});

add_test(function cleanup() {
  // There's an xpcom-shutdown hook for this, but let's give this a shot.
  reconciler.stopListening();
  run_next_test();
});
