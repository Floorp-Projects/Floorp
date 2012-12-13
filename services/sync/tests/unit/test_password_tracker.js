/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

Service.engineManager.register(PasswordEngine);
let engine = Service.engineManager.get("passwords");
let store  = engine._store;
let tracker = engine._tracker;

// Don't do asynchronous writes.
tracker.persistChangedIDs = false;

function run_test() {
  initTestLogging("Trace");
  run_next_test();
}

add_test(function test_tracking() {
  let recordNum = 0;

  _("Verify we've got an empty tracker to work with.");
  do_check_empty(tracker.changedIDs);

  function createPassword() {
    _("RECORD NUM: " + recordNum);
    let record = {id: "GUID" + recordNum,
                  hostname: "http://foo.bar.com",
                  formSubmitURL: "http://foo.bar.com/baz",
                  username: "john" + recordNum,
                  password: "smith",
                  usernameField: "username",
                  passwordField: "password"};
    recordNum++;
    let login = store._nsLoginInfoFromRecord(record);
    Services.logins.addLogin(login);
  }

  try {
    _("Create a password record. Won't show because we haven't started tracking yet");
    createPassword();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

    _("Tell the tracker to start tracking changes.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createPassword();
    do_check_attribute_count(tracker.changedIDs, 1);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:start-tracking");
    createPassword();
    do_check_attribute_count(tracker.changedIDs, 2);
    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE * 2);

    _("Let's stop tracking again.");
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
    createPassword();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

    _("Notifying twice won't do any harm.");
    Svc.Obs.notify("weave:engine:stop-tracking");
    createPassword();
    do_check_empty(tracker.changedIDs);
    do_check_eq(tracker.score, 0);

  } finally {
    _("Clean up.");
    store.wipe();
    tracker.clearChangedIDs();
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
    run_next_test();
  }
});

add_test(function test_onWipe() {
  _("Verify we've got an empty tracker to work with.");
  do_check_empty(tracker.changedIDs);
  do_check_eq(tracker.score, 0);

  try {
    _("A store wipe should increment the score");
    Svc.Obs.notify("weave:engine:start-tracking");
    store.wipe();

    do_check_eq(tracker.score, SCORE_INCREMENT_XLARGE);
  } finally {
    tracker.resetScore();
    Svc.Obs.notify("weave:engine:stop-tracking");
    run_next_test();
  }
});
