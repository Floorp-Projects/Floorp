/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://services-sync/engines/tabs.js");
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

const { SyncScheduler } = ChromeUtils.import(
  "resource://services-sync/policies.js"
);

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/NimbusTestUtils.jsm"
);

const { ExperimentAPI } = ChromeUtils.import(
  "resource://nimbus/ExperimentAPI.jsm"
);

var scheduler = new SyncScheduler(Service);
let clientsEngine;

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_store").get(() => manager.store);

  return { sandbox, manager };
}

add_task(async function setup() {
  await Service.promiseInitialized;
  clientsEngine = Service.clientsEngine;

  scheduler.setDefaults();
});

function fakeSvcWinMediator() {
  // actions on windows are captured in logs
  let logs = [];
  delete Services.wm;

  function getNext() {
    let elt = { addTopics: [], remTopics: [], numAPL: 0, numRPL: 0 };
    logs.push(elt);
    return {
      addEventListener(topic) {
        elt.addTopics.push(topic);
      },
      removeEventListener(topic) {
        elt.remTopics.push(topic);
      },
      gBrowser: {
        addProgressListener() {
          elt.numAPL++;
        },
        removeProgressListener() {
          elt.numRPL++;
        },
      },
    };
  }

  Services.wm = {
    getEnumerator() {
      return [getNext(), getNext()];
    },
  };
  return logs;
}

function fakeGetTabState(tab) {
  return tab;
}

function clearQuickWriteTimer(tracker) {
  if (tracker.tabsQuickWriteTimer) {
    tracker.tabsQuickWriteTimer.clear();
  }
}

add_task(async function run_test() {
  let engine = Service.engineManager.get("tabs");

  _("We assume that tabs have changed at startup.");
  let tracker = engine._tracker;
  tracker.getTabState = fakeGetTabState;

  Assert.ok(tracker.modified);
  Assert.ok(
    Utils.deepEquals(Object.keys(await engine.getChangedIDs()), [
      clientsEngine.localID,
    ])
  );

  let logs;

  _("Test listeners are registered on windows");
  logs = fakeSvcWinMediator();
  tracker.start();
  Assert.equal(logs.length, 2);
  for (let log of logs) {
    Assert.equal(log.addTopics.length, 3);
    Assert.ok(log.addTopics.includes("TabOpen"));
    Assert.ok(log.addTopics.includes("TabClose"));
    Assert.ok(log.addTopics.includes("unload"));
    Assert.equal(log.remTopics.length, 0);
    Assert.equal(log.numAPL, 1, "Added 1 progress listener");
    Assert.equal(log.numRPL, 0, "Didn't remove a progress listener");
  }

  _("Test listeners are unregistered on windows");
  logs = fakeSvcWinMediator();
  await tracker.stop();
  Assert.equal(logs.length, 2);
  for (let log of logs) {
    Assert.equal(log.addTopics.length, 0);
    Assert.equal(log.remTopics.length, 3);
    Assert.ok(log.remTopics.includes("TabOpen"));
    Assert.ok(log.remTopics.includes("TabClose"));
    Assert.ok(log.remTopics.includes("unload"));
    Assert.equal(log.numAPL, 0, "Didn't add a progress listener");
    Assert.equal(log.numRPL, 1, "Removed 1 progress listener");
  }

  _("Test tab listener");
  for (let evttype of ["TabOpen", "TabClose"]) {
    // Pretend we just synced.
    await tracker.clearChangedIDs();
    Assert.ok(!tracker.modified);

    // Send a fake tab event
    tracker.onTab({
      type: evttype,
      originalTarget: evttype,
      target: { entries: [], currentURI: "about:config" },
    });
    Assert.ok(tracker.modified);
    Assert.ok(
      Utils.deepEquals(Object.keys(await engine.getChangedIDs()), [
        clientsEngine.localID,
      ])
    );
  }

  // Pretend we just synced.
  await tracker.clearChangedIDs();
  Assert.ok(!tracker.modified);

  tracker.onTab({
    type: "TabOpen",
    originalTarget: "TabOpen",
    target: { entries: [], currentURI: "about:config" },
  });
  Assert.ok(
    Utils.deepEquals(Object.keys(await engine.getChangedIDs()), [
      clientsEngine.localID,
    ])
  );

  // Pretend we just synced and saw some progress listeners.
  await tracker.clearChangedIDs();
  Assert.ok(!tracker.modified);
  tracker.onLocationChange({ isTopLevel: false }, undefined, undefined, 0);
  Assert.ok(!tracker.modified, "non-toplevel request didn't flag as modified");

  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("https://www.mozilla.org"),
    Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT
  );
  Assert.ok(
    !tracker.modified,
    "location change within the same document request didn't flag as modified"
  );

  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("https://www.mozilla.org")
  );
  Assert.ok(
    tracker.modified,
    "location change for a new top-level document flagged as modified"
  );
  Assert.ok(
    Utils.deepEquals(Object.keys(await engine.getChangedIDs()), [
      clientsEngine.localID,
    ])
  );
});

