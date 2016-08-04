/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

//
// This file contains tests that need to leave uncaught asynchronous
// errors. If your test catches all its asynchronous errors, please
// put it in another file.
//

Promise.Debugging.clearUncaughtErrorObservers();

function run_test() {
  run_next_test();
}

add_task(function* test_phase_simple_async() {
  do_print("Testing various combinations of a phase with a single condition");
  for (let kind of ["phase", "barrier", "xpcom-barrier", "xpcom-barrier-unwrapped"]) {
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
            do_print("Asynchronous test with " + arg + ", " + resolution + ", " + kind);
            let lock = makeLock(kind);
            let outParam = { isFinished: false };
            lock.addBlocker(
              "Async test",
               function() {
                 if (success) {
                   return longRunningAsyncTask(resolution, outParam);
                 }
                 throw resolution;
               },
               ...state
            );
            do_check_false(outParam.isFinished);
            yield lock.wait();
            do_check_eq(outParam.isFinished, success);
          }
        }

        // Synchronous phase - just test that we don't throw/freeze
        do_print("Synchronous test with " + arg + ", " + resolution + ", " + kind);
        let lock = makeLock(kind);
        lock.addBlocker(
          "Sync test",
          resolution
        );
        yield lock.wait();
      }
    }
  }
});

add_task(function* test_phase_many() {
  do_print("Testing various combinations of a phase with many conditions");
  for (let kind of ["phase", "barrier", "xpcom-barrier", "xpcom-barrier-unwrapped"]) {
    let lock = makeLock(kind);
    let outParams = [];
    for (let arg of [undefined, null, "foo", 100, new Error("BOOM")]) {
      for (let resolve of [true, false]) {
        do_print("Testing with " + kind + ", " + arg + ", " + resolve);
        let resolution = resolve ? arg : Promise.reject(arg);
        let outParam = { isFinished: false };
        lock.addBlocker(
          "Test " + Math.random(),
          () => longRunningAsyncTask(resolution, outParam)
        );
      }
    }
    do_check_true(outParams.every((x) => !x.isFinished));
    yield lock.wait();
    do_check_true(outParams.every((x) => x.isFinished));
  }
});




add_task(function*() {
  Services.prefs.clearUserPref("toolkit.asyncshutdown.testing");
});

