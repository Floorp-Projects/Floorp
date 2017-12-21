/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-sync/engines/passwords.js");
Cu.import("resource://services-sync/service.js");
Cu.import("resource://services-sync/util.js");


async function checkRecord(name, record, expectedCount, timeCreated,
                     expectedTimeCreated, timePasswordChanged,
                     expectedTimePasswordChanged, recordIsUpdated) {
  let engine = Service.engineManager.get("passwords");
  let store = engine._store;

  let count = {};
  let logins = Services.logins.findLogins(count, record.hostname,
                                          record.formSubmitURL, null);

  _("Record" + name + ":" + JSON.stringify(logins));
  _("Count" + name + ":" + count.value);

  Assert.equal(count.value, expectedCount);

  if (expectedCount > 0) {
    Assert.ok(!!(await store.getAllIDs())[record.id]);
    let stored_record = logins[0].QueryInterface(Ci.nsILoginMetaInfo);

    if (timeCreated !== undefined) {
      Assert.equal(stored_record.timeCreated, expectedTimeCreated);
    }

    if (timePasswordChanged !== undefined) {
      if (recordIsUpdated) {
        Assert.ok(stored_record.timePasswordChanged >= expectedTimePasswordChanged);
      } else {
        Assert.equal(stored_record.timePasswordChanged, expectedTimePasswordChanged);
      }
      return stored_record.timePasswordChanged;
    }
  } else {
    Assert.ok(!(await store.getAllIDs())[record.id]);
  }
  return undefined;
}


async function changePassword(name, hostname, password, expectedCount, timeCreated,
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
    Assert.equal((await store.applyIncomingBatch([record])).length, 0);
  }

  return checkRecord(name, record, expectedCount, timeCreated,
                     expectedTimeCreated, timePasswordChanged,
                     expectedTimePasswordChanged, recordIsUpdated);

}


async function test_apply_records_with_times(hostname, timeCreated, timePasswordChanged) {
  // The following record is going to be inserted in the store and it needs
  // to be found there. Then its timestamps are going to be compared to
  // the expected values.
  await changePassword(" ", hostname, "password", 1, timeCreated, timeCreated,
                 timePasswordChanged, timePasswordChanged, true);
}


async function test_apply_multiple_records_with_times() {
  // The following records are going to be inserted in the store and they need
  // to be found there. Then their timestamps are going to be compared to
  // the expected values.
  await changePassword("A", "http://foo.a.com", "password", 1, undefined, undefined,
                 undefined, undefined, true);
  await changePassword("B", "http://foo.b.com", "password", 1, 1000, 1000, undefined,
                 undefined, true);
  await changePassword("C", "http://foo.c.com", "password", 1, undefined, undefined,
                 1000, 1000, true);
  await changePassword("D", "http://foo.d.com", "password", 1, 1000, 1000, 1000,
                 1000, true);

  // The following records are not going to be inserted in the store and they
  // are not going to be found there.
  await changePassword("NotInStoreA", "http://foo.aaaa.com", "password", 0,
                 undefined, undefined, undefined, undefined, false);
  await changePassword("NotInStoreB", "http://foo.bbbb.com", "password", 0, 1000,
                 1000, undefined, undefined, false);
  await changePassword("NotInStoreC", "http://foo.cccc.com", "password", 0,
                 undefined, undefined, 1000, 1000, false);
  await changePassword("NotInStoreD", "http://foo.dddd.com", "password", 0, 1000,
                 1000, 1000, 1000, false);
}


async function test_apply_same_record_with_different_times() {
  // The following record is going to be inserted multiple times in the store
  // and it needs to be found there. Then its timestamps are going to be
  // compared to the expected values.

  /* eslint-disable no-unused-vars */
  /* The eslint linter thinks that timePasswordChanged is unused, even though
     it is passed as an argument to changePassword. */
  var timePasswordChanged = 100;
  timePasswordChanged = await changePassword("A", "http://a.tn", "password", 1, 100,
                                       100, 100, timePasswordChanged, true);
  timePasswordChanged = await changePassword("A", "http://a.tn", "password", 1, 100,
                                       100, 800, timePasswordChanged, true,
                                       true);
  timePasswordChanged = await changePassword("A", "http://a.tn", "password", 1, 500,
                                       100, 800, timePasswordChanged, true,
                                       true);
  timePasswordChanged = await changePassword("A", "http://a.tn", "password2", 1, 500,
                                       100, 1536213005222, timePasswordChanged,
                                       true, true);
  timePasswordChanged = await changePassword("A", "http://a.tn", "password2", 1, 500,
                                       100, 800, timePasswordChanged, true, true);
  /* eslint-enable no-unsed-vars */
}

async function test_LoginRec_toString(store, recordData) {
  let rec = await store.createRecord(recordData.id);
  ok(rec);
  ok(!rec.toString().includes(rec.password));
}

add_task(async function run_test() {
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
    Assert.equal((await store.applyIncomingBatch([recordA, recordB])).length, 0);

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

    Assert.equal(goodCount.value, 1);
    Assert.equal(badCount.value, 0);

    Assert.ok(!!(await store.getAllIDs())[BOGUS_GUID_B]);
    Assert.ok(!(await store.getAllIDs())[BOGUS_GUID_A]);

    await test_LoginRec_toString(store, recordB);

    await test_apply_records_with_times("http://afoo.baz.com", undefined, undefined);
    await test_apply_records_with_times("http://bfoo.baz.com", 1000, undefined);
    await test_apply_records_with_times("http://cfoo.baz.com", undefined, 2000);
    await test_apply_records_with_times("http://dfoo.baz.com", 1000, 2000);

    await test_apply_multiple_records_with_times();

    await test_apply_same_record_with_different_times();

  } finally {
    await store.wipe();
  }
});
