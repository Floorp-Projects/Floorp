  /* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/PromiseUtils.jsm");
Components.utils.import("resource://gre/modules/Timer.jsm");

// Tests for PromiseUtils.jsm
function run_test() {
  run_next_test();
}

/* Tests for the case when arguments to resolveOrTimeout
 * are not of correct type */
add_task(function* test_wrong_arguments() {
  let p = new Promise((resolve, reject) => {});
  // for the first argument
  Assert.throws(() => PromiseUtils.resolveOrTimeout("string", 200), /first argument <promise> must be a Promise object/,
                "TypeError thrown because first argument is not a Promsie object");
  // for second argument
  Assert.throws(() => PromiseUtils.resolveOrTimeout(p, "string"), /second argument <delay> must be a positive number/,
                "TypeError thrown because second argument is not a positive number");
  // for the third argument
  Assert.throws(() => PromiseUtils.resolveOrTimeout(p, 200, "string"), /third optional argument <rejection> must be a function/,
                "TypeError thrown because thrird argument is not a function");
});

/* Tests for the case when the optional third argument is not provided
 * In that case the returned promise rejects with a default Error */
add_task(function* test_optional_third_argument() {
  let p = new Promise((resolve, reject) => {});
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200), /Promise Timeout/, "Promise rejects with a default Error");
});

/* Test for the case when the passed promise resolves quickly
 * In that case the returned promise also resolves with the same value */
add_task(function* test_resolve_quickly() {
  let p = new Promise((resolve, reject) => setTimeout(() => resolve("Promise is resolved"), 20));
  let result = yield PromiseUtils.resolveOrTimeout(p, 200);
  Assert.equal(result, "Promise is resolved", "Promise resolves quickly");
});

/* Test for the case when the passed promise rejects quickly
 * In that case the returned promise also rejects with the same value */
add_task(function* test_reject_quickly() {
  let p = new Promise((resolve, reject) => setTimeout(() => reject("Promise is rejected"), 20));
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200), /Promise is rejected/, "Promise rejects quickly");
});

/* Tests for the case when the passed promise doesn't settle
 * and rejection returns string/object/undefined */
add_task(function* test_rejection_function() {
  let p = new Promise((resolve, reject) => {});
  // for rejection returning string
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200, () => {
    return "Rejection returned a string";
  }), /Rejection returned a string/, "Rejection returned a string");

  // for rejection returning object
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200, () => {
    return {Name:"Promise"};
  }), Object, "Rejection returned an object");

  // for rejection returning undefined
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200, () => {
    return;
  }), undefined, "Rejection returned undefined");
});

/* Tests for the case when the passed promise doesn't settles
 * and rejection throws an error */
add_task(function* test_rejection_throw_error() {
  let p = new Promise((resolve, reject) => {});
  yield Assert.rejects(PromiseUtils.resolveOrTimeout(p, 200, () => {
    throw new Error("Rejection threw an Error");
  }), /Rejection threw an Error/, "Rejection threw an error");
});