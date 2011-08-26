Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/constants.js");

Engines.register(PasswordEngine);
let engine = Engines.get("passwords");
let store  = engine._store;

function test_tracking() {
  let recordNum = 0;

  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
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
  }
}

function test_onWipe() {
  _("Verify we've got an empty tracker to work with.");
  let tracker = engine._tracker;
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
  }
}

function run_test() {
  initTestLogging("Trace");

  Log4Moz.repository.getLogger("Sync.Engine.Passwords").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.Store.Passwords").level = Log4Moz.Level.Trace;
  Log4Moz.repository.getLogger("Sync.Tracker.Passwords").level = Log4Moz.Level.Trace;

  test_tracking();
  test_onWipe();
}
