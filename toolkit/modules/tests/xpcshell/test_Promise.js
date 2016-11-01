/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

Components.utils.import("resource://gre/modules/Promise.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource://testing-common/PromiseTestUtils.jsm");

// Prevent test failures due to the unhandled rejections in this test file.
PromiseTestUtils.disableUncaughtRejectionObserverForSelfTest();

// //////////////////////////////////////////////////////////////////////////////
// // Test runner

var run_promise_tests = function run_promise_tests(tests, cb) {
  let loop = function loop(index) {
    if (index >= tests.length) {
      if (cb) {
        cb.call();
      }
      return;
    }
    do_print("Launching test " + (index + 1) + "/" + tests.length);
    let test = tests[index];
    // Execute from an empty stack
    let next = function next() {
      do_print("Test " + (index + 1) + "/" + tests.length + " complete");
      do_execute_soon(function() {
        loop(index + 1);
      });
    };
    let result = test();
    result.then(next, next);
  };
  return loop(0);
};

var make_promise_test = function(test) {
  return function runtest() {
    do_print("Test starting: " + test.name);
    try {
      let result = test();
      if (result && "promise" in result) {
        result = result.promise;
      }
      if (!result || !("then" in result)) {
        let exn;
        try {
          do_throw("Test " + test.name + " did not return a promise: " + result);
        } catch (x) {
          exn = x;
        }
        return Promise.reject(exn);
      }
      // The test returns a promise
      result = result.then(
        // Test complete
        function onResolve() {
          do_print("Test complete: " + test.name);
        },
        // The test failed with an unexpected error
        function onReject(err) {
          let detail;
          if (err && typeof err == "object" && "stack" in err) {
            detail = err.stack;
          } else {
            detail = "(no stack)";
          }
          do_throw("Test " + test.name + " rejected with the following reason: "
              + err + detail);
      });
      return result;
    } catch (x) {
      // The test failed because of an error outside of a promise
      do_throw("Error in body of test " + test.name + ": " + x + " at " + x.stack);
      return Promise.reject();
    }
  };
};

// //////////////////////////////////////////////////////////////////////////////
// // Tests

var tests = [];

// Utility function to observe an failures in a promise
// This function is useful if the promise itself is
// not returned.
var observe_failures = function observe_failures(promise) {
  promise.catch(function onReject(reason) {
    test.do_throw("Observed failure in test " + test + ": " + reason);
  });
};

// Test that all observers are notified
tests.push(make_promise_test(
  function notification(test) {
    // The size of the test
    const SIZE = 10;
    const RESULT = "this is an arbitrary value";

    // Number of observers that yet need to be notified
    let expected = SIZE;

    // |true| once an observer has been notified
    let notified = [];

    // The promise observed
    let source = Promise.defer();
    let result = Promise.defer();

    let install_observer = function install_observer(i) {
      observe_failures(source.promise.then(
        function onSuccess(value) {
          do_check_true(!notified[i], "Ensuring that observer is notified at most once");
          notified[i] = true;

          do_check_eq(value, RESULT, "Ensuring that the observed value is correct");
          if (--expected == 0) {
            result.resolve();
          }
        }));
    };

    // Install a number of observers before resolving
    let i;
    for (i = 0; i < SIZE/2; ++i) {
      install_observer(i);
    }

    source.resolve(RESULT);

    // Install remaining observers
    for (;i < SIZE; ++i) {
      install_observer(i);
    }

    return result;
  }));

// Test that observers get the correct "this" value in strict mode.
tests.push(
  make_promise_test(function handlers_this_value(test) {
    return Promise.resolve().then(
      function onResolve() {
        // Since this file is in strict mode, the correct value is "undefined".
        do_check_eq(this, undefined);
        throw "reject";
      }
    ).then(
      null,
      function onReject() {
        // Since this file is in strict mode, the correct value is "undefined".
        do_check_eq(this, undefined);
      }
    );
  }));

