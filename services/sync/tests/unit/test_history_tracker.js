Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/engines/history.js");
Cu.import("resource://services-sync/util.js");

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

  try {
    _("Create bookmark. Won't show because we haven't started tracking yet");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 1);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    addVisit();
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  } finally {
    _("Clean up.");
    Svc.History.removeAllPages();
  }
}
