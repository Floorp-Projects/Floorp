/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function run_test() {
  run_next_test();
}

add_task(function* test_no_condition() {
  for (let kind of ["phase", "barrier", "xpcom-barrier", "xpcom-barrier-unwrapped"]) {
    do_print("Testing a barrier with no condition (" + kind + ")");
    let lock = makeLock(kind);
    yield lock.wait();
    do_print("Barrier with no condition didn't lock");
  }
});


add_task(function* test_phase_various_failures() {
  do_print("Ensure that we cannot add a condition for a phase once notification has been received");
  for (let kind of ["phase", "barrier", "xpcom-barrier", "xpcom-barrier-unwrapped"]) {
    let lock = makeLock(kind);
    lock.wait(); // Don't actually wait for the promise to be resolved
    let exn = get_exn(() => lock.addBlocker("Test", true));
    do_check_true(!!exn);

    if (kind == "xpcom-barrier") {
      do_print("Skipping this part of the test that is caught differently by XPConnect");
      continue;
    }
    do_print("Ensure that an incomplete blocker causes a TypeError");

    lock = makeLock(kind);
    exn = get_exn(() => lock.addBlocker());
    do_check_exn(exn, "TypeError");

    exn = get_exn(() => lock.addBlocker(null, true));
    do_check_exn(exn, "TypeError");

    exn = get_exn(() => lock.addBlocker("Test 2", () => true, "not a function"));
    do_check_exn(exn, "TypeError");
  }
});


add_task(function* test_phase_removeBlocker() {
  do_print("Testing that we can call removeBlocker before, during and after the call to wait()");

  for (let kind of ["phase", "barrier", "xpcom-barrier", "xpcom-barrier-unwrapped"]) {

    do_print("Switching to kind " + kind);
    do_print("Attempt to add then remove a blocker before wait()");
    let lock = makeLock(kind);
    let blocker = () => {
      do_print("This promise will never be resolved");
      return Promise.defer().promise;
    };

    lock.addBlocker("Wait forever", blocker);
    let do_remove_blocker = function(lock, blocker, shouldRemove) {
      do_print("Attempting to remove blocker " + blocker + ", expecting result " + shouldRemove);
      if (kind == "xpcom-barrier") {
        // The xpcom variant always returns `undefined`, so we can't
        // check its result.
        lock.removeBlocker(blocker);
        return;
      }
      do_check_eq(lock.removeBlocker(blocker), shouldRemove);
    };
    do_remove_blocker(lock, blocker, true);
    do_remove_blocker(lock, blocker, false);
    do_print("Attempt to remove non-registered blockers before wait()");
    do_remove_blocker(lock, "foo", false);
    do_remove_blocker(lock, null, false);
    do_print("Waiting (should lift immediately)");
    yield lock.wait();

    do_print("Attempt to add a blocker then remove it during wait()");
    lock = makeLock(kind);
    let blockers = [
      () => {
        do_print("This blocker will self-destruct");
        do_remove_blocker(lock, blockers[0], true);
        return Promise.defer().promise;
      },
      () => {
        do_print("This blocker will self-destruct twice");
        do_remove_blocker(lock, blockers[1], true);
        do_remove_blocker(lock, blockers[1], false);
        return Promise.defer().promise;
      },
      () => {
        do_print("Attempt to remove non-registered blockers during wait()");
        do_remove_blocker(lock, "foo", false);
        do_remove_blocker(lock, null, false);
      }
    ];
    for (let i in blockers) {
      lock.addBlocker("Wait forever again: " + i, blockers[i]);
    }
    do_print("Waiting (should lift very quickly)");
    yield lock.wait();
    do_remove_blocker(lock, blockers[0], false);


    do_print("Attempt to remove a blocker after wait");
    lock = makeLock(kind);
    blocker = Promise.resolve;
    yield lock.wait();
    do_remove_blocker(lock, blocker, false);

    do_print("Attempt to remove non-registered blocker after wait()");
    do_remove_blocker(lock, "foo", false);
    do_remove_blocker(lock, null, false);
  }

});

add_task(function* test_state() {
  do_print("Testing information contained in `state`");

  let BLOCKER_NAME = "test_state blocker " + Math.random();

  // Set up the barrier. Note that we cannot test `barrier.state`
  // immediately, as it initially contains "Not started"
  let barrier = new AsyncShutdown.Barrier("test_filename");
  let deferred = Promise.defer();
  let {filename, lineNumber} = Components.stack;
  barrier.client.addBlocker(BLOCKER_NAME,
    function() {
      return deferred.promise;
    });

  let promiseDone = barrier.wait();

  // Now that we have called `wait()`, the state contains interesting things
  let state = barrier.state[0];
  do_print("State: " + JSON.stringify(barrier.state, null, "\t"));
  Assert.equal(state.filename, filename);
  Assert.equal(state.lineNumber, lineNumber + 1);
  Assert.equal(state.name, BLOCKER_NAME);
  Assert.ok(state.stack.some(x => x.contains("test_state")), "The stack contains the caller function's name");
  Assert.ok(state.stack.some(x => x.contains(filename)), "The stack contains the calling file's name");

  deferred.resolve();
  yield promiseDone;
});

add_task(function*() {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});