// Test that observers registered on a pending promise are notified in order.
tests.push(
  make_promise_test(function then_returns_before_callbacks(test) {
    let deferred = Promise.defer();
    let promise = deferred.promise;

    let order = 0;

    promise.then(
      function onResolve() {
        do_check_eq(order, 0);
        order++;
      }
    );

    promise.then(
      function onResolve() {
        do_check_eq(order, 1);
        order++;
      }
    );

    let newPromise = promise.then(
      function onResolve() {
        do_check_eq(order, 2);
      }
    );

    deferred.resolve();

    // This test finishes after the last handler succeeds.
    return newPromise;
  }));

// Test that observers registered on a resolved promise are notified in order.
tests.push(
  make_promise_test(function then_returns_before_callbacks(test) {
    let promise = Promise.resolve();

    let order = 0;

    promise.then(
      function onResolve() {
        do_check_eq(order, 0);
        order++;
      }
    );

    promise.then(
      function onResolve() {
        do_check_eq(order, 1);
        order++;
      }
    );

    // This test finishes after the last handler succeeds.
    return promise.then(
      function onResolve() {
        do_check_eq(order, 2);
      }
    );
  }));

// Test that all observers are notified at most once, even if source
// is resolved/rejected several times
tests.push(make_promise_test(
  function notification_once(test) {
    // The size of the test
    const SIZE = 10;
    const RESULT = "this is an arbitrary value";

    // Number of observers that yet need to be notified
    let expected = SIZE;

    // |true| once an observer has been notified
    let notified = [];

    // The promise observed
    let observed = Promise.defer();
    let result = Promise.defer();

    let install_observer = function install_observer(i) {
      observe_failures(observed.promise.then(
        function onSuccess(value) {
          do_check_true(!notified[i], "Ensuring that observer is notified at most once");
          notified[i] = true;

          do_check_eq(value, RESULT, "Ensuring that the observed value is correct");
          if (--expected == 0) {
            result.resolve();
          }
        }));
    };

    // Install a number of observers before resolving
    let i;
    for (i = 0; i < SIZE/2; ++i) {
      install_observer(i);
    }

    observed.resolve(RESULT);

    // Install remaining observers
    for (;i < SIZE; ++i) {
      install_observer(i);
    }

    // Resolve some more
    for (i = 0; i < 10; ++i) {
      observed.resolve(RESULT);
      observed.reject();
    }

    return result;
  }));

// Test that throwing an exception from a onResolve listener
// does not prevent other observers from receiving the notification
// of success.
tests.push(
  make_promise_test(function exceptions_do_not_stop_notifications(test)  {
    let source = Promise.defer();

    let exception_thrown = false;
    let exception_content = new Error("Boom!");

    let observer_1 = source.promise.then(
      function onResolve() {
        exception_thrown = true;
        throw exception_content;
      });

    let observer_2 = source.promise.then(
      function onResolve() {
        do_check_true(exception_thrown, "Second observer called after first observer has thrown");
      }
    );

    let result = observer_1.then(
      function onResolve() {
        do_throw("observer_1 should not have resolved");
      },
      function onReject(reason) {
        do_check_true(reason == exception_content, "Obtained correct rejection");
      }
    );

    source.resolve();
    return result;
  }
));

// Test that, once a promise is resolved, further resolve/reject
// are ignored.
tests.push(
  make_promise_test(function subsequent_resolves_are_ignored(test) {
    let deferred = Promise.defer();
    deferred.resolve(1);
    deferred.resolve(2);
    deferred.reject(3);

    let result = deferred.promise.then(
      function onResolve(value) {
        do_check_eq(value, 1, "Resolution chose the first value");
      },
      function onReject(reason) {
        do_throw("Obtained a rejection while the promise was already resolved");
      }
    );

    return result;
  }));

// Test that, once a promise is rejected, further resolve/reject
// are ignored.
tests.push(
  make_promise_test(function subsequent_rejects_are_ignored(test) {
    let deferred = Promise.defer();
    deferred.reject(1);
    deferred.reject(2);
    deferred.resolve(3);

    let result = deferred.promise.then(
      function onResolve() {
        do_throw("Obtained a resolution while the promise was already rejected");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "Rejection chose the first value");
      }
    );

    return result;
  }));

