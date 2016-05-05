/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for the FxA storage manager.

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FxAccountsStorage.jsm");
Cu.import("resource://gre/modules/FxAccountsCommon.js");
Cu.import("resource://gre/modules/Log.jsm");

initTestLogging("Trace");
log.level = Log.Level.Trace;

const DEVICE_REGISTRATION_VERSION = 42;

// A couple of mocks we can use.
function MockedPlainStorage(accountData) {
  let data = null;
  if (accountData) {
    data = {
      version: DATA_FORMAT_VERSION,
      accountData: accountData,
    }
  }
  this.data = data;
  this.numReads = 0;
}
MockedPlainStorage.prototype = {
  get: Task.async(function* () {
    this.numReads++;
    Assert.equal(this.numReads, 1, "should only ever be 1 read of acct data");
    return this.data;
  }),

  set: Task.async(function* (data) {
    this.data = data;
  }),
};

function MockedSecureStorage(accountData) {
  let data = null;
  if (accountData) {
    data = {
      version: DATA_FORMAT_VERSION,
      accountData: accountData,
    }
  }
  this.data = data;
  this.numReads = 0;
}

MockedSecureStorage.prototype = {
  fetchCount: 0,
  locked: false,
  STORAGE_LOCKED: function() {},
  get: Task.async(function* (uid, email) {
    this.fetchCount++;
    if (this.locked) {
      throw new this.STORAGE_LOCKED();
    }
    this.numReads++;
    Assert.equal(this.numReads, 1, "should only ever be 1 read of unlocked data");
    return this.data;
  }),

  set: Task.async(function* (uid, contents) {
    this.data = contents;
  }),
}

function add_storage_task(testFunction) {
  add_task(function* () {
    print("Starting test with secure storage manager");
    yield testFunction(new FxAccountsStorageManager());
  });
  add_task(function* () {
    print("Starting test with simple storage manager");
    yield testFunction(new FxAccountsStorageManager({useSecure: false}));
  });
}

// initialized without account data and there's nothing to read. Not logged in.
add_storage_task(function* checkInitializedEmpty(sm) {
  if (sm.secureStorage) {
    sm.secureStorage = new MockedSecureStorage(null);
  }
  yield sm.initialize();
  Assert.strictEqual((yield sm.getAccountData()), null);
  Assert.rejects(sm.updateAccountData({kA: "kA"}), "No user is logged in")
});

// Initialized with account data (ie, simulating a new user being logged in).
// Should reflect the initial data and be written to storage.
add_storage_task(function* checkNewUser(sm) {
  let initialAccountData = {
    uid: "uid",
    email: "someone@somewhere.com",
    kA: "kA",
    deviceId: "device id"
  };
  sm.plainStorage = new MockedPlainStorage()
  if (sm.secureStorage) {
    sm.secureStorage = new MockedSecureStorage(null);
  }
  yield sm.initialize(initialAccountData);
  let accountData = yield sm.getAccountData();
  Assert.equal(accountData.uid, initialAccountData.uid);
  Assert.equal(accountData.email, initialAccountData.email);
  Assert.equal(accountData.kA, initialAccountData.kA);
  Assert.equal(accountData.deviceId, initialAccountData.deviceId);

  // and it should have been written to storage.
  Assert.equal(sm.plainStorage.data.accountData.uid, initialAccountData.uid);
  Assert.equal(sm.plainStorage.data.accountData.email, initialAccountData.email);
  Assert.equal(sm.plainStorage.data.accountData.deviceId, initialAccountData.deviceId);
  // check secure
  if (sm.secureStorage) {
    Assert.equal(sm.secureStorage.data.accountData.kA, initialAccountData.kA);
  } else {
    Assert.equal(sm.plainStorage.data.accountData.kA, initialAccountData.kA);
  }
});

