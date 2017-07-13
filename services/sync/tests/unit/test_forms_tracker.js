/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/engines/forms.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

add_task(async function run_test() {
  _("Verify we've got an empty tracker to work with.");
  let engine = new FormEngine(Service);
  await engine.initialize();
  let tracker = engine._tracker;
  // Don't do asynchronous writes.
  tracker.persistChangedIDs = false;

  do_check_empty(tracker.changedIDs);
  Log.repository.rootLogger.addAppender(new Log.DumpAppender());

  async function addEntry(name, value) {
    await engine._store.create({name, value});
  }
  async function removeEntry(name, value) {
    let guid = await engine._findDupe({name, value});
    await engine._store.remove({id: guid});
  }

  try {
    _("Create an entry. Won't show because we haven't started tracking yet");
    await addEntry("name", "John Doe");
    do_check_empty(tracker.changedIDs);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    await removeEntry("name", "John Doe");
    await addEntry("email", "john@doe.com");
    do_check_attribute_count(tracker.changedIDs, 2);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    await addEntry("address", "Memory Lane");
    do_check_attribute_count(tracker.changedIDs, 3);


    _("Check that ignoreAll is respected");
    tracker.clearChangedIDs();
    tracker.score = 0;
    tracker.ignoreAll = true;
    await addEntry("username", "johndoe123");
    await addEntry("favoritecolor", "green");
    await removeEntry("name", "John Doe");
    tracker.ignoreAll = false;
    do_check_empty(tracker.changedIDs);
    equal(tracker.score, 0);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    Svc.Obs.notify("weave:engine:stop-tracking");
    await removeEntry("address", "Memory Lane");
    do_check_empty(tracker.changedIDs);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    await removeEntry("email", "john@doe.com");
    do_check_empty(tracker.changedIDs);



  } finally {
    _("Clean up.");
    await engine._store.wipe();
  }
});