// Test that returning normally from a rejection recovers from the error
// and that listeners are informed of a success.
tests.push(
  make_promise_test(function recovery(test) {
    let boom = new Error("Boom!");
    let deferred = Promise.defer();
    const RESULT = "An arbitrary value";

    let promise = deferred.promise.then(
      function onResolve() {
        do_throw("A rejected promise should not resolve");
      },
      function onReject(reason) {
        do_check_true(reason == boom, "Promise was rejected with the correct error");
        return RESULT;
      }
    );

    promise = promise.then(
      function onResolve(value) {
        do_check_eq(value, RESULT, "Promise was recovered with the correct value");
      }
    );

    deferred.reject(boom);
    return promise;
  }));

// Test that returning a resolved promise from a onReject causes a resolution
// (recovering from the error) and that returning a rejected promise
// from a onResolve listener causes a rejection (raising an error).
tests.push(
  make_promise_test(function recovery_with_promise(test) {
    let boom = new Error("Arbitrary error");
    let deferred = Promise.defer();
    const RESULT = "An arbitrary value";
    const boom2 = new Error("Another arbitrary error");

    // return a resolved promise from a onReject listener
    let promise = deferred.promise.then(
      function onResolve() {
        do_throw("A rejected promise should not resolve");
      },
      function onReject(reason) {
        do_check_true(reason == boom, "Promise was rejected with the correct error");
        return Promise.resolve(RESULT);
      }
    );

    // return a rejected promise from a onResolve listener
    promise = promise.then(
      function onResolve(value) {
        do_check_eq(value, RESULT, "Promise was recovered with the correct value");
        return Promise.reject(boom2);
      }
    );

    promise = promise.catch(
      function onReject(reason) {
        do_check_eq(reason, boom2, "Rejection was propagated with the correct " +
                "reason, through a promise");
      }
    );

    deferred.reject(boom);
    return promise;
  }));

// Test that we can resolve with promises of promises
tests.push(
  make_promise_test(function test_propagation(test) {
    const RESULT = "Yet another arbitrary value";
    let d1 = Promise.defer();
    let d2 = Promise.defer();
    let d3 = Promise.defer();

    d3.resolve(d2.promise);
    d2.resolve(d1.promise);
    d1.resolve(RESULT);

    return d3.promise.then(
      function onSuccess(value) {
        do_check_eq(value, RESULT, "Resolution with a promise eventually yielded "
                + " the correct result");
      }
    );
  }));

// Test sequences of |then| and |catch|
tests.push(
  make_promise_test(function test_chaining(test) {
    let error_1 = new Error("Error 1");
    let error_2 = new Error("Error 2");
    let result_1 = "First result";
    let result_2 = "Second result";
    let result_3 = "Third result";

    let source = Promise.defer();

    let promise = source.promise.then().then();

    source.resolve(result_1);

    // Check that result_1 is correctly propagated
    promise = promise.then(
      function onSuccess(result) {
        do_check_eq(result, result_1, "Result was propagated correctly through " +
                " several applications of |then|");
        return result_2;
      }
    );

    // Check that returning from the promise produces a resolution
    promise = promise.catch(
      function onReject() {
        do_throw("Incorrect rejection");
      }
    );

    // ... and that the check did not alter the value
    promise = promise.then(
      function onResolve(value) {
        do_check_eq(value, result_2, "Result was propagated correctly once again");
      }
    );

    // Now the same kind of tests for rejections
    promise = promise.then(
      function onResolve() {
        throw error_1;
      }
    );

    promise = promise.then(
      function onResolve() {
        do_throw("Incorrect resolution: the exception should have caused a rejection");
      }
    );

    promise = promise.catch(
      function onReject(reason) {
        do_check_true(reason == error_1, "Reason was propagated correctly");
        throw error_2;
      }
    );

    promise = promise.catch(
      function onReject(reason) {
        do_check_true(reason == error_2, "Throwing an error altered the reason " +
            "as expected");
        return result_3;
      }
    );

    promise = promise.then(
      function onResolve(result) {
        do_check_eq(result, result_3, "Error was correctly recovered");
      }
    );

    return promise;
  }));