// Initialized without account data but storage has it available.
add_storage_task(function* checkEverythingRead(sm) {
  sm.plainStorage = new MockedPlainStorage({
    uid: "uid",
    email: "someone@somewhere.com",
    deviceId: "wibble",
    deviceRegistrationVersion: null
  });
  if (sm.secureStorage) {
    sm.secureStorage = new MockedSecureStorage(null);
  }
  yield sm.initialize();
  let accountData = yield sm.getAccountData();
  Assert.ok(accountData, "read account data");
  Assert.equal(accountData.uid, "uid");
  Assert.equal(accountData.email, "someone@somewhere.com");
  Assert.equal(accountData.deviceId, "wibble");
  Assert.equal(accountData.deviceRegistrationVersion, null);
  // Update the data - we should be able to fetch it back and it should appear
  // in our storage.
  yield sm.updateAccountData({
    verified: true,
    kA: "kA",
    kB: "kB",
    deviceRegistrationVersion: DEVICE_REGISTRATION_VERSION
  });
  accountData = yield sm.getAccountData();
  Assert.equal(accountData.kB, "kB");
  Assert.equal(accountData.kA, "kA");
  Assert.equal(accountData.deviceId, "wibble");
  Assert.equal(accountData.deviceRegistrationVersion, DEVICE_REGISTRATION_VERSION);
  // Check the new value was written to storage.
  yield sm._promiseStorageComplete; // storage is written in the background.
  // "verified", "deviceId" and "deviceRegistrationVersion" are plain-text fields.
  Assert.equal(sm.plainStorage.data.accountData.verified, true);
  Assert.equal(sm.plainStorage.data.accountData.deviceId, "wibble");
  Assert.equal(sm.plainStorage.data.accountData.deviceRegistrationVersion, DEVICE_REGISTRATION_VERSION);
  // "kA" and "foo" are secure
  if (sm.secureStorage) {
    Assert.equal(sm.secureStorage.data.accountData.kA, "kA");
    Assert.equal(sm.secureStorage.data.accountData.kB, "kB");
  } else {
    Assert.equal(sm.plainStorage.data.accountData.kA, "kA");
    Assert.equal(sm.plainStorage.data.accountData.kB, "kB");
  }
});

add_storage_task(function* checkInvalidUpdates(sm) {
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  if (sm.secureStorage) {
    sm.secureStorage = new MockedSecureStorage(null);
  }
  Assert.rejects(sm.updateAccountData({uid: "another"}), "Can't change");
  Assert.rejects(sm.updateAccountData({email: "someoneelse"}), "Can't change");
});

add_storage_task(function* checkNullUpdatesRemovedUnlocked(sm) {
  if (sm.secureStorage) {
    sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
    sm.secureStorage = new MockedSecureStorage({kA: "kA", kB: "kB"});
  } else {
    sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com",
                                              kA: "kA", kB: "kB"});
  }
  yield sm.initialize();

  yield sm.updateAccountData({kA: null});
  let accountData = yield sm.getAccountData();
  Assert.ok(!accountData.kA);
  Assert.equal(accountData.kB, "kB");
});

add_storage_task(function* checkDelete(sm) {
  if (sm.secureStorage) {
    sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
    sm.secureStorage = new MockedSecureStorage({kA: "kA", kB: "kB"});
  } else {
    sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com",
                                              kA: "kA", kB: "kB"});
  }
  yield sm.initialize();

  yield sm.deleteAccountData();
  // Storage should have been reset to null.
  Assert.equal(sm.plainStorage.data, null);
  if (sm.secureStorage) {
    Assert.equal(sm.secureStorage.data, null);
  }
  // And everything should reflect no user.
  Assert.equal((yield sm.getAccountData()), null);
});

