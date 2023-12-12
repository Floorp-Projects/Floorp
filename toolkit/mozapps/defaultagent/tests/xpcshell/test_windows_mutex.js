/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Multiple instances of a named mutex on Windows can lock on the same thread, so
// we have to run each test across at least two distinct threads. Running on a
// separate process achieves the same.
do_load_child_test_harness();

let parentFactory = Cc["@mozilla.org/windows-mutex-factory;1"].createInstance(
  Ci.nsIWindowsMutexFactory
);

function promiseCommand(aCommand) {
  // Exceptions don't propogate to the process that called `sendCommand` nor
  // tigger a test failure, so wrap the command to ensure we fail appropriately.
  let wrappedCommand = `try {${aCommand}} catch(e) {Assert.ok(false, "Error running command received in child process. Note the passed in function must be self-contained. Error: \${e.toString()}");}`;
  return new Promise(resolve => sendCommand(wrappedCommand, resolve));
}

// This is passed as a string to a child process, thus must be self-contained.
function assertLockOkOnChild(aName, aTestString) {
  if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
    Assert.ok(false, `${assertLockOkOnChild.name} run on child process.`);
  }

  let childFactory = Cc["@mozilla.org/windows-mutex-factory;1"].createInstance(
    Ci.nsIWindowsMutexFactory
  );

  let lockingMutex = childFactory.createMutex(aName);

  info(`Locking mutex for subtest "${aTestString}"`);
  lockingMutex.tryLock();
  try {
    Assert.ok(lockingMutex.isLocked(), aTestString);
  } finally {
    lockingMutex.unlock();
  }
}

// This is passed as a string to a child process, thus must be self-contained.
function assertLockThrowsOnChild(aName, aTestString) {
  if (Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT) {
    Assert.ok(false, `${assertLockThrowsOnChild.name} run on child process.`);
  }

  let childFactory = Cc["@mozilla.org/windows-mutex-factory;1"].createInstance(
    Ci.nsIWindowsMutexFactory
  );

  let blockedMutex = childFactory.createMutex(aName);

  info(`Locking mutex for subtest "${aTestString}"`);
  Assert.throws(blockedMutex.tryLock, /NS_ERROR_NOT_AVAILABLE/, aTestString);
  Assert.ok(!blockedMutex.isLocked(), "Not locked after error.");
}

add_task(async function test_lock_blocks() {
  const kTestMutexName = Services.uuid.generateUUID().toString();
  let lockingMutex = parentFactory.createMutex(kTestMutexName);

  Assert.ok(!lockingMutex.isLocked(), "Reported unlocked before locking.");

  info(`Locking mutex named "${kTestMutexName}"`);
  lockingMutex.tryLock();
  try {
    Assert.ok(lockingMutex.isLocked(), "Reported locked after locking.");

    await promiseCommand(
      `(${assertLockThrowsOnChild.toString()})("${kTestMutexName}", "Concurrent attempts to lock identically named mutex throws.");`
    );
  } finally {
    lockingMutex.unlock();
  }
});

add_task(async function test_unlock_unblocks() {
  const kTestMutexName = Services.uuid.generateUUID().toString();
  let lockingMutex = parentFactory.createMutex(kTestMutexName);

  info(`Locking mutex named "${kTestMutexName}"`);
  lockingMutex.tryLock();
  lockingMutex.unlock();

  Assert.ok(!lockingMutex.isLocked(), "Reported unlocked after unlocking.");

  await promiseCommand(
    `(${assertLockOkOnChild.toString()})("${kTestMutexName}", "Locked previously unlocked mutex.");`
  );
});

add_task(async function test_names_dont_conflict() {
  const kTestMutexName = Services.uuid.generateUUID().toString();
  let mutex1 = parentFactory.createMutex(kTestMutexName);

  info(`Locking mutex named "${kTestMutexName}"`);
  mutex1.tryLock();
  try {
    await promiseCommand(
      `(${assertLockOkOnChild.toString()})(Services.uuid.generateUUID().toString(), "Differently named mutexes don't conflict");`
    );
  } finally {
    mutex1.unlock();
  }
});

add_task(async function test_relock_when_locked() {
  const kTestMutexName = Services.uuid.generateUUID().toString();
  let mutex = parentFactory.createMutex(kTestMutexName);

  mutex.tryLock();
  try {
    Assert.ok(() => mutex.tryLock(), "Relocking locked mutex succeeds.");
    Assert.ok(
      mutex.isLocked(),
      "Reported locked after relocking locked mutex."
    );
  } finally {
    mutex.unlock();
  }
});

add_task(async function test_unlock_without_lock() {
  const kTestMutexName = Services.uuid.generateUUID().toString();
  let mutex = parentFactory.createMutex(kTestMutexName);

  mutex.unlock();
  Assert.ok(
    !mutex.isLocked(),
    "Reported unlocked after unnecessarily unlocking mutex."
  );

  mutex.tryLock();
  try {
    Assert.ok(
      mutex.isLocked(),
      "Reported locked after locking unnecessarily unlocked mutex."
    );
  } finally {
    mutex.unlock();
  }
});