// Test that resolving with a rejected promise actually rejects
tests.push(
  make_promise_test(function resolve_to_rejected(test) {
    let source = Promise.defer();
    let error = new Error("Boom");

    let promise = source.promise.then(
      function onResolve() {
        do_throw("Incorrect call to onResolve listener");
      },
      function onReject(reason) {
        do_check_eq(reason, error, "Rejection lead to the expected reason");
      }
    );

    source.resolve(Promise.reject(error));

    return promise;
  }));

// Test that Promise.resolve resolves as expected
tests.push(
  make_promise_test(function test_resolve(test) {
    const RESULT = "arbitrary value";
    let p1 = Promise.resolve(RESULT);
    let p2 = Promise.resolve(p1);
    do_check_eq(p1, p2, "Promise.resolve used on a promise just returns the promise");

    return p1.then(
      function onResolve(result) {
        do_check_eq(result, RESULT, "Promise.resolve propagated the correct result");
      }
    );
  }));

// Test that Promise.resolve throws when its argument is an async function.
tests.push(
  make_promise_test(function test_promise_resolve_throws_with_async_function(test) {
    Assert.throws(() => Promise.resolve(Task.async(function* () {})),
                  /Cannot resolve a promise with an async function/);
    return Promise.resolve();
  }));

// Test that the code after "then" is always executed before the callbacks
tests.push(
  make_promise_test(function then_returns_before_callbacks(test) {
    let promise = Promise.resolve();

    let thenExecuted = false;

    promise = promise.then(
      function onResolve() {
        thenExecuted = true;
      }
    );

    do_check_false(thenExecuted);

    return promise;
  }));

// Test that chaining promises does not generate long stack traces
tests.push(
  make_promise_test(function chaining_short_stack(test) {
    let source = Promise.defer();
    let promise = source.promise;

    const NUM_ITERATIONS = 100;

    for (let i = 0; i < NUM_ITERATIONS; i++) {
      promise = promise.then(
        function onResolve(result) {
          return result + ".";
        }
      );
    }

    promise = promise.then(
      function onResolve(result) {
        // Check that the execution went as expected.
        let expectedString = new Array(1 + NUM_ITERATIONS).join(".");
        do_check_true(result == expectedString);

        // Check that we didn't generate one or more stack frames per iteration.
        let stackFrameCount = 0;
        let stackFrame = Components.stack;
        while (stackFrame) {
          stackFrameCount++;
          stackFrame = stackFrame.caller;
        }

        do_check_true(stackFrameCount < NUM_ITERATIONS);
      }
    );

    source.resolve("");

    return promise;
  }));

// Test that the values of the promise return by Promise.all() are kept in the
// given order even if the given promises are resolved in arbitrary order
tests.push(
  make_promise_test(function all_resolve(test) {
    let d1 = Promise.defer();
    let d2 = Promise.defer();
    let d3 = Promise.defer();

    d3.resolve(4);
    d2.resolve(2);
    do_execute_soon(() => d1.resolve(1));

    let promises = [d1.promise, d2.promise, 3, d3.promise];

    return Promise.all(promises).then(
      function onResolve([val1, val2, val3, val4]) {
        do_check_eq(val1, 1);
        do_check_eq(val2, 2);
        do_check_eq(val3, 3);
        do_check_eq(val4, 4);
      }
    );
  }));

// Test that rejecting one of the promises passed to Promise.all()
// rejects the promise return by Promise.all()
tests.push(
  make_promise_test(function all_reject(test) {
    let error = new Error("Boom");

    let d1 = Promise.defer();
    let d2 = Promise.defer();
    let d3 = Promise.defer();

    d3.resolve(3);
    d2.resolve(2);
    do_execute_soon(() => d1.reject(error));

    let promises = [d1.promise, d2.promise, d3.promise];

    return Promise.all(promises).then(
      function onResolve() {
        do_throw("Incorrect call to onResolve listener");
      },
      function onReject(reason) {
        do_check_eq(reason, error, "Rejection lead to the expected reason");
      }
    );
  }));