// Some tests only for the secure storage manager.
add_task(function* checkNullUpdatesRemovedLocked() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA", kB: "kB"});
  sm.secureStorage.locked = true;
  yield sm.initialize();

  yield sm.updateAccountData({kA: null});
  let accountData = yield sm.getAccountData();
  Assert.ok(!accountData.kA);
  // still no kB as we are locked.
  Assert.ok(!accountData.kB);

  // now unlock - should still be no kA but kB should appear.
  sm.secureStorage.locked = false;
  accountData = yield sm.getAccountData();
  Assert.ok(!accountData.kA);
  Assert.equal(accountData.kB, "kB");
  // And secure storage should have been written with our previously-cached
  // data.
  Assert.strictEqual(sm.secureStorage.data.accountData.kA, undefined);
  Assert.strictEqual(sm.secureStorage.data.accountData.kB, "kB");
});

add_task(function* checkEverythingReadSecure() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA"});
  yield sm.initialize();

  let accountData = yield sm.getAccountData();
  Assert.ok(accountData, "read account data");
  Assert.equal(accountData.uid, "uid");
  Assert.equal(accountData.email, "someone@somewhere.com");
  Assert.equal(accountData.kA, "kA");
});

add_task(function* checkMemoryFieldsNotReturnedByDefault() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA"});
  yield sm.initialize();

  // keyPair is a memory field.
  yield sm.updateAccountData({keyPair: "the keypair value"});
  let accountData = yield sm.getAccountData();

  // Requesting everything should *not* return in memory fields.
  Assert.strictEqual(accountData.keyPair, undefined);
  // But requesting them specifically does get them.
  accountData = yield sm.getAccountData("keyPair");
  Assert.strictEqual(accountData.keyPair, "the keypair value");
});

add_task(function* checkExplicitGet() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA"});
  yield sm.initialize();

  let accountData = yield sm.getAccountData(["uid", "kA"]);
  Assert.ok(accountData, "read account data");
  Assert.equal(accountData.uid, "uid");
  Assert.equal(accountData.kA, "kA");
  // We didn't ask for email so shouldn't have got it.
  Assert.strictEqual(accountData.email, undefined);
});

add_task(function* checkExplicitGetNoSecureRead() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA"});
  yield sm.initialize();

  Assert.equal(sm.secureStorage.fetchCount, 0);
  // request 2 fields in secure storage - it should have caused a single fetch.
  let accountData = yield sm.getAccountData(["email", "uid"]);
  Assert.ok(accountData, "read account data");
  Assert.equal(accountData.uid, "uid");
  Assert.equal(accountData.email, "someone@somewhere.com");
  Assert.strictEqual(accountData.kA, undefined);
  Assert.equal(sm.secureStorage.fetchCount, 1);
});

add_task(function* checkLockedUpdates() {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "old-kA", kB: "kB"});
  sm.secureStorage.locked = true;
  yield sm.initialize();

  let accountData = yield sm.getAccountData();
  // requesting kA and kB will fail as storage is locked.
  Assert.ok(!accountData.kA);
  Assert.ok(!accountData.kB);
  // While locked we can still update it and see the updated value.
  sm.updateAccountData({kA: "new-kA"});
  accountData = yield sm.getAccountData();
  Assert.equal(accountData.kA, "new-kA");
  // unlock.
  sm.secureStorage.locked = false;
  accountData = yield sm.getAccountData();
  // should reflect the value we updated and the one we didn't.
  Assert.equal(accountData.kA, "new-kA");
  Assert.equal(accountData.kB, "kB");
  // And storage should also reflect it.
  Assert.strictEqual(sm.secureStorage.data.accountData.kA, "new-kA");
  Assert.strictEqual(sm.secureStorage.data.accountData.kB, "kB");
});

// Some tests for the "storage queue" functionality.

// A helper for our queued tests. It creates a StorageManager and then queues
// an unresolved promise. The tests then do additional setup and checks, then
// resolves or rejects the blocked promise.
var setupStorageManagerForQueueTest = Task.async(function* () {
  let sm = new FxAccountsStorageManager();
  sm.plainStorage = new MockedPlainStorage({uid: "uid", email: "someone@somewhere.com"})
  sm.secureStorage = new MockedSecureStorage({kA: "kA"});
  sm.secureStorage.locked = true;
  yield sm.initialize();

  let resolveBlocked, rejectBlocked;
  let blockedPromise = new Promise((resolve, reject) => {
    resolveBlocked = resolve;
    rejectBlocked = reject;
  });

  sm._queueStorageOperation(() => blockedPromise);
  return {sm, blockedPromise, resolveBlocked, rejectBlocked}
});

