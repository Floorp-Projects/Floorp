/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.import("resource://services-sync/engines/tabs.js");
ChromeUtils.import("resource://services-sync/service.js");
ChromeUtils.import("resource://services-sync/util.js");

let clientsEngine;

add_task(async function setup() {
  await Service.promiseInitialized;
  clientsEngine = Service.clientsEngine;
});

function fakeSvcWinMediator() {
  // actions on windows are captured in logs
  let logs = [];
  delete Services.wm;

  function getNext() {
    let elt = {addTopics: [], remTopics: [], numAPL: 0, numRPL: 0};
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

add_task(async function run_test() {
  let engine = Service.engineManager.get("tabs");

  _("We assume that tabs have changed at startup.");
  let tracker = engine._tracker;
  tracker.persistChangedIDs = false;

  Assert.ok(tracker.modified);
  Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                             [clientsEngine.localID]));

  let logs;

  _("Test listeners are registered on windows");
  logs = fakeSvcWinMediator();
  tracker.start();
  Assert.equal(logs.length, 2);
  for (let log of logs) {
    Assert.equal(log.addTopics.length, 5);
    Assert.ok(log.addTopics.includes("pageshow"));
    Assert.ok(log.addTopics.includes("TabOpen"));
    Assert.ok(log.addTopics.includes("TabClose"));
    Assert.ok(log.addTopics.includes("TabSelect"));
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
    Assert.equal(log.remTopics.length, 5);
    Assert.ok(log.remTopics.includes("pageshow"));
    Assert.ok(log.remTopics.includes("TabOpen"));
    Assert.ok(log.remTopics.includes("TabClose"));
    Assert.ok(log.remTopics.includes("TabSelect"));
    Assert.ok(log.remTopics.includes("unload"));
    Assert.equal(log.numAPL, 0, "Didn't add a progress listener");
    Assert.equal(log.numRPL, 1, "Removed 1 progress listener");
  }

  _("Test tab listener");
  for (let evttype of ["TabOpen", "TabClose", "TabSelect"]) {
    // Pretend we just synced.
    await tracker.clearChangedIDs();
    Assert.ok(!tracker.modified);

    // Send a fake tab event
    tracker.onTab({type: evttype, originalTarget: evttype});
    Assert.ok(tracker.modified);
    Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                               [clientsEngine.localID]));
  }

  // Pretend we just synced.
  await tracker.clearChangedIDs();
  Assert.ok(!tracker.modified);

  tracker.onTab({type: "pageshow", originalTarget: "pageshow"});
  Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                             [clientsEngine.localID]));

  // Pretend we just synced and saw some progress listeners.
  await tracker.clearChangedIDs();
  Assert.ok(!tracker.modified);
  tracker.onLocationChange({ isTopLevel: false }, undefined, undefined, 0);
  Assert.ok(!tracker.modified, "non-toplevel request didn't flag as modified");

  tracker.onLocationChange({ isTopLevel: true }, undefined, undefined,
                           Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
  Assert.ok(!tracker.modified, "location change within the same document request didn't flag as modified");

  tracker.onLocationChange({ isTopLevel: true }, undefined, undefined, 0);
  Assert.ok(tracker.modified, "location change for a new top-level document flagged as modified");
  Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                             [clientsEngine.localID]));
});
