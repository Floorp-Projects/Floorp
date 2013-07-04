/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-sync/engines/forms.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  _("Verify we've got an empty tracker to work with.");
  let engine = new FormEngine(Service);
  let tracker = engine._tracker;
  // Don't do asynchronous writes.
  tracker.persistChangedIDs = false;

  do_check_empty(tracker.changedIDs);
  Log4Moz.repository.rootLogger.addAppender(new Log4Moz.DumpAppender());

  function addEntry(name, value) {
    engine._store.create({name: name, value: value});
  }
  function removeEntry(name, value) {
    guid = engine._findDupe({name: name, value: value});
    engine._store.remove({id: guid});
  }

  try {
    _("Create an entry. Won't show because we haven't started tracking yet");
    addEntry("name", "John Doe");
    do_check_empty(tracker.changedIDs);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    removeEntry("name", "John Doe");
    addEntry("email", "john@doe.com");
    do_check_attribute_count(tracker.changedIDs, 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    addEntry("address", "Memory Lane");
    do_check_attribute_count(tracker.changedIDs, 3);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    removeEntry("address", "Memory Lane");
    do_check_empty(tracker.changedIDs);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    removeEntry("email", "john@doe.com");
    do_check_empty(tracker.changedIDs);

  } finally {
    _("Clean up.");
    engine._store.wipe();
  }
}
