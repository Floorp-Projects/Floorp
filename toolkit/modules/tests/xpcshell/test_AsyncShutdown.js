let Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/AsyncShutdown.jsm");

Services.prefs.setBoolPref("toolkit.asyncshutdown.testing", true);

function run_test() {
  run_next_test();
}

function get_exn(f) {
  try {
    f();
    return null;
  } catch (ex) {
    return ex;
  }
}

function do_check_exn(exn, constructor) {
  do_check_neq(exn, null);
  if (exn.name == constructor) {
    do_check_eq(exn.constructor.name, constructor);
    return;
  }
  do_print("Wrong error constructor");
  do_print(exn.constructor.name);
  do_print(exn.stack);
  do_check_true(false);
}

/**
 * Utility function used to provide the same API for both AsyncShutdown
 * phases and AsyncShutdown barriers.
 *
 * @param {bool} heavy If |true|, use a AsyncShutdown phase, otherwise
 * a AsyncShutdown barrier.
 *
 * @return An object with the following methods:
 *   - addBlocker() - the same method as AsyncShutdown phases and barrier clients
 *   - wait() - trigger the resolution of the lock
 */
function makeLock(heavy) {
  if (heavy) {
    let topic = "test-Phase-" + ++makeLock.counter;
    let phase = AsyncShutdown._getPhase(topic);
    return {
      addBlocker: function(...args) {
        return phase.addBlocker(...args);
      },
      removeBlocker: function(blocker) {
        return phase.removeBlocker(blocker);
      },
      wait: function() {
        Services.obs.notifyObservers(null, topic, null);
        return Promise.resolve();
      }
    };
  } else {
    let name = "test-Barrier-" + ++makeLock.counter;
    let barrier = new AsyncShutdown.Barrier(name);
    return {
      addBlocker: barrier.client.addBlocker,
      removeBlocker: barrier.client.removeBlocker,
      wait: function() {
        return barrier.wait();
      }
    };
  }
}
makeLock.counter = 0;

/**
 * An asynchronous task that takes several ticks to complete.
 *
 * @param {*=} resolution The value with which the resulting promise will be
 * resolved once the task is complete. This may be a rejected promise,
 * in which case the resulting promise will itself be rejected.
 * @param {object=} outResult An object modified by side-effect during the task.
 * Initially, its field |isFinished| is set to |false|. Once the task is
 * complete, its field |isFinished| is set to |true|.
 *
 * @return {promise} A promise fulfilled once the task is complete
 */
function longRunningAsyncTask(resolution = undefined, outResult = {}) {
  outResult.isFinished = false;
  if (!("countFinished" in outResult)) {
    outResult.countFinished = 0;
  }
  let deferred = Promise.defer();
  do_timeout(100, function() {
    ++outResult.countFinished;
    outResult.isFinished = true;
    deferred.resolve(resolution);
  });
  return deferred.promise;
}


//////// Tests on AsyncShutdown phases

/*add_task*/(function* test_no_condition() {
  for (let heavy of [false, true]) {
    do_print("Testing a barrier with no condition (" + heavy?"heavy":"light" + ")");
    let lock = makeLock(heavy);
    yield lock.wait();
    do_print("Barrier with no condition didn't lock");
  }
});

/*add_task*/(function* test_phase_simple_async() {
  do_print("Testing various combinations of a phase with a single condition");
  for (let heavy of [false, true]) {
    for (let arg of [undefined, null, "foo", 100, new Error("BOOM")]) {
      for (let resolution of [arg, Promise.reject(arg)]) {
        for (let success of [false, true]) {
          for (let state of [[null],
                             [],
                             [() => "some state"],
                             [function() {
                               throw new Error("State BOOM"); }],
                             [function() {
                               return {
                                 toJSON: function() {
                                   throw new Error("State.toJSON BOOM");
                                 }
                               };
                             }]]) {
            // Asynchronous phase
            do_print("Asynchronous test with " + arg + ", " + resolution + ", " + heavy);
            let lock = makeLock(heavy);
            let outParam = { isFinished: false };
            lock.addBlocker(
              "Async test",
               function() {
                 if (success) {
                   return longRunningAsyncTask(resolution, outParam);
                 } else {
                   throw resolution;
                 }
               },
               ...state
            );
            do_check_false(outParam.isFinished);
            yield lock.wait();
            do_check_eq(outParam.isFinished, success);
          }
        }

        // Synchronous phase - just test that we don't throw/freeze
        do_print("Synchronous test with " + arg + ", " + resolution + ", " + heavy);
        let lock = makeLock(heavy);
        lock.addBlocker(
          "Sync test",
          resolution
        );
        yield lock.wait();
      }
    }
  }
});