// Test that passing only values (not promises) to Promise.all()
// forwards them all as resolution values.
tests.push(
  make_promise_test(function all_resolve_no_promises(test) {
    try {
      Promise.all(null);
      do_check_true(false, "all() should only accept iterables");
    } catch (e) {
      do_check_true(true, "all() fails when first the arg is not an iterable");
    }

    let p1 = Promise.all([]).then(
      function onResolve(val) {
        do_check_true(Array.isArray(val) && val.length == 0);
      }
    );

    let p2 = Promise.all([1, 2, 3]).then(
      function onResolve([val1, val2, val3]) {
        do_check_eq(val1, 1);
        do_check_eq(val2, 2);
        do_check_eq(val3, 3);
      }
    );

    return Promise.all([p1, p2]);
  }));

// Test that Promise.all() handles non-array iterables
tests.push(
  make_promise_test(function all_iterable(test) {
    function* iterable() {
      yield 1;
      yield 2;
      yield 3;
    }

    return Promise.all(iterable()).then(
      function onResolve([val1, val2, val3]) {
        do_check_eq(val1, 1);
        do_check_eq(val2, 2);
        do_check_eq(val3, 3);
      },
      function onReject() {
        do_throw("all() unexpectedly rejected");
      }
    );
  }));

// Test that throwing from the iterable passed to Promise.all() rejects the
// promise returned by Promise.all()
tests.push(
  make_promise_test(function all_iterable_throws(test) {
    function* iterable() {
      throw 1;
    }

    return Promise.all(iterable()).then(
      function onResolve() {
        do_throw("all() unexpectedly resolved");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "all() rejects when the iterator throws");
      }
    );
  }));

// Test that Promise.race() resolves with the first available resolution value
tests.push(
  make_promise_test(function race_resolve(test) {
    let p1 = Promise.resolve(1);
    let p2 = Promise.resolve().then(() => 2);

    return Promise.race([p1, p2]).then(
      function onResolve(value) {
        do_check_eq(value, 1);
      }
    );
  }));

// Test that passing only values (not promises) to Promise.race() works
tests.push(
  make_promise_test(function race_resolve_no_promises(test) {
    try {
      Promise.race(null);
      do_check_true(false, "race() should only accept iterables");
    } catch (e) {
      do_check_true(true, "race() fails when first the arg is not an iterable");
    }

    return Promise.race([1, 2, 3]).then(
      function onResolve(value) {
        do_check_eq(value, 1);
      }
    );
  }));

// Test that Promise.race() never resolves when passed an empty iterable
tests.push(
  make_promise_test(function race_resolve_never(test) {
    return new Promise(resolve => {
      Promise.race([]).then(
        function onResolve() {
          do_throw("race() unexpectedly resolved");
        },
        function onReject() {
          do_throw("race() unexpectedly rejected");
        }
      );

      // Approximate "never" so we don't have to solve the halting problem.
      do_timeout(200, resolve);
    });
  }));

// Test that Promise.race() handles non-array iterables.
tests.push(
  make_promise_test(function race_iterable(test) {
    function* iterable() {
      yield 1;
      yield 2;
      yield 3;
    }

    return Promise.race(iterable()).then(
      function onResolve(value) {
        do_check_eq(value, 1);
      },
      function onReject() {
        do_throw("race() unexpectedly rejected");
      }
    );
  }));

// Test that throwing from the iterable passed to Promise.race() rejects the
// promise returned by Promise.race()
tests.push(
  make_promise_test(function race_iterable_throws(test) {
    function* iterable() {
      throw 1;
    }

    return Promise.race(iterable()).then(
      function onResolve() {
        do_throw("race() unexpectedly resolved");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "race() rejects when the iterator throws");
      }
    );
  }));

// Test that rejecting one of the promises passed to Promise.race() rejects the
// promise returned by Promise.race()
tests.push(
  make_promise_test(function race_reject(test) {
    let p1 = Promise.reject(1);
    let p2 = Promise.resolve(2);
    let p3 = Promise.resolve(3);

    return Promise.race([p1, p2, p3]).then(
      function onResolve() {
        do_throw("race() unexpectedly resolved");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "race() rejects when given a rejected promise");
      }
    );
  }));

