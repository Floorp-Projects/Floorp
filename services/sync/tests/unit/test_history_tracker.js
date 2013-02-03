/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function onScoreUpdated(callback) {
  Svc.Obs.add("weave:engine:score:updated", function observer() {
    Svc.Obs.remove("weave:engine:score:updated", observer);
    try {
      callback();
    } catch (ex) {
      do_throw(ex);
    }
  });
}

Service.engineManager.clear();
Service.engineManager.register(HistoryEngine);
let engine = Service.engineManager.get("history");
let tracker = engine._tracker;

// Don't write out by default.
tracker.persistChangedIDs = false;

let _counter = 0;
function addVisit() {
  let uriString = "http://getfirefox.com/" + _counter++;
  let uri = Utils.makeURI(uriString);
  _("Adding visit for URI " + uriString);
  let place = {
    uri: uri,
    visits: [ {
      visitDate: Date.now() * 1000,
      transitionType: PlacesUtils.history.TRANSITION_LINK
    } ]
  };

  let cb = Async.makeSpinningCallback();
  PlacesUtils.asyncHistory.updatePlaces(place, {
    handleError: function () {
      _("Error adding visit for " + uriString);
      cb(new Error("Error adding history entry"));
    },

    handleResult: function () {
    },

    handleCompletion: function () {
      _("Added visit for " + uriString);
      cb();
    }
  });

  // Spin the event loop to embed this async call in a sync API.
  cb.wait();
  return uri;
}

function run_test() {
  initTestLogging("Trace");
  Log4Moz.repository.getLogger("Sync.Tracker.History").level = Log4Moz.Level.Trace;
  run_next_test();
}

add_test(function test_empty() {
  _("Verify we've got an empty, disabled tracker to work with.");
  do_check_empty(tracker.changedIDs);
  do_check_eq(tracker.score, 0);
  do_check_false(tracker._enabled);
  run_next_test();
});

add_test(function test_not_tracking(next) {
  _("Create history item. Won't show because we haven't started tracking yet");
  addVisit();
  Utils.nextTick(function() {
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);
    run_next_test();
  });
});

add_test(function test_start_tracking() {
  _("Add hook for save completion.");
  tracker.persistChangedIDs = true;
  tracker.onSavedChangedIDs = function () {
    _("changedIDs written to disk. Proceeding.");
    // Turn this back off.
    tracker.persistChangedIDs = false;
    delete tracker.onSavedChangedIDs;
    run_next_test();
  };

  _("Tell the tracker to start tracking changes.");
  onScoreUpdated(function() {
    _("Score updated in test_start_tracking.");
    do_check_attribute_count(tracker.changedIDs, 1);
    do_check_eq(tracker.score, SCORE_INCREMENT_SMALL);
  });

  Svc.Obs.notify("weave:engine:start-tracking");
  addVisit();
});

add_test(function test_start_tracking_twice() {
  _("Verifying preconditions from test_start_tracking.");
  do_check_attribute_count(tracker.changedIDs, 1);
  do_check_eq(tracker.score, SCORE_INCREMENT_SMALL);

  _("Notifying twice won't do any harm.");
  onScoreUpdated(function() {
    _("Score updated in test_start_tracking_twice.");
    do_check_attribute_count(tracker.changedIDs, 2);
    do_check_eq(tracker.score, 2 * SCORE_INCREMENT_SMALL);
    run_next_test();
  });

  Svc.Obs.notify("weave:engine:start-tracking");
  addVisit();
});

add_test(function test_track_delete() {
  _("Deletions are tracked.");

  // This isn't present because we weren't tracking when it was visited.
  let uri = Utils.makeURI("http://getfirefox.com/0");
  let guid = engine._store.GUIDForUri(uri);
  do_check_false(guid in tracker.changedIDs);

  onScoreUpdated(function() {
    do_check_true(guid in tracker.changedIDs);
    do_check_attribute_count(tracker.changedIDs, 3);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE + 2 * SCORE_INCREMENT_SMALL);
    run_next_test();
  });

  do_check_eq(tracker.score, 2 * SCORE_INCREMENT_SMALL);
  PlacesUtils.history.removePage(uri);
});

add_test(function test_dont_track_expiration() {
  _("Expirations are not tracked.");
  let uriToExpire = addVisit();
  let guidToExpire = engine._store.GUIDForUri(uriToExpire);
  let uriToRemove = addVisit();
  let guidToRemove = engine._store.GUIDForUri(uriToRemove);

  tracker.clearChangedIDs();
  do_check_false(guidToExpire in tracker.changedIDs);
  do_check_false(guidToRemove in tracker.changedIDs);

  onScoreUpdated(function() {
    do_check_false(guidToExpire in tracker.changedIDs);
    do_check_true(guidToRemove in tracker.changedIDs);
    do_check_attribute_count(tracker.changedIDs, 1);
    run_next_test();
  });

  // Observe expiration.
  Services.obs.addObserver(function onExpiration(aSubject, aTopic, aData) {
    Services.obs.removeObserver(onExpiration, aTopic);
    // Remove the remaining page to update its score.
    PlacesUtils.history.removePage(uriToRemove);
  }, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

  // Force expiration of 1 entry.
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);
  Cc["@mozilla.org/places/expiration;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "places-debug-start-expiration", 1);
});

add_test(function test_stop_tracking() {
  _("Let's stop tracking again.");
  tracker.clearChangedIDs();
  Svc.Obs.notify("weave:engine:stop-tracking");
  addVisit();
  Utils.nextTick(function() {
    do_check_empty(tracker.changedIDs);
    run_next_test();
  });
});

add_test(function test_stop_tracking_twice() {
  _("Notifying twice won't do any harm.");
  Svc.Obs.notify("weave:engine:stop-tracking");
  addVisit();
  Utils.nextTick(function() {
    do_check_empty(tracker.changedIDs);
    run_next_test();
  });
});

add_test(function cleanup() {
   _("Clean up.");
  PlacesUtils.history.removeAllPages();
  run_next_test();
});
