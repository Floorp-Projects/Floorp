/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the Task.jsm module.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

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
  Services.tm.mainThread.dispatch(() => deferred.resolve(aValue),
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


////////////////// Test rewriting of stack traces

// Backup Task.Debuggin.maintainStack.
// Will be restored by `exit_stack_tests`.
var maintainStack;
add_test(function enter_stack_tests() {
  maintainStack = Task.Debugging.maintainStack;
  Task.Debugging.maintainStack = true;
  run_next_test();
});


/**
 * Ensure that a list of frames appear in a stack, in the right order
 */
function do_check_rewritten_stack(frames, ex) {
  do_print("Checking that the expected frames appear in the right order");
  do_print(frames.join(", "));
  let stack = ex.stack;
  do_print(stack);

  let framesFound = 0;
  let lineNumber = 0;
  let reLine = /([^\r\n])+/g;
  let match;
  while (framesFound < frames.length && (match = reLine.exec(stack))) {
    let line = match[0];
    let frame = frames[framesFound];
    do_print("Searching for " + frame + " in line " + line);
    if (line.indexOf(frame) != -1) {
      do_print("Found " + frame);
      ++framesFound;
    } else {
      do_print("Didn't find " + frame);
    }
  }

  if (framesFound >= frames.length) {
    return;
  }
  do_throw("Did not find: " + frames.slice(framesFound).join(", ") +
           " in " + stack.substr(reLine.lastIndex));

  do_print("Ensuring that we have removed Task.jsm, Promise.jsm");
  do_check_true(stack.indexOf("Task.jsm") == -1);
  do_check_true(stack.indexOf("Promise.jsm") == -1);
  do_check_true(stack.indexOf("Promise-backend.js") == -1);
}


// Test that we get an acceptable rewritten stack when we launch
// an error in a Task.spawn.
add_test(function test_spawn_throw_stack() {
  Task.spawn(function* task_spawn_throw_stack() {
    for (let i = 0; i < 5; ++i) {
      yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
    }
    throw new Error("BOOM");
  }).then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_spawn_throw_stack",
                              "test_spawn_throw_stack"],
                             ex);
    run_next_test();
  });
});

// Test that we get an acceptable rewritten stack when we yield
// a rejection in a Task.spawn.
add_test(function test_spawn_yield_reject_stack() {
  Task.spawn(function* task_spawn_yield_reject_stack() {
    for (let i = 0; i < 5; ++i) {
      yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
    }
    yield Promise.reject(new Error("BOOM"));
  }).then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_spawn_yield_reject_stack",
                              "test_spawn_yield_reject_stack"],
                              ex);
    run_next_test();
  });
});

// Test that we get an acceptable rewritten stack when we launch
// an error in a Task.async function.
add_test(function test_async_function_throw_stack() {
  let task_async_function_throw_stack = Task.async(function*() {
    for (let i = 0; i < 5; ++i) {
      yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
    }
    throw new Error("BOOM");
  })().then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_async_function_throw_stack",
                              "test_async_function_throw_stack"],
                             ex);
    run_next_test();
  });
});

// Test that we get an acceptable rewritten stack when we launch
// an error in a Task.async function.
add_test(function test_async_function_yield_reject_stack() {
  let task_async_function_yield_reject_stack = Task.async(function*() {
    for (let i = 0; i < 5; ++i) {
      yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
    }
    yield Promise.reject(new Error("BOOM"));
  })().then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_async_function_yield_reject_stack",
                              "test_async_function_yield_reject_stack"],
                              ex);
    run_next_test();
  });
});

// Test that we get an acceptable rewritten stack when we launch
// an error in a Task.async function.
add_test(function test_async_method_throw_stack() {
  let object = {
   task_async_method_throw_stack: Task.async(function*() {
    for (let i = 0; i < 5; ++i) {
      yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
    }
    throw new Error("BOOM");
   })
  };
  object.task_async_method_throw_stack().then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_async_method_throw_stack",
                              "test_async_method_throw_stack"],
                             ex);
    run_next_test();
  });
});

// Test that we get an acceptable rewritten stack when we launch
// an error in a Task.async function.
add_test(function test_async_method_yield_reject_stack() {
  let object = {
    task_async_method_yield_reject_stack: Task.async(function*() {
      for (let i = 0; i < 5; ++i) {
        yield Promise.resolve(); // Without stack rewrite, this would lose valuable information
      }
      yield Promise.reject(new Error("BOOM"));
    })
  };
  object.task_async_method_yield_reject_stack().then(do_throw, function(ex) {
    do_check_rewritten_stack(["task_async_method_yield_reject_stack",
                              "test_async_method_yield_reject_stack"],
                              ex);
    run_next_test();
  });
});

// Test that two tasks whose execution takes place interleaved do not capture each other's stack.
add_test(function test_throw_stack_do_not_capture_the_wrong_task() {
  for (let iter_a of [3, 4, 5]) { // Vary the interleaving
    for (let iter_b of [3, 4, 5]) {
      Task.spawn(function* task_a() {
        for (let i = 0; i < iter_a; ++i) {
          yield Promise.resolve();
        }
        throw new Error("BOOM");
      }).then(do_throw, function(ex) {
        do_check_rewritten_stack(["task_a",
                                  "test_throw_stack_do_not_capture_the_wrong_task"],
                                  ex);
        do_check_true(!ex.stack.includes("task_b"));
        run_next_test();
      });
      Task.spawn(function* task_b() {
        for (let i = 0; i < iter_b; ++i) {
          yield Promise.resolve();
        }
      });
    }
  }
});

// Put things together
add_test(function test_throw_complex_stack()
{
  // Setup the following stack:
  //    inner_method()
  //    task_3()
  //    task_2()
  //    task_1()
  //    function_3()
  //    function_2()
  //    function_1()
  //    test_throw_complex_stack()
  (function function_1() {
    return (function function_2() {
      return (function function_3() {
        return Task.spawn(function* task_1() {
          yield Promise.resolve();
          try {
            yield Task.spawn(function* task_2() {
              yield Promise.resolve();
              yield Task.spawn(function* task_3() {
                yield Promise.resolve();
                  let inner_object = {
                    inner_method: Task.async(function*() {
                      throw new Error("BOOM");
                    })
                  };
                  yield Promise.resolve();
                  yield inner_object.inner_method();
                });
              });
            } catch (ex) {
              yield Promise.resolve();
              throw ex;
            }
          });
        })();
      })();
  })().then(
    () => do_throw("Shouldn't have succeeded"),
    (ex) => {
      let expect = ["inner_method",
        "task_3",
        "task_2",
        "task_1",
        "function_3",
        "function_2",
        "function_1",
        "test_throw_complex_stack"];
      do_check_rewritten_stack(expect, ex);

      run_next_test();
    });
});

add_test(function test_without_maintainStack() {
  do_print("Calling generateReadableStack without a Task");
  Task.Debugging.generateReadableStack(new Error("Not a real error"));

  Task.Debugging.maintainStack = false;

  do_print("Calling generateReadableStack with neither a Task nor maintainStack");
  Task.Debugging.generateReadableStack(new Error("Not a real error"));

  do_print("Calling generateReadableStack without maintainStack");
  Task.spawn(function*() {
    Task.Debugging.generateReadableStack(new Error("Not a real error"));
    run_next_test();
  });
});

add_test(function exit_stack_tests() {
  Task.Debugging.maintainStack = maintainStack;
  run_next_test();
});

