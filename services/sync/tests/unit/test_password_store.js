/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");

function run_test() {
  initTestLogging("Trace");
  Log.repository.getLogger("Sync.Engine.Passwords").level = Log.Level.Trace;
  Log.repository.getLogger("Sync.Store.Passwords").level = Log.Level.Trace;

  const BOGUS_GUID_A = "zzzzzzzzzzzz";
  const BOGUS_GUID_B = "yyyyyyyyyyyy";
  let recordA = {id: BOGUS_GUID_A,
                  hostname: "http://foo.bar.com",
                  formSubmitURL: "http://foo.bar.com/baz",
                  httpRealm: "secure",
                  username: "john",
                  password: "smith",
                  usernameField: "username",
                  passwordField: "password"};
  let recordB = {id: BOGUS_GUID_B,
                  hostname: "http://foo.baz.com",
                  formSubmitURL: "http://foo.baz.com/baz",
                  username: "john",
                  password: "smith",
                  usernameField: "username",
                  passwordField: "password"};

  let engine = Service.engineManager.get("passwords");
  let store = engine._store;
  function applyEnsureNoFailures(records) {
    do_check_eq(store.applyIncomingBatch(records).length, 0);
  }

  try {
    applyEnsureNoFailures([recordA, recordB]);

    // Only the good record makes it to Services.logins.
    let badCount = {};
    let goodCount = {};
    let badLogins = Services.logins.findLogins(badCount, recordA.hostname,
                                               recordA.formSubmitURL,
                                               recordA.httpRealm);
    let goodLogins = Services.logins.findLogins(goodCount, recordB.hostname,
                                                recordB.formSubmitURL, null);

    _("Bad: " + JSON.stringify(badLogins));
    _("Good: " + JSON.stringify(goodLogins));
    _("Count: " + badCount.value + ", " + goodCount.value);

    do_check_eq(goodCount.value, 1);
    do_check_eq(badCount.value, 0);

    do_check_true(!!store.getAllIDs()[BOGUS_GUID_B]);
    do_check_true(!store.getAllIDs()[BOGUS_GUID_A]);
  } finally {
    store.wipe();
  }
}