// Test behavior of the Promise constructor.
tests.push(
  make_promise_test(function test_constructor(test) {
    try {
      new Promise(null);
      do_check_true(false, "Constructor should fail when not passed a function");
    } catch (e) {
      do_check_true(true, "Constructor fails when not passed a function");
    }

    let executorRan = false;
    let promise = new Promise(
      function executor(resolve, reject) {
        executorRan = true;
        do_check_eq(this, undefined);
        do_check_eq(typeof resolve, "function",
                    "resolve function should be passed to the executor");
        do_check_eq(typeof reject, "function",
                    "reject function should be passed to the executor");
      }
    );
    do_check_instanceof(promise, Promise);
    do_check_true(executorRan, "Executor should execute synchronously");

    // resolve a promise from the executor
    let resolvePromise = new Promise(
      function executor(resolve) {
        resolve(1);
      }
    ).then(
      function onResolve(value) {
        do_check_eq(value, 1, "Executor resolved with correct value");
      },
      function onReject() {
        do_throw("Executor unexpectedly rejected");
      }
    );

    // reject a promise from the executor
    let rejectPromise = new Promise(
      function executor(_, reject) {
        reject(1);
      }
    ).then(
      function onResolve() {
        do_throw("Executor unexpectedly resolved");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "Executor rejected with correct value");
      }
    );

    // throw from the executor, causing a rejection
    let throwPromise = new Promise(
      function executor() {
        throw 1;
      }
    ).then(
      function onResolve() {
        do_throw("Throwing inside an executor should not resolve the promise");
      },
      function onReject(reason) {
        do_check_eq(reason, 1, "Executor rejected with correct value");
      }
    );

    return Promise.all([resolvePromise, rejectPromise, throwPromise]);
  }));

// Test deadlock in Promise.jsm with nested event loops
// The scenario being tested is:
// promise_1.then({
//   do some work that will asynchronously signal done
//   start an event loop waiting for the done signal
// }
// where the async work uses resolution of a second promise to
// trigger the "done" signal. While this would likely work in a
// naive implementation, our constant-stack implementation needs
// a special case to avoid deadlock. Note that this test is
// sensitive to the implementation-dependent order in which then()
// clauses for two different promises are executed, so it is
// possible for other implementations to pass this test and still
// have similar deadlocks.
tests.push(
  make_promise_test(function promise_nested_eventloop_deadlock(test) {
    // Set up a (long enough to be noticeable) timeout to
    // exit the nested event loop and throw if the test run is hung
    let shouldExitNestedEventLoop = false;

    function event_loop() {
      let thr = Services.tm.mainThread;
      while (!shouldExitNestedEventLoop) {
        thr.processNextEvent(true);
      }
    }

    // I wish there was a way to cancel xpcshell do_timeout()s
    do_timeout(2000, () => {
      if (!shouldExitNestedEventLoop) {
        shouldExitNestedEventLoop = true;
        do_throw("Test timed out");
      }
    });

    let promise1 = Promise.resolve(1);
    let promise2 = Promise.resolve(2);

    do_print("Setting wait for first promise");
    promise1.then(value => {
      do_print("Starting event loop");
      event_loop();
    }, null);

    do_print("Setting wait for second promise");
    return promise2.catch(error => { return 3; })
    .then(
      count => {
        shouldExitNestedEventLoop = true;
      });
  }));

function wait_for_uncaught(aMustAppear, aTimeout = undefined) {
  let remaining = new Set();
  for (let k of aMustAppear) {
    remaining.add(k);
  }
  let deferred = Promise.defer();
  let print = do_print;
  let execute_soon = do_execute_soon;
  let observer = function({message, stack}) {
    let data = message + stack;
    print("Observing " + message + ", looking for " + aMustAppear.join(", "));
    for (let expected of remaining) {
      if (data.indexOf(expected) != -1) {
        print("I found " + expected);
        remaining.delete(expected);
      }
      if (remaining.size == 0 && observer) {
        Promise.Debugging.removeUncaughtErrorObserver(observer);
        observer = null;
        deferred.resolve();
      }
    }
  };
  Promise.Debugging.addUncaughtErrorObserver(observer);
  if (aTimeout) {
    do_timeout(aTimeout, function timeout() {
      if (observer) {
        Promise.Debugging.removeUncaughtErrorObserver(observer);
        observer = null;
      }
      deferred.reject(new Error("Timeout"));
    });
  }
  return deferred.promise;
}

