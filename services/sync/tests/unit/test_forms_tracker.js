Cu.import("resource://services-sync/engines/forms.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/log4moz.js");

function run_test() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = new FormEngine()._tracker;
  do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  try {
    _("Create an entry. Won't show because we haven't started tracking yet");
    Svc.Form.addEntry("name", "John Doe");
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    Svc.Form.removeEntry("name", "John Doe");
    Svc.Form.addEntry("email", "john@doe.com");
    do_check_eq([id for (id in tracker.changedIDs)].length, 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    Svc.Form.addEntry("address", "Memory Lane");
    do_check_eq([id for (id in tracker.changedIDs)].length, 3);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    Svc.Form.removeEntry("address", "Memory Lane");
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    Svc.Form.removeEntry("email", "john@doe.com");
    do_check_eq([id for (id in tracker.changedIDs)].length, 0);
  
    _("Test error detection.");
    // This throws an exception without the fix for Bug 597400.
    tracker.trackEntry("foo", "bar");
    
  } finally {
    _("Clean up.");
    Svc.Form.removeAllEntries();
  }
}
