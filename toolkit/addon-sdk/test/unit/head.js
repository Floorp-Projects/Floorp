/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */


Components.utils.import("resource://gre/modules/commonjs/promise/core.js");

let run_promise_tests = function run_promise_tests(tests, cb) {
  let timer = Components.classes["@mozilla.org/timer;1"]
     .createInstance(Components.interfaces.nsITimer);
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

let make_promise_test = function(test) {
  return function runtest() {
    do_print("Test starting: " + test);
    try {
      let result = test();
      if (result && "promise" in result) {
        result = result.promise;
      }
      if (!result || !("then" in result)) {
        let exn;
        try {
          do_throw("Test " + test + " did not return a promise: " + result);
        } catch (x) {
          exn = x;
        }
        return Promise.reject(exn);
      }
      // The test returns a promise
      result = result.then(
        // Test complete
        function onResolve() {
          do_print("Test complete: " + test);
        },
        // The test failed with an unexpected error
        function onReject(err) {
          let detail;
          if (err && typeof err == "object" && "stack" in err) {
            detail = err.stack;
          } else {
            detail = "(no stack)";
          }
          do_throw("Test " + test + " rejected with the following reason: "
              + err + detail);
      });
      return result;
    } catch (x) {
      // The test failed because of an error outside of a promise
      do_throw("Error in body of test " + test + ": " + x + " at " + x.stack);
      return Promise.reject();
    }
  };
};

