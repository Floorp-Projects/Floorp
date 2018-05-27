/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://services-sync/engines/addons.js");
ChromeUtils.import("resource://services-sync/constants.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
AddonTestUtils.overrideCertDB();

Services.prefs.setCharPref("extensions.minCompatibleAppVersion", "0");
Services.prefs.setCharPref("extensions.minCompatiblePlatformVersion", "0");
Services.prefs.setBoolPref("extensions.legacy.enabled", true);

AddonTestUtils.awaitPromise(AddonTestUtils.promiseStartupManager());
Svc.Prefs.set("engine.addons", true);

let engine;
let reconciler;
let store;
let tracker;

const addon1ID = "addon1@tests.mozilla.org";

const ADDONS = {
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

async function cleanup() {
  tracker.stop();

  tracker.resetScore();
  await tracker.clearChangedIDs();

  reconciler._addons = {};
  reconciler._changes = [];
  await reconciler.saveState();
}

add_task(async function setup() {
  await Service.engineManager.register(AddonsEngine);
  engine     = Service.engineManager.get("addons");
  reconciler = engine._reconciler;
  store      = engine._store;
  tracker    = engine._tracker;

  // Don't write out by default.
  tracker.persistChangedIDs = false;

  await cleanup();
});

add_task(async function test_empty() {
  _("Verify the tracker is empty to start with.");

  Assert.equal(0, Object.keys((await tracker.getChangedIDs())).length);
  Assert.equal(0, tracker.score);

  await cleanup();
});

add_task(async function test_not_tracking() {
  _("Ensures the tracker doesn't do anything when it isn't tracking.");

  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  await uninstallAddon(addon, reconciler);

  Assert.equal(0, Object.keys((await tracker.getChangedIDs())).length);
  Assert.equal(0, tracker.score);

  await cleanup();
});

add_task(async function test_track_install() {
  _("Ensure that installing an add-on notifies tracker.");

  reconciler.startListening();

  tracker.start();

  Assert.equal(0, tracker.score);
  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  let changed = await tracker.getChangedIDs();

  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon, reconciler);
  await cleanup();
});

add_task(async function test_track_uninstall() {
  _("Ensure that uninstalling an add-on notifies tracker.");

  reconciler.startListening();

  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  let guid = addon.syncGUID;
  Assert.equal(0, tracker.score);

  tracker.start();

  await uninstallAddon(addon, reconciler);
  let changed = await tracker.getChangedIDs();
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(guid in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await cleanup();
});

add_task(async function test_track_user_disable() {
  _("Ensure that tracker sees disabling of add-on");

  reconciler.startListening();

  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  Assert.ok(!addon.userDisabled);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);

  tracker.start();
  Assert.equal(0, tracker.score);

  let disabledPromise = new Promise(res => {
    let listener = {
      onDisabled(disabled) {
        _("onDisabled");
        if (disabled.id == addon.id) {
          AddonManager.removeAddonListener(listener);
          res();
        }
      },
      onDisabling(disabling) {
        _("onDisabling add-on");
      }
    };
    AddonManager.addAddonListener(listener);
  });

  _("Disabling add-on");
  await addon.disable();
  _("Disabling started...");
  await disabledPromise;
  await reconciler.queueCaller.promiseCallsComplete();

  let changed = await tracker.getChangedIDs();
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon, reconciler);
  await cleanup();
});

add_task(async function test_track_enable() {
  _("Ensure that enabling a disabled add-on notifies tracker.");

  reconciler.startListening();

  let addon = await installAddon(XPIS.test_bootstrap1_1, reconciler);
  await addon.disable();
  await Async.promiseYield();

  Assert.equal(0, tracker.score);

  tracker.start();
  await addon.enable();
  await Async.promiseYield();
  await reconciler.queueCaller.promiseCallsComplete();

  let changed = await tracker.getChangedIDs();
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon, reconciler);
  await cleanup();
});