// First the general functionality.
add_task(function* checkQueueSemantics() {
  let { sm, resolveBlocked } = yield setupStorageManagerForQueueTest();

  // We've one unresolved promise in the queue - add another promise.
  let resolveSubsequent;
  let subsequentPromise = new Promise(resolve => {
    resolveSubsequent = resolve;
  });
  let subsequentCalled = false;

  sm._queueStorageOperation(() => {
    subsequentCalled = true;
    resolveSubsequent();
    return subsequentPromise;
  });

  // Our "subsequent" function should not have been called yet.
  Assert.ok(!subsequentCalled);

  // Release our blocked promise.
  resolveBlocked();

  // Our subsequent promise should end up resolved.
  yield subsequentPromise;
  Assert.ok(subsequentCalled);
  yield sm.finalize();
});

// Check that a queued promise being rejected works correctly.
add_task(function* checkQueueSemanticsOnError() {
  let { sm, blockedPromise, rejectBlocked } = yield setupStorageManagerForQueueTest();

  let resolveSubsequent;
  let subsequentPromise = new Promise(resolve => {
    resolveSubsequent = resolve;
  });
  let subsequentCalled = false;

  sm._queueStorageOperation(() => {
    subsequentCalled = true;
    resolveSubsequent();
    return subsequentPromise;
  });

  // Our "subsequent" function should not have been called yet.
  Assert.ok(!subsequentCalled);

  // Reject our blocked promise - the subsequent operations should still work
  // correctly.
  rejectBlocked("oh no");

  // Our subsequent promise should end up resolved.
  yield subsequentPromise;
  Assert.ok(subsequentCalled);

  // But the first promise should reflect the rejection.
  try {
    yield blockedPromise;
    Assert.ok(false, "expected this promise to reject");
  } catch (ex) {
    Assert.equal(ex, "oh no");
  }
  yield sm.finalize();
});


// And some tests for the specific operations that are queued.
add_task(function* checkQueuedReadAndUpdate() {
  let { sm, resolveBlocked } = yield setupStorageManagerForQueueTest();
  // Mock the underlying operations
  // _doReadAndUpdateSecure is queued by _maybeReadAndUpdateSecure
  let _doReadCalled = false;
  sm._doReadAndUpdateSecure = () => {
    _doReadCalled = true;
    return Promise.resolve();
  }

  let resultPromise = sm._maybeReadAndUpdateSecure();
  Assert.ok(!_doReadCalled);

  resolveBlocked();
  yield resultPromise;
  Assert.ok(_doReadCalled);
  yield sm.finalize();
});

add_task(function* checkQueuedWrite() {
  let { sm, resolveBlocked } = yield setupStorageManagerForQueueTest();
  // Mock the underlying operations
  let __writeCalled = false;
  sm.__write = () => {
    __writeCalled = true;
    return Promise.resolve();
  }

  let writePromise = sm._write();
  Assert.ok(!__writeCalled);

  resolveBlocked();
  yield writePromise;
  Assert.ok(__writeCalled);
  yield sm.finalize();
});

add_task(function* checkQueuedDelete() {
  let { sm, resolveBlocked } = yield setupStorageManagerForQueueTest();
  // Mock the underlying operations
  let _deleteCalled = false;
  sm._deleteAccountData = () => {
    _deleteCalled = true;
    return Promise.resolve();
  }

  let resultPromise = sm.deleteAccountData();
  Assert.ok(!_deleteCalled);

  resolveBlocked();
  yield resultPromise;
  Assert.ok(_deleteCalled);
  yield sm.finalize();
});

function run_test() {
  run_next_test();
}
