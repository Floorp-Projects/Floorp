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

function run_test() {
  Engines.register(HistoryEngine);
  let tracker = Engines.get("history")._tracker;

  _("Verify we've got an empty tracker to work with.");
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);

  let _counter = 0;
  function addVisit() {
    Svc.History.addVisit(Utils.makeURI("http://getfirefox.com/" + _counter),
                         Date.now() * 1000, null, 1, false, 0);
    _counter += 1;
  }

  do_test_pending();
  asyncChainTests(function (next) {

    _("Create history item. Won't show because we haven't started tracking yet");
    addVisit();
    Utils.delay(function() {
      do_check_eq([id for (id in tracker.changedIDs)].length, 0);
      next();
    }, 0);

  }, function (next) {

    _("Tell the tracker to start tracking changes.");
    onScoreUpdated(function() {
      do_check_eq([id for (id in tracker.changedIDs)].length, 1);
      next();
    });
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();

  }, function (next) {

    _("Notifying twice won't do any harm.");
    onScoreUpdated(function() {
      do_check_eq([id for (id in tracker.changedIDs)].length, 2);
      next();
    });
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();

  }, function (next) {

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    Utils.delay(function() {
      do_check_eq([id for (id in tracker.changedIDs)].length, 0);
      next();
    }, 0);

  }, function (next) {

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    Utils.delay(function() {
      do_check_eq([id for (id in tracker.changedIDs)].length, 0);
      next();
    }, 0);

  }, function (next) {

    _("Clean up.");
    Svc.History.removeAllPages();
    do_test_finished();

  })();
}
