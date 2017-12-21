/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

let clientsEngine;

add_task(async function setup() {
  await Service.promiseInitialized;
  clientsEngine = Service.clientsEngine;
});

function fakeSvcWinMediator() {
  // actions on windows are captured in logs
  let logs = [];
  delete Services.wm;
  Services.wm = {
    getEnumerator() {
      return {
        cnt: 2,
        hasMoreElements() {
          return this.cnt-- > 0;
        },
        getNext() {
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
      };
    }
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
  Svc.Obs.notify("weave:engine:start-tracking");
  Assert.equal(logs.length, 2);
  for (let log of logs) {
    Assert.equal(log.addTopics.length, 5);
    Assert.ok(log.addTopics.indexOf("pageshow") >= 0);
    Assert.ok(log.addTopics.indexOf("TabOpen") >= 0);
    Assert.ok(log.addTopics.indexOf("TabClose") >= 0);
    Assert.ok(log.addTopics.indexOf("TabSelect") >= 0);
    Assert.ok(log.addTopics.indexOf("unload") >= 0);
    Assert.equal(log.remTopics.length, 0);
    Assert.equal(log.numAPL, 1, "Added 1 progress listener");
    Assert.equal(log.numRPL, 0, "Didn't remove a progress listener");
  }

  _("Test listeners are unregistered on windows");
  logs = fakeSvcWinMediator();
  Svc.Obs.notify("weave:engine:stop-tracking");
  Assert.equal(logs.length, 2);
  for (let log of logs) {
    Assert.equal(log.addTopics.length, 0);
    Assert.equal(log.remTopics.length, 5);
    Assert.ok(log.remTopics.indexOf("pageshow") >= 0);
    Assert.ok(log.remTopics.indexOf("TabOpen") >= 0);
    Assert.ok(log.remTopics.indexOf("TabClose") >= 0);
    Assert.ok(log.remTopics.indexOf("TabSelect") >= 0);
    Assert.ok(log.remTopics.indexOf("unload") >= 0);
    Assert.equal(log.numAPL, 0, "Didn't add a progress listener");
    Assert.equal(log.numRPL, 1, "Removed 1 progress listener");
  }

  _("Test tab listener");
  for (let evttype of ["TabOpen", "TabClose", "TabSelect"]) {
    // Pretend we just synced.
    tracker.clearChangedIDs();
    Assert.ok(!tracker.modified);

    // Send a fake tab event
    tracker.onTab({type: evttype, originalTarget: evttype});
    Assert.ok(tracker.modified);
    Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                               [clientsEngine.localID]));
  }

  // Pretend we just synced.
  tracker.clearChangedIDs();
  Assert.ok(!tracker.modified);

  tracker.onTab({type: "pageshow", originalTarget: "pageshow"});
  Assert.ok(Utils.deepEquals(Object.keys((await engine.getChangedIDs())),
                             [clientsEngine.localID]));

  // Pretend we just synced and saw some progress listeners.
  tracker.clearChangedIDs();
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
