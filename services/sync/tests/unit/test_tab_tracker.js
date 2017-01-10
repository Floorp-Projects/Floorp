/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/tabs.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

var clientsEngine = Service.clientsEngine;

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

function run_test() {
  let engine = Service.engineManager.get("tabs");

  _("We assume that tabs have changed at startup.");
  let tracker = engine._tracker;
  tracker.persistChangedIDs = false;

  do_check_true(tracker.modified);
  do_check_true(Utils.deepEquals(Object.keys(engine.getChangedIDs()),
                                 [clientsEngine.localID]));

  let logs;

  _("Test listeners are registered on windows");
  logs = fakeSvcWinMediator();
  Svc.Obs.notify("weave:engine:start-tracking");
  do_check_eq(logs.length, 2);
  for (let log of logs) {
    do_check_eq(log.addTopics.length, 5);
    do_check_true(log.addTopics.indexOf("pageshow") >= 0);
    do_check_true(log.addTopics.indexOf("TabOpen") >= 0);
    do_check_true(log.addTopics.indexOf("TabClose") >= 0);
    do_check_true(log.addTopics.indexOf("TabSelect") >= 0);
    do_check_true(log.addTopics.indexOf("unload") >= 0);
    do_check_eq(log.remTopics.length, 0);
    do_check_eq(log.numAPL, 1, "Added 1 progress listener");
    do_check_eq(log.numRPL, 0, "Didn't remove a progress listener");
  }

  _("Test listeners are unregistered on windows");
  logs = fakeSvcWinMediator();
  Svc.Obs.notify("weave:engine:stop-tracking");
  do_check_eq(logs.length, 2);
  for (let log of logs) {
    do_check_eq(log.addTopics.length, 0);
    do_check_eq(log.remTopics.length, 5);
    do_check_true(log.remTopics.indexOf("pageshow") >= 0);
    do_check_true(log.remTopics.indexOf("TabOpen") >= 0);
    do_check_true(log.remTopics.indexOf("TabClose") >= 0);
    do_check_true(log.remTopics.indexOf("TabSelect") >= 0);
    do_check_true(log.remTopics.indexOf("unload") >= 0);
    do_check_eq(log.numAPL, 0, "Didn't add a progress listener");
    do_check_eq(log.numRPL, 1, "Removed 1 progress listener");
  }

  _("Test tab listener");
  for (let evttype of ["TabOpen", "TabClose", "TabSelect"]) {
    // Pretend we just synced.
    tracker.clearChangedIDs();
    do_check_false(tracker.modified);

    // Send a fake tab event
    tracker.onTab({type: evttype, originalTarget: evttype});
    do_check_true(tracker.modified);
    do_check_true(Utils.deepEquals(Object.keys(engine.getChangedIDs()),
                                   [clientsEngine.localID]));
  }

  // Pretend we just synced.
  tracker.clearChangedIDs();
  do_check_false(tracker.modified);

  tracker.onTab({type: "pageshow", originalTarget: "pageshow"});
  do_check_true(Utils.deepEquals(Object.keys(engine.getChangedIDs()),
                                 [clientsEngine.localID]));

  // Pretend we just synced and saw some progress listeners.
  tracker.clearChangedIDs();
  do_check_false(tracker.modified);
  tracker.onLocationChange({ isTopLevel: false }, undefined, undefined, 0);
  do_check_false(tracker.modified, "non-toplevel request didn't flag as modified");

  tracker.onLocationChange({ isTopLevel: true }, undefined, undefined,
                           Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT);
  do_check_false(tracker.modified, "location change within the same document request didn't flag as modified");

  tracker.onLocationChange({ isTopLevel: true }, undefined, undefined, 0);
  do_check_true(tracker.modified, "location change for a new top-level document flagged as modified");
  do_check_true(Utils.deepEquals(Object.keys(engine.getChangedIDs()),
                                 [clientsEngine.localID]));
}