/*add_task*/(function* test_phase_many() {
  do_print("Testing various combinations of a phase with many conditions");
  for (let heavy of [false, true]) {
    let lock = makeLock(heavy);
    let outParams = [];
    for (let arg of [undefined, null, "foo", 100, new Error("BOOM")]) {
      for (let resolve of [true, false]) {
        do_print("Testing with " + heavy + ", " + arg + ", " + resolve);
        let resolution = resolve ? arg : Promise.reject(arg);
        let outParam = { isFinished: false };
        lock.addBlocker(
          "Test",
          () => longRunningAsyncTask(resolution, outParam)
        );
      }
    }
    do_check_true(outParams.every((x) => !x.isFinished));
    yield lock.wait();
    do_check_true(outParams.every((x) => x.isFinished));
  }
});



/*add_task*/(function* test_phase_various_failures() {
  do_print("Ensure that we cannot add a condition for a phase once notification has been received");
  for (let heavy of [false, true]) {
    let lock = makeLock(heavy);
    lock.wait(); // Don't actually wait for the promise to be resolved
    let exn = get_exn(() => lock.addBlocker("Test", true));
    do_check_true(!!exn);

    do_print("Ensure that an incomplete blocker causes a TypeError");

    lock = makeLock(heavy);
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

  for (let heavy of [false, true]) {

    do_print("Attempt to add then remove a blocker before wait()");
    let lock = makeLock(heavy);
    let blocker = () => {
      do_print("This promise will never be resolved");
      return Promise.defer().promise;
    };

    lock.addBlocker("Wait forever", blocker);
    do_check_true(lock.removeBlocker(blocker));
    do_check_false(lock.removeBlocker(blocker));
    do_print("Attempt to remove non-registered blockers before wait()");
    do_check_false(lock.removeBlocker("foo"));
    do_check_false(lock.removeBlocker(null));
    do_print("Waiting (should lift immediately)");
    yield lock.wait();

    do_print("Attempt to add a blocker then remove it during wait()");
    lock = makeLock(heavy);
    let blockers = [
      () => {
        do_print("This blocker will self-destruct");
        do_check_true(lock.removeBlocker(blockers[0]));
        return Promise.defer().promise;
      },
      () => {
        do_print("This blocker will self-destruct twice");
        do_check_true(lock.removeBlocker(blockers[1]));
        do_check_false(lock.removeBlocker(blockers[1]));
        return Promise.defer().promise;
      },
      () => {
        do_print("Attempt to remove non-registered blockers during wait()");
        do_check_false(lock.removeBlocker("foo"));
        do_check_false(lock.removeBlocker(null));
      }
    ];
    for (let i in blockers) {
      lock.addBlocker("Wait forever again: " + i, blockers[i]);
    }
    do_print("Waiting (should lift very quickly)");
    yield lock.wait();
    do_check_false(lock.removeBlocker(blockers[0]));


    do_print("Attempt to remove a blocker after wait");
    lock = makeLock(heavy);
    blocker = Promise.resolve;
    yield lock.wait();

    do_print("Attempt to remove non-registered blocker after wait()");
    do_check_false(lock.removeBlocker("foo"));
    do_check_false(lock.removeBlocker(null));
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
  
  deferred.resolve();
  yield promiseDone;
});

add_task(function*() {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});

