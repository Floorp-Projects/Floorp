/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the Task.jsm module.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");

/**
 * Returns a promise that will be resolved with the given value, when an event
 * posted on the event loop of the main thread is processed.
 */
function promiseResolvedLater(aValue) {
  let deferred = Promise.defer();
  Services.tm.mainThread.dispatch(function () deferred.resolve(aValue),
                                  Ci.nsIThread.DISPATCH_NORMAL);
  return deferred.promise;
}

////////////////////////////////////////////////////////////////////////////////
/// Tests

function run_test()
{
  run_next_test();
}

add_test(function test_normal()
{
  Task.spawn(function () {
    let result = yield Promise.resolve("Value");
    for (let i = 0; i < 3; i++) {
      result += yield promiseResolvedLater("!");
    }
    throw new Task.Result("Task result: " + result);
  }).then(function (result) {
    do_check_eq("Task result: Value!!!", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_exceptions()
{
  Task.spawn(function () {
    try {
      yield Promise.reject("Rejection result by promise.");
      do_throw("Exception expected because the promise was rejected.");
    } catch (ex) {
      // We catch this exception now, we will throw a different one later.
      do_check_eq("Rejection result by promise.", ex);
    }
    throw new Error("Exception uncaught by task.");
  }).then(function (result) {
    do_throw("Unexpected success!");
  }, function (ex) {
    do_check_eq("Exception uncaught by task.", ex.message);
    run_next_test();
  });
});

add_test(function test_recursion()
{
  function task_fibonacci(n) {
    throw new Task.Result(n < 2 ? n : (yield task_fibonacci(n - 1)) +
                                      (yield task_fibonacci(n - 2)));
  };

  Task.spawn(task_fibonacci(6)).then(function (result) {
    do_check_eq(8, result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_primitive()
{
  function fibonacci(n) {
    return n < 2 ? n : fibonacci(n - 1) + fibonacci(n - 2);
  };

  // Polymorphism between task and non-task functions (see "test_recursion").
  Task.spawn(fibonacci(6)).then(function (result) {
    do_check_eq(8, result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_function()
{
  Task.spawn(function () {
    return "This is not a generator.";
  }).then(function (result) {
    do_check_eq("This is not a generator.", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_function_this()
{
  Task.spawn(function () {
    return this;
  }).then(function (result) {
    // Since the task function wasn't defined in strict mode, its "this" object
    // should be the same as the "this" object in this function, i.e. the global
    // object.
    do_check_eq(result, this);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_function_this_strict()
{
  "use strict";
  Task.spawn(function () {
    return this;
  }).then(function (result) {
    // Since the task function was defined in strict mode, its "this" object
    // should be undefined.
    do_check_eq(typeof(result), "undefined");
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_function_returning_promise()
{
  Task.spawn(function () {
    return promiseResolvedLater("Resolution value.");
  }).then(function (result) {
    do_check_eq("Resolution value.", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_spawn_function_exceptions()
{
  Task.spawn(function () {
    throw new Error("Exception uncaught by task.");
  }).then(function (result) {
    do_throw("Unexpected success!");
  }, function (ex) {
    do_check_eq("Exception uncaught by task.", ex.message);
    run_next_test();
  });
});

add_test(function test_spawn_function_taskresult()
{
  Task.spawn(function () {
    throw new Task.Result("Task result");
  }).then(function (result) {
    do_check_eq("Task result", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_yielded_undefined()
{
  Task.spawn(function () {
    yield;
    throw new Task.Result("We continued correctly.");
  }).then(function (result) {
    do_check_eq("We continued correctly.", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_yielded_primitive()
{
  Task.spawn(function () {
    throw new Task.Result("Primitive " + (yield "value."));
  }).then(function (result) {
    do_check_eq("Primitive value.", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_star_normal()
{
  Task.spawn(function* () {
    let result = yield Promise.resolve("Value");
    for (let i = 0; i < 3; i++) {
      result += yield promiseResolvedLater("!");
    }
    return "Task result: " + result;
  }).then(function (result) {
    do_check_eq("Task result: Value!!!", result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_star_exceptions()
{
  Task.spawn(function* () {
    try {
      yield Promise.reject("Rejection result by promise.");
      do_throw("Exception expected because the promise was rejected.");
    } catch (ex) {
      // We catch this exception now, we will throw a different one later.
      do_check_eq("Rejection result by promise.", ex);
    }
    throw new Error("Exception uncaught by task.");
  }).then(function (result) {
    do_throw("Unexpected success!");
  }, function (ex) {
    do_check_eq("Exception uncaught by task.", ex.message);
    run_next_test();
  });
});

add_test(function test_star_recursion()
{
  function* task_fibonacci(n) {
    return n < 2 ? n : (yield task_fibonacci(n - 1)) +
                       (yield task_fibonacci(n - 2));
  };

  Task.spawn(task_fibonacci(6)).then(function (result) {
    do_check_eq(8, result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_mixed_legacy_and_star()
{
  Task.spawn(function* () {
    return yield (function() {
      throw new Task.Result(yield 5);
    })();
  }).then(function (result) {
    do_check_eq(5, result);
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_async_function_from_generator()
{
  Task.spawn(function* () {
    let object = {
      asyncFunction: Task.async(function* (param) {
        do_check_eq(this, object);
        return param;
      })
    };

    // Ensure the async function returns a promise that resolves as expected.
    do_check_eq((yield object.asyncFunction(1)), 1);

    // Ensure a second call to the async function also returns such a promise.
    do_check_eq((yield object.asyncFunction(3)), 3);
  }).then(function () {
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_async_function_from_function()
{
  Task.spawn(function* () {
    return Task.spawn(function* () {
      let object = {
        asyncFunction: Task.async(function (param) {
          do_check_eq(this, object);
          return param;
        })
      };

      // Ensure the async function returns a promise that resolves as expected.
      do_check_eq((yield object.asyncFunction(5)), 5);

      // Ensure a second call to the async function also returns such a promise.
      do_check_eq((yield object.asyncFunction(7)), 7);
    });
  }).then(function () {
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_async_function_that_throws_rejects_promise()
{
  Task.spawn(function* () {
    let object = {
      asyncFunction: Task.async(function* () {
        throw "Rejected!";
      })
    };

    yield object.asyncFunction();
  }).then(function () {
    do_throw("unexpected success calling async function that throws error");
  }, function (ex) {
    do_check_eq(ex, "Rejected!");
    run_next_test();
  });
});

add_test(function test_async_return_function()
{
  Task.spawn(function* () {
    // Ensure an async function that returns a function resolves to the function
    // itself instead of calling the function and resolving to its return value.
    return Task.spawn(function* () {
      let returnValue = function () {
        return "These aren't the droids you're looking for.";
      };

      let asyncFunction = Task.async(function () {
        return returnValue;
      });

      do_check_eq((yield asyncFunction()), returnValue);
    });
  }).then(function () {
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_async_throw_argument_not_function()
{
  Task.spawn(function* () {
    // Ensure Task.async throws if its aTask argument is not a function.
    Assert.throws(() => Task.async("not a function"),
                  /aTask argument must be a function/);
  }).then(function () {
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});

add_test(function test_async_throw_on_function_in_place_of_promise()
{
  Task.spawn(function* () {
    // Ensure Task.spawn throws if passed an async function.
    Assert.throws(() => Task.spawn(Task.async(function* () {})),
                  /Cannot use an async function in place of a promise/);
  }).then(function () {
    run_next_test();
  }, function (ex) {
    do_throw("Unexpected error: " + ex);
  });
});