// Test that uncaught errors are reported as uncaught
(function() {
  let make_string_rejection = function make_string_rejection() {
    let salt = (Math.random() * ( Math.pow(2, 24) - 1 ));
    let string = "This is an uncaught rejection " + salt;
    // Our error is not Error-like nor an nsIException, so the stack will
    // include the closure doing the actual rejection.
    return {mustFind: ["test_rejection_closure", string], error: string};
  };
  let make_num_rejection = function make_num_rejection() {
    let salt = (Math.random() * ( Math.pow(2, 24) - 1 ));
    // Our error is not Error-like nor an nsIException, so the stack will
    // include the closure doing the actual rejection.
    return {mustFind: ["test_rejection_closure", salt], error: salt};
  };
  let make_undefined_rejection = function make_undefined_rejection() {
    // Our error is not Error-like nor an nsIException, so the stack will
    // include the closure doing the actual rejection.
    return {mustFind: ["test_rejection_closure"], error: undefined};
  };
  let make_error_rejection = function make_error_rejection() {
    let salt = (Math.random() * ( Math.pow(2, 24) - 1 ));
    let error = new Error("This is an uncaught error " + salt);
    return {
      mustFind: [error.message, error.fileName, error.lineNumber, error.stack],
      error: error
    };
  };
  let make_exception_rejection = function make_exception_rejection() {
    let salt = (Math.random() * ( Math.pow(2, 24) - 1 ));
    let exn = new Components.Exception("This is an uncaught exception " + salt,
                                       Components.results.NS_ERROR_NOT_AVAILABLE);
    return {
      mustFind: [exn.message, exn.filename, exn.lineNumber, exn.location.toString()],
      error: exn
    };
  };
  for (let make_rejection of [make_string_rejection,
    make_num_rejection,
    make_undefined_rejection,
    make_error_rejection,
    make_exception_rejection]) {
      let {mustFind, error} = make_rejection();
      let name = make_rejection.name;
      tests.push(make_promise_test(function test_uncaught_is_reported() {
        do_print("Testing with rejection " + name);
        let promise = wait_for_uncaught(mustFind);
        (function test_rejection_closure() {
          // For the moment, we cannot be absolutely certain that a value is
          // garbage-collected, even if it is not referenced anymore, due to
          // the conservative stack-scanning algorithm.
          //
          // To be _almost_ certain that a value will be garbage-collected, we
          // 1. isolate that value in an anonymous closure;
          // 2. allocate 100 values instead of 1 (gc-ing a single value from
          //    these is sufficient for the test);
          // 3. place everything in a loop, as the JIT typically reuses memory;
          // 4. call all the GC methods we can.
          //
          // Unfortunately, we might still have intermittent failures,
          // materialized as timeouts.
          //
          for (let i = 0; i < 100; ++i) {
            Promise.reject(error);
          }
        })();
        do_print("Posted all rejections");
        Components.utils.forceGC();
        Components.utils.forceCC();
        Components.utils.forceShrinkingGC();
        return promise;
      }));
  }
})();


// Test that caught errors are not reported as uncaught
tests.push(
make_promise_test(function test_caught_is_not_reported() {
  let salt = (Math.random() * ( Math.pow(2, 24) - 1 ));
  let promise = wait_for_uncaught([salt], 500);
  (function() {
    let uncaught = Promise.reject("This error, on the other hand, is caught " + salt);
    uncaught.catch(function() { /* ignore rejection */ });
    uncaught = null;
  })();
  // Isolate this in a function to increase likelihood that the gc will
  // realise that |uncaught| has remained uncaught.
  Components.utils.forceGC();

  return promise.then(function onSuccess() {
    throw new Error("This error was caught and should not have been reported");
  }, function onError() {
    do_print("The caught error was not reported, all is fine");
  }
  );
}));

// Bug 1033406 - Make sure Promise works even after freezing.
tests.push(
  make_promise_test(function test_freezing_promise(test) {
    var p = new Promise(function executor(resolve) {
      do_execute_soon(resolve);
    });
    Object.freeze(p);
    return p;
  })
);

function run_test()
{
  do_test_pending();
  run_promise_tests(tests, do_test_finished);
}
