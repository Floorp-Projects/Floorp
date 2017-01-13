/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");


function checkRecord(name, record, expectedCount, timeCreated,
                     expectedTimeCreated, timePasswordChanged,
                     expectedTimePasswordChanged, recordIsUpdated) {
  let engine = Service.engineManager.get("passwords");
  let store = engine._store;

  let count = {};
  let logins = Services.logins.findLogins(count, record.hostname,
                                          record.formSubmitURL, null);

  _("Record" + name + ":" + JSON.stringify(logins));
  _("Count" + name + ":" + count.value);

  do_check_eq(count.value, expectedCount);

  if (expectedCount > 0) {
    do_check_true(!!store.getAllIDs()[record.id]);
    let stored_record = logins[0].QueryInterface(Ci.nsILoginMetaInfo);

    if (timeCreated !== undefined) {
      do_check_eq(stored_record.timeCreated, expectedTimeCreated);
    }

    if (timePasswordChanged !== undefined) {
      if (recordIsUpdated) {
        do_check_true(stored_record.timePasswordChanged >= expectedTimePasswordChanged);
      } else {
        do_check_eq(stored_record.timePasswordChanged, expectedTimePasswordChanged);
      }
      return stored_record.timePasswordChanged;
    }
  } else {
    do_check_true(!store.getAllIDs()[record.id]);
  }
}


function changePassword(name, hostname, password, expectedCount, timeCreated,
                        expectedTimeCreated, timePasswordChanged,
                        expectedTimePasswordChanged, insert, recordIsUpdated) {

  const BOGUS_GUID = "zzzzzz" + hostname;

  let record = {id: BOGUS_GUID,
                  hostname,
                  formSubmitURL: hostname,
                  username: "john",
                  password,
                  usernameField: "username",
                  passwordField: "password"};

  if (timeCreated !== undefined) {
    record.timeCreated = timeCreated;
  }

  if (timePasswordChanged !== undefined) {
    record.timePasswordChanged = timePasswordChanged;
  }


  let engine = Service.engineManager.get("passwords");
  let store = engine._store;

  if (insert) {
    do_check_eq(store.applyIncomingBatch([record]).length, 0);
  }

  return checkRecord(name, record, expectedCount, timeCreated,
                     expectedTimeCreated, timePasswordChanged,
                     expectedTimePasswordChanged, recordIsUpdated);

}


function test_apply_records_with_times(hostname, timeCreated, timePasswordChanged) {
  // The following record is going to be inserted in the store and it needs
  // to be found there. Then its timestamps are going to be compared to
  // the expected values.
  changePassword(" ", hostname, "password", 1, timeCreated, timeCreated,
                 timePasswordChanged, timePasswordChanged, true);
}


function test_apply_multiple_records_with_times() {
  // The following records are going to be inserted in the store and they need
  // to be found there. Then their timestamps are going to be compared to
  // the expected values.
  changePassword("A", "http://foo.a.com", "password", 1, undefined, undefined,
                 undefined, undefined, true);
  changePassword("B", "http://foo.b.com", "password", 1, 1000, 1000, undefined,
                 undefined, true);
  changePassword("C", "http://foo.c.com", "password", 1, undefined, undefined,
                 1000, 1000, true);
  changePassword("D", "http://foo.d.com", "password", 1, 1000, 1000, 1000,
                 1000, true);

  // The following records are not going to be inserted in the store and they
  // are not going to be found there.
  changePassword("NotInStoreA", "http://foo.aaaa.com", "password", 0,
                 undefined, undefined, undefined, undefined, false);
  changePassword("NotInStoreB", "http://foo.bbbb.com", "password", 0, 1000,
                 1000, undefined, undefined, false);
  changePassword("NotInStoreC", "http://foo.cccc.com", "password", 0,
                 undefined, undefined, 1000, 1000, false);
  changePassword("NotInStoreD", "http://foo.dddd.com", "password", 0, 1000,
                 1000, 1000, 1000, false);
}


function test_apply_same_record_with_different_times() {
  // The following record is going to be inserted multiple times in the store
  // and it needs to be found there. Then its timestamps are going to be
  // compared to the expected values.
  var timePasswordChanged = 100;
  timePasswordChanged = changePassword("A", "http://a.tn", "password", 1, 100,
                                       100, 100, timePasswordChanged, true);
  timePasswordChanged = changePassword("A", "http://a.tn", "password", 1, 100,
                                       100, 800, timePasswordChanged, true,
                                       true);
  timePasswordChanged = changePassword("A", "http://a.tn", "password", 1, 500,
                                       100, 800, timePasswordChanged, true,
                                       true);
  timePasswordChanged = changePassword("A", "http://a.tn", "password2", 1, 500,
                                       100, 1536213005222, timePasswordChanged,
                                       true, true);
  timePasswordChanged = changePassword("A", "http://a.tn", "password2", 1, 500,
                                       100, 800, timePasswordChanged, true, true);
}


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

  try {
    do_check_eq(store.applyIncomingBatch([recordA, recordB]).length, 0);

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

    test_apply_records_with_times("http://afoo.baz.com", undefined, undefined);
    test_apply_records_with_times("http://bfoo.baz.com", 1000, undefined);
    test_apply_records_with_times("http://cfoo.baz.com", undefined, 2000);
    test_apply_records_with_times("http://dfoo.baz.com", 1000, 2000);

    test_apply_multiple_records_with_times();

    test_apply_same_record_with_different_times();

  } finally {
    store.wipe();
  }
}
