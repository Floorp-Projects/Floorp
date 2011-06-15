Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/history.js");
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

Engines.register(HistoryEngine);
let engine = Engines.get("history");
let tracker = engine._tracker;

let _counter = 0;
function addVisit() {
  PlacesUtils.history.addVisit(
    Utils.makeURI("http://getfirefox.com/" + _counter),
    Date.now() * 1000, null, 1, false, 0);
  _counter += 1;
}


function run_test() {
  run_next_test();
}

add_test(function test_empty() {
  _("Verify we've got an empty tracker to work with.");
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  run_next_test();
});

add_test(function test_not_tracking(next) {
  _("Create history item. Won't show because we haven't started tracking yet");
  addVisit();
  Utils.nextTick(function() {
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
    run_next_test();
  });
});

add_test(function test_start_tracking() {
  _("Tell the tracker to start tracking changes.");
  onScoreUpdated(function() {
    do_check_eq([id for (id in tracker.changedIDs)].length, 1);
    run_next_test();
  });
  Svc.Obs.notify("weave:engine:start-tracking");
  addVisit();
});

add_test(function test_start_tracking_twice() {
  _("Notifying twice won't do any harm.");
  onScoreUpdated(function() {
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);
    run_next_test();
  });
  Svc.Obs.notify("weave:engine:start-tracking");
  addVisit();
});

add_test(function test_track_delete() {
  _("Deletions are tracked.");
  let uri = Utils.makeURI("http://getfirefox.com/0");
  let guid = engine._store.GUIDForUri(uri);
  do_check_false(guid in tracker.changedIDs);

  onScoreUpdated(function() {
    do_check_true(guid in tracker.changedIDs);
    do_check_eq([id for (id in tracker.changedIDs)].length, 3);
    run_next_test();
  });
  PlacesUtils.history.removePage(uri);
});

add_test(function test_stop_tracking() {
  _("Let's stop tracking again.");
  tracker.clearChangedIDs();
  Svc.Obs.notify("weave:engine:stop-tracking");
  addVisit();
  Utils.nextTick(function() {
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
    run_next_test();
  });
});

add_test(function test_stop_tracking_twice() {
  _("Notifying twice won't do any harm.");
  Svc.Obs.notify("weave:engine:stop-tracking");
  addVisit();
  Utils.nextTick(function() {
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
    run_next_test();
  });
});

add_test(function cleanup() {
   _("Clean up.");
  PlacesUtils.history.removeAllPages();
  run_next_test();
});
