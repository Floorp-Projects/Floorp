/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  run_next_test();
}

add_test(function test_tracker_basics() {
  let tracker = new Tracker("Tracker", Service);
  tracker.persistChangedIDs = false;

  let id = "the_id!";

  _("Make sure nothing exists yet..");
  do_check_eq(tracker.changedIDs[id], null);

  _("Make sure adding of time 0 works");
  tracker.addChangedID(id, 0);
  do_check_eq(tracker.changedIDs[id], 0);

  _("A newer time will replace the old 0");
  tracker.addChangedID(id, 10);
  do_check_eq(tracker.changedIDs[id], 10);

  _("An older time will not replace the newer 10");
  tracker.addChangedID(id, 5);
  do_check_eq(tracker.changedIDs[id], 10);

  _("Adding without time defaults to current time");
  tracker.addChangedID(id);
  do_check_true(tracker.changedIDs[id] > 10);

  run_next_test();
});

add_test(function test_tracker_persistence() {
  let tracker = new Tracker("Tracker", Service);
  let id = "abcdef";

  tracker.persistChangedIDs = true;
  tracker.onSavedChangedIDs = function () {
    _("IDs saved.");
    do_check_eq(5, tracker.changedIDs[id]);

    // Verify the write by reading the file back.
    Utils.jsonLoad("changes/tracker", this, function (json) {
      do_check_eq(5, json[id]);
      tracker.persistChangedIDs = false;
      delete tracker.onSavedChangedIDs;
      run_next_test();
    });
  };

  tracker.addChangedID(id, 5);
});
