/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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

    if (kind == "xpcom-barrier") {
      const blocker = () => true;
      lock.addBlocker("Test 3", blocker);
      Assert.throws(
        () => lock.addBlocker("Test 3", blocker),
        /We have already registered the blocker \(Test 3\)/
      );
    }

    // Attempting to add a blocker after we are done waiting
    Assert.ok(!lock.isClosed, "Barrier is open");
    await lock.wait();
    Assert.throws(() => lock.addBlocker("Test 4", () => true), /is finished/);
    Assert.ok(lock.isClosed, "Barrier is closed");
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

    let deferredOuter = Promise.withResolvers();
    let deferredInner = Promise.withResolvers();
    let deferredBlockInner = Promise.withResolvers();

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
    let promiseSteps = (async function () {
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
      return Promise.withResolvers().promise;
    };

    lock.addBlocker("Wait forever", blocker);
    let do_remove_blocker = function (aLock, aBlocker, aShouldRemove) {
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
        return Promise.withResolvers().promise;
      },
      () => {
        info("This blocker will self-destruct twice");
        do_remove_blocker(lock, blockers[1], true);
        do_remove_blocker(lock, blockers[1], false);
        return Promise.withResolvers().promise;
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

add_task(async function test_addBlocker_noDistinctNamesConstraint() {
  info("Testing that we can add two distinct blockers with identical names");

  for (let kind of [
    "phase",
    "barrier",
    "xpcom-barrier",
    "xpcom-barrier-unwrapped",
  ]) {
    info("Switching to kind " + kind);
    let lock = makeLock(kind);
    let deferred1 = Promise.withResolvers();
    let resolved1 = false;
    let deferred2 = Promise.withResolvers();
    let resolved2 = false;
    let blocker1 = () => {
      info("Entering blocker1");
      return deferred1.promise;
    };
    let blocker2 = () => {
      info("Entering blocker2");
      return deferred2.promise;
    };

    info("Attempt to add two distinct blockers with identical names");
    lock.addBlocker("Blocker", blocker1);
    lock.addBlocker("Blocker", blocker2);

    // Note that phase-style locks spin the event loop and do not return from
    // `lock.wait()` until after all blockers have been resolved. Therefore,
    // to be able to test them, we need to dispatch the following steps to the
    // event loop before calling `lock.wait()`, which we do by forcing
    // a Promise.resolve().
    //
    let promiseSteps = (async () => {
      info("Waiting for an event-loop spin");
      await Promise.resolve();

      info("Resolving blocker1");
      deferred1.resolve();
      resolved1 = true;

      info("Waiting for an event-loop spin");
      await Promise.resolve();

      info("Resolving blocker2");
      deferred2.resolve();
      resolved2 = true;
    })();

    info("Waiting for lock");
    await lock.wait();

    Assert.ok(resolved1);
    Assert.ok(resolved2);
    await promiseSteps;
  }
});

add_task(async function test_state() {
  info("Testing information contained in `state`");

  let BLOCKER_NAME = "test_state blocker " + Math.random();

  // Set up the barrier. Note that we cannot test `barrier.state`
  // immediately, as it initially contains "Not started"
  let barrier = new AsyncShutdown.Barrier("test_filename");
  let deferred = Promise.withResolvers();
  let { filename, lineNumber } = Components.stack;
  barrier.client.addBlocker(BLOCKER_NAME, function () {
    return deferred.promise;
  });

  let promiseDone = barrier.wait();

  // Now that we have called `wait()`, the state contains interesting things
  info("State: " + JSON.stringify(barrier.state, null, "\t"));
  let state = barrier.state[0];
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

add_task(async function test_multistate() {
  info("Testing information contained in multiple `state`");

  let BLOCKER_NAMES = [
    "test_state blocker " + Math.random(),
    "test_state blocker " + Math.random(),
  ];

  // Set up the barrier. Note that we cannot test `barrier.state`
  // immediately, as it initially contains "Not started"
  let barrier = asyncShutdownService.makeBarrier("test_filename");
  let deferred = Promise.withResolvers();
  let { filename, lineNumber } = Components.stack;
  for (let name of BLOCKER_NAMES) {
    barrier.client.jsclient.addBlocker(name, () => deferred.promise, {
      fetchState: () => ({ progress: name }),
    });
  }

  let promiseDone = new Promise(r => barrier.wait(r));

  // Now that we have called `wait()`, the state contains interesting things.
  Assert.ok(
    barrier.state instanceof Ci.nsIPropertyBag,
    "State is a PropertyBag"
  );
  for (let i = 0; i < BLOCKER_NAMES.length; ++i) {
    let state = barrier.state.getProperty(i.toString());
    Assert.equal(typeof state, "string", "state is a string");
    info("State: " + state + "\t");
    state = JSON.parse(state);
    Assert.equal(state.filename, filename);
    Assert.equal(state.lineNumber, lineNumber + 2);
    Assert.equal(state.name, BLOCKER_NAMES[i]);
    Assert.ok(
      state.stack.some(x => x.includes("test_multistate")),
      "The stack contains the caller function's name"
    );
    Assert.ok(
      state.stack.some(x => x.includes(filename)),
      "The stack contains the calling file's name"
    );
    Assert.equal(
      state.state.progress,
      BLOCKER_NAMES[i],
      "The state contains the fetchState provided value"
    );
  }

  deferred.resolve();
  await promiseDone;
});

add_task(async function () {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});