add_task(async function run_sync_on_tab_change_test() {
  let { manager } = await setupForExperimentFeature();
  await manager.onStartup();
  await ExperimentAPI.ready();

  let testExperimentDelay = 20000;
  let testPrefDelay = 5000;

  // This is the fallback pref if we don't have a experiment running
  Svc.Prefs.set("syncedTabs.syncDelayAfterTabChange", testPrefDelay);
  // We should only be syncing on tab change if
  // the user has > 1 client
  Svc.Prefs.set("clients.devices.desktop", 1);
  Svc.Prefs.set("clients.devices.mobile", 1);
  scheduler.updateClientMode();
  Assert.equal(scheduler.numClients, 2);

  let doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      enabled: true,
      featureId: "syncAfterTabChange",
      value: {
        syncDelayAfterTabChange: testExperimentDelay,
        syncDelayAfterTabChangeOverride: true,
      },
    },
    {
      manager,
    }
  );
  Assert.ok(
    manager.store.getExperimentForFeature("syncAfterTabChange"),
    "Should be enrolled in the experiment"
  );

  let engine = Service.engineManager.get("tabs");

  _("We assume that tabs have changed at startup.");
  let tracker = engine._tracker;
  tracker.getTabState = fakeGetTabState;

  Assert.ok(tracker.modified);
  Assert.ok(
    Utils.deepEquals(Object.keys(await engine.getChangedIDs()), [
      clientsEngine.localID,
    ])
  );

  _("Test sync is scheduled after a tab change if experiment is enabled");
  for (let evttype of ["TabOpen", "TabClose"]) {
    // Pretend we just synced
    await tracker.clearChangedIDs();
    clearQuickWriteTimer(tracker);

    // Send a fake tab event
    tracker.onTab({
      type: evttype,
      originalTarget: evttype,
      target: { entries: [], currentURI: "about:config" },
    });
    // Ensure the tracker fired
    Assert.ok(tracker.modified);
    // We should be more delayed than the pref value because the experiment
    // is enabled
    let nextSchedule = tracker.tabsQuickWriteTimer.delay;
    Assert.ok(
      nextSchedule <= testExperimentDelay && nextSchedule > testPrefDelay
    );
  }

  _("Test sync is NOT scheduled after an unsupported tab open");
  for (let evttype of ["TabOpen"]) {
    // Send a fake tab event
    tracker.onTab({
      type: evttype,
      originalTarget: evttype,
      target: { entries: ["about:newtab"], currentURI: null },
    });
    // Ensure the tracker fired
    Assert.ok(tracker.modified);
    // We should be scheduling <= experiment value
    Assert.ok(scheduler.nextSync - Date.now() <= testExperimentDelay);
  }

  _("Test navigating within the same tab does NOT trigger a sync");
  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);

  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("https://www.mozilla.org"),
    Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD
  );
  Assert.ok(
    !tracker.modified,
    "location change for reloading doesn't trigger a sync"
  );
  Assert.ok(!tracker.tabsQuickWriteTimer, "reload does not trigger a sync");

  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);

  _("Test navigating to an about page does trigger sync");
  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("about:config")
  );
  Assert.ok(tracker.modified, "about page does not trigger a tab modified");
  Assert.ok(
    tracker.tabsQuickWriteTimer,
    "about schema should trigger a sync happening soon"
  );

  _("Test adjusting the filterScheme pref works");
  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);

  Svc.Prefs.set(
    "engine.tabs.filteredSchemes",
    // Removing the about scheme for this test
    "resource|chrome|file|blob|moz-extension"
  );
  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("about:config")
  );
  Assert.ok(
    tracker.modified,
    "about page triggers a modified after we changed the pref"
  );
  Assert.ok(
    tracker.tabsQuickWriteTimer,
    "about page should schedule a quickWrite sync soon after we changed the pref"
  );

  _("Test no sync after tab change for accounts with <= 1 clients");
  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);
  // Setting clients to only 1 so we don't sync after a tab change
  Svc.Prefs.set("clients.devices.desktop", 1);
  Svc.Prefs.set("clients.devices.mobile", 0);
  scheduler.updateClientMode();
  Assert.equal(scheduler.numClients, 1);

  tracker.onLocationChange(
    { isTopLevel: true },
    undefined,
    Services.io.newURI("https://www.mozilla.org")
  );
  Assert.ok(
    tracker.modified,
    "location change for a new top-level document flagged as modified"
  );
  Assert.ok(
    !tracker.tabsQuickWriteTimer,
    "We should NOT be syncing shortly because there is only one client"
  );

  await doEnrollmentCleanup();

  _("If there is no experiment, fallback to the pref");
  let delayPref = Svc.Prefs.get("syncedTabs.syncDelayAfterTabChange");
  let evttype = "TabOpen";
  Assert.equal(delayPref, testPrefDelay);
  // Only have task continuity if we have more than 1 device
  Svc.Prefs.set("clients.devices.desktop", 1);
  Svc.Prefs.set("clients.devices.mobile", 1);
  scheduler.updateClientMode();
  Assert.equal(scheduler.numClients, 2);
  clearQuickWriteTimer(tracker);

  // Fire ontab event
  tracker.onTab({
    type: evttype,
    originalTarget: evttype,
    target: { entries: [], currentURI: "about:config" },
  });

  // Ensure the tracker fired
  Assert.ok(tracker.modified);
  // We should be scheduling <= preference value
  Assert.equal(tracker.tabsQuickWriteTimer.delay, delayPref);

  _("We should not have a sync if experiment if off and pref is 0");

  Svc.Prefs.set("syncedTabs.syncDelayAfterTabChange", 0);
  let doEnrollmentCleanup2 = await ExperimentFakes.enrollWithFeatureConfig(
    {
      enabled: true,
      featureId: "syncAfterTabChange",
      value: {
        syncDelayAfterTabChange: 0,
        syncDelayAfterTabChangeOverride: true,
      },
    },
    {
      manager,
    }
  );
  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);

  // Fire ontab event
  evttype = "TabOpen";
  tracker.onTab({
    type: evttype,
    originalTarget: evttype,
    target: { entries: [], currentURI: "about:config" },
  });
  // Ensure the tracker fired
  Assert.ok(tracker.modified);

  // We should NOT be scheduled for a sync soon
  Assert.ok(!tracker.tabsQuickWriteTimer);

  // cleanup
  await doEnrollmentCleanup2();

  _("If the experiment is on but override is off, we use the pref value");

  Svc.Prefs.set("syncedTabs.syncDelayAfterTabChange", testPrefDelay);
  let doEnrollmentCleanup3 = await ExperimentFakes.enrollWithFeatureConfig(
    {
      enabled: true,
      featureId: "syncAfterTabChange",
      value: {
        syncDelayAfterTabChange: 500000,
        syncDelayAfterTabChangeOverride: false,
      },
    },
    {
      manager,
    }
  );
  // Pretend we just synced
  await tracker.clearChangedIDs();
  clearQuickWriteTimer(tracker);

  // Fire ontab event
  evttype = "TabOpen";
  tracker.onTab({
    type: evttype,
    originalTarget: evttype,
    target: { entries: [], currentURI: "about:config" },
  });
  // Ensure the tracker fired
  Assert.ok(tracker.modified);

  // We should NOT be scheduled for a sync soon
  Assert.ok(tracker.tabsQuickWriteTimer);
  Assert.equal(tracker.tabsQuickWriteTimer.delay, testPrefDelay);

  // cleanup
  await doEnrollmentCleanup3();

  scheduler.setDefaults();
  Svc.Prefs.resetBranch("");
});
