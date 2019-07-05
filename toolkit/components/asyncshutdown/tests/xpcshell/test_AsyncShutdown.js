/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);

add_task(async function test_no_condition() {
  for (let kind of [
    "phase",
    "barrier",
    "xpcom-barrier",
    "xpcom-barrier-unwrapped",
  ]) {
    info("Testing a barrier with no condition (" + kind + ")");
    let lock = makeLock(kind);
    await lock.wait();
    info("Barrier with no condition didn't lock");
  }
});

add_task(async function test_phase_various_failures() {
  for (let kind of [
    "phase",
    "barrier",
    "xpcom-barrier",
    "xpcom-barrier-unwrapped",
  ]) {
    info("Kind: " + kind);
    // Testing with wrong arguments
    let lock = makeLock(kind);

    Assert.throws(
      () => lock.addBlocker(),
      /TypeError|NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS/
    );
    Assert.throws(
      () => lock.addBlocker(null, true),
      /TypeError|NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS/
    );

    if (kind != "xpcom-barrier") {
      // xpcom-barrier actually expects a string in that position
      Assert.throws(
        () => lock.addBlocker("Test 2", () => true, "not a function"),
        /TypeError/
      );
    }

    // Attempting to add a blocker after we are done waiting
    await lock.wait();
    Assert.throws(() => lock.addBlocker("Test 3", () => true), /is finished/);
  }
});

add_task(async function test_reentrant() {
  info("Ensure that we can call addBlocker from within a blocker");

  for (let kind of [
    "phase",
    "barrier",
    "xpcom-barrier",
    "xpcom-barrier-unwrapped",
  ]) {
    info("Kind: " + kind);
    let lock = makeLock(kind);

    let deferredOuter = PromiseUtils.defer();
    let deferredInner = PromiseUtils.defer();
    let deferredBlockInner = PromiseUtils.defer();

    lock.addBlocker("Outer blocker", () => {
      info("Entering outer blocker");
      deferredOuter.resolve();
      lock.addBlocker("Inner blocker", () => {
        info("Entering inner blocker");
        deferredInner.resolve();
        return deferredBlockInner.promise;
      });
    });

    // Note that phase-style locks spin the event loop and do not return from
    // `lock.wait()` until after all blockers have been resolved. Therefore,
    // to be able to test them, we need to dispatch the following steps to the
    // event loop before calling `lock.wait()`, which we do by forcing
    // a Promise.resolve().
    //
    let promiseSteps = (async function() {
      await Promise.resolve();

      info("Waiting until we have entered the outer blocker");
      await deferredOuter.promise;

      info("Waiting until we have entered the inner blocker");
      await deferredInner.promise;

      info("Allowing the lock to resolve");
      deferredBlockInner.resolve();
    })();

    info("Starting wait");
    await lock.wait();

    info("Waiting until all steps have been walked");
    await promiseSteps;
  }
});

add_task(async function test_phase_removeBlocker() {
  info(
    "Testing that we can call removeBlocker before, during and after the call to wait()"
  );

  for (let kind of [
    "phase",
    "barrier",
    "xpcom-barrier",
    "xpcom-barrier-unwrapped",
  ]) {
    info("Switching to kind " + kind);
    info("Attempt to add then remove a blocker before wait()");
    let lock = makeLock(kind);
    let blocker = () => {
      info("This promise will never be resolved");
      return PromiseUtils.defer().promise;
    };

    lock.addBlocker("Wait forever", blocker);
    let do_remove_blocker = function(aLock, aBlocker, aShouldRemove) {
      info(
        "Attempting to remove blocker " +
          aBlocker +
          ", expecting result " +
          aShouldRemove
      );
      if (kind == "xpcom-barrier") {
        // The xpcom variant always returns `undefined`, so we can't
        // check its result.
        aLock.removeBlocker(aBlocker);
        return;
      }
      Assert.equal(aLock.removeBlocker(aBlocker), aShouldRemove);
    };
    do_remove_blocker(lock, blocker, true);
    do_remove_blocker(lock, blocker, false);
    info("Attempt to remove non-registered blockers before wait()");
    do_remove_blocker(lock, "foo", false);
    do_remove_blocker(lock, null, false);
    info("Waiting (should lift immediately)");
    await lock.wait();

    info("Attempt to add a blocker then remove it during wait()");
    lock = makeLock(kind);
    let blockers = [
      () => {
        info("This blocker will self-destruct");
        do_remove_blocker(lock, blockers[0], true);
        return PromiseUtils.defer().promise;
      },
      () => {
        info("This blocker will self-destruct twice");
        do_remove_blocker(lock, blockers[1], true);
        do_remove_blocker(lock, blockers[1], false);
        return PromiseUtils.defer().promise;
      },
      () => {
        info("Attempt to remove non-registered blockers during wait()");
        do_remove_blocker(lock, "foo", false);
        do_remove_blocker(lock, null, false);
      },
    ];
    for (let i in blockers) {
      lock.addBlocker("Wait forever again: " + i, blockers[i]);
    }
    info("Waiting (should lift very quickly)");
    await lock.wait();
    do_remove_blocker(lock, blockers[0], false);

    info("Attempt to remove a blocker after wait");
    lock = makeLock(kind);
    blocker = Promise.resolve.bind(Promise);
    await lock.wait();
    do_remove_blocker(lock, blocker, false);

    info("Attempt to remove non-registered blocker after wait()");
    do_remove_blocker(lock, "foo", false);
    do_remove_blocker(lock, null, false);
  }
});

add_task(async function test_state() {
  info("Testing information contained in `state`");

  let BLOCKER_NAME = "test_state blocker " + Math.random();

  // Set up the barrier. Note that we cannot test `barrier.state`
  // immediately, as it initially contains "Not started"
  let barrier = new AsyncShutdown.Barrier("test_filename");
  let deferred = PromiseUtils.defer();
  let { filename, lineNumber } = Components.stack;
  barrier.client.addBlocker(BLOCKER_NAME, function() {
    return deferred.promise;
  });

  let promiseDone = barrier.wait();

  // Now that we have called `wait()`, the state contains interesting things
  let state = barrier.state[0];
  info("State: " + JSON.stringify(barrier.state, null, "\t"));
  Assert.equal(state.filename, filename);
  Assert.equal(state.lineNumber, lineNumber + 1);
  Assert.equal(state.name, BLOCKER_NAME);
  Assert.ok(
    state.stack.some(x => x.includes("test_state")),
    "The stack contains the caller function's name"
  );
  Assert.ok(
    state.stack.some(x => x.includes(filename)),
    "The stack contains the calling file's name"
  );

  deferred.resolve();
  await promiseDone;
});

add_task(async function() {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});
