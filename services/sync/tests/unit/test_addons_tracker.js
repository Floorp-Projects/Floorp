/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/AddonManager.jsm");
ChromeUtils.import("resource://services-sync/engines/addons.js");
ChromeUtils.import("resource://services-sync/constants.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");

loadAddonTestFunctions();
startupManager();
Svc.Prefs.set("engine.addons", true);

let engine;
let reconciler;
let store;
let tracker;

const addon1ID = "addon1@tests.mozilla.org";

async function cleanup() {
  Svc.Obs.notify("weave:engine:stop-tracking");
  tracker.stopTracking();

  tracker.resetScore();
  tracker.clearChangedIDs();

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

  Assert.equal(0, Object.keys(tracker.changedIDs).length);
  Assert.equal(0, tracker.score);

  await cleanup();
});

add_task(async function test_not_tracking() {
  _("Ensures the tracker doesn't do anything when it isn't tracking.");

  let addon = await installAddon("test_bootstrap1_1");
  await uninstallAddon(addon);

  Assert.equal(0, Object.keys(tracker.changedIDs).length);
  Assert.equal(0, tracker.score);

  await cleanup();
});

add_task(async function test_track_install() {
  _("Ensure that installing an add-on notifies tracker.");

  reconciler.startListening();

  Svc.Obs.notify("weave:engine:start-tracking");

  Assert.equal(0, tracker.score);
  let addon = await installAddon("test_bootstrap1_1");
  let changed = tracker.changedIDs;

  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon);
  await cleanup();
});

add_task(async function test_track_uninstall() {
  _("Ensure that uninstalling an add-on notifies tracker.");

  reconciler.startListening();

  let addon = await installAddon("test_bootstrap1_1");
  let guid = addon.syncGUID;
  Assert.equal(0, tracker.score);

  Svc.Obs.notify("weave:engine:start-tracking");

  await uninstallAddon(addon);
  let changed = tracker.changedIDs;
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(guid in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await cleanup();
});

add_task(async function test_track_user_disable() {
  _("Ensure that tracker sees disabling of add-on");

  reconciler.startListening();

  let addon = await installAddon("test_bootstrap1_1");
  Assert.ok(!addon.userDisabled);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);

  Svc.Obs.notify("weave:engine:start-tracking");
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
  addon.userDisabled = true;
  _("Disabling started...");
  await disabledPromise;

  let changed = tracker.changedIDs;
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon);
  await cleanup();
});

add_task(async function test_track_enable() {
  _("Ensure that enabling a disabled add-on notifies tracker.");

  reconciler.startListening();

  let addon = await installAddon("test_bootstrap1_1");
  addon.userDisabled = true;
  await Async.promiseYield();

  Assert.equal(0, tracker.score);

  Svc.Obs.notify("weave:engine:start-tracking");
  addon.userDisabled = false;
  await Async.promiseYield();

  let changed = tracker.changedIDs;
  Assert.equal(1, Object.keys(changed).length);
  Assert.ok(addon.syncGUID in changed);
  Assert.equal(SCORE_INCREMENT_XLARGE, tracker.score);

  await uninstallAddon(addon);
  await cleanup();
});
