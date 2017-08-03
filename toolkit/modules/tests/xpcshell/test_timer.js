/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests exports from Timer.jsm

var imported = {};
Components.utils.import("resource://gre/modules/Timer.jsm", imported);

function run_test() {
  run_next_test();
}

add_task(async function test_setTimeout() {
  let timeout1 = imported.setTimeout(() => do_throw("Should not be called"), 100);
  do_check_eq(typeof timeout1, "number", "setTimeout returns a number");
  do_check_true(timeout1 > 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise((resolve) => {
    let timeout2 = imported.setTimeout((param1, param2) => {
      do_check_true(true, "Should be called");
      do_check_eq(param1, 5, "first parameter is correct");
      do_check_eq(param2, "test", "second parameter is correct");
      resolve();
    }, 100, 5, "test");

    do_check_eq(typeof timeout2, "number", "setTimeout returns a number");
    do_check_true(timeout2 > 0, "setTimeout returns a positive number");
    do_check_neq(timeout1, timeout2, "Calling setTimeout again returns a different value");
  });
});

add_task(async function test_setTimeoutWithTarget() {
  let target = Services.systemGroupEventTarget;
  let timeout1 = imported.setTimeoutWithTarget(
    () => do_throw("Should not be called"), 100, target);
  do_check_eq(typeof timeout1, "number", "setTimeout returns a number");
  do_check_true(timeout1 > 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise((resolve) => {
    let timeout2 = imported.setTimeoutWithTarget((param1, param2) => {
      do_check_true(true, "Should be called");
      do_check_eq(param1, 5, "first parameter is correct");
      do_check_eq(param2, "test", "second parameter is correct");
      resolve();
    }, 100, target, 5, "test");

    do_check_eq(typeof timeout2, "number", "setTimeout returns a number");
    do_check_true(timeout2 > 0, "setTimeout returns a positive number");
    do_check_neq(timeout1, timeout2, "Calling setTimeout again returns a different value");
  });
});

add_task(async function test_setInterval() {
  let interval1 = imported.setInterval(() => do_throw("Should not be called!"), 100);
  do_check_eq(typeof interval1, "number", "setInterval returns a number");
  do_check_true(interval1 > 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise((resolve) => {
    imported.setInterval((param1, param2) => {
      do_check_true(true, "Should be called");
      do_check_eq(param1, 15, "first parameter is correct");
      do_check_eq(param2, "hola", "second parameter is correct");
      if (calls >= EXPECTED_CALLS) {
        resolve();
      }
      calls++;
    }, 100, 15, "hola");
  });
});

add_task(async function test_setIntervalWithTarget() {
  let target = Services.systemGroupEventTarget;
  let interval1 = imported.setIntervalWithTarget(
    () => do_throw("Should not be called!"), 100, target);
  do_check_eq(typeof interval1, "number", "setInterval returns a number");
  do_check_true(interval1 > 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise((resolve) => {
    imported.setIntervalWithTarget((param1, param2) => {
      do_check_true(true, "Should be called");
      do_check_eq(param1, 15, "first parameter is correct");
      do_check_eq(param2, "hola", "second parameter is correct");
      if (calls >= EXPECTED_CALLS) {
        resolve();
      }
      calls++;
    }, 100, target, 15, "hola");
  });
});
