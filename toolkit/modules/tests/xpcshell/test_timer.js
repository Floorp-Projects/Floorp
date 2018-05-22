/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests exports from Timer.jsm

var imported = {};
ChromeUtils.import("resource://gre/modules/Timer.jsm", imported);

add_task(async function test_setTimeout() {
  let timeout1 = imported.setTimeout(() => do_throw("Should not be called"), 100);
  Assert.equal(typeof timeout1, "number", "setTimeout returns a number");
  Assert.ok(timeout1 > 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise((resolve) => {
    let timeout2 = imported.setTimeout((param1, param2) => {
      Assert.ok(true, "Should be called");
      Assert.equal(param1, 5, "first parameter is correct");
      Assert.equal(param2, "test", "second parameter is correct");
      resolve();
    }, 100, 5, "test");

    Assert.equal(typeof timeout2, "number", "setTimeout returns a number");
    Assert.ok(timeout2 > 0, "setTimeout returns a positive number");
    Assert.notEqual(timeout1, timeout2, "Calling setTimeout again returns a different value");
  });
});

add_task(async function test_setTimeoutWithTarget() {
  let target = Services.systemGroupEventTarget;
  let timeout1 = imported.setTimeoutWithTarget(
    () => do_throw("Should not be called"), 100, target);
  Assert.equal(typeof timeout1, "number", "setTimeout returns a number");
  Assert.ok(timeout1 > 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise((resolve) => {
    let timeout2 = imported.setTimeoutWithTarget((param1, param2) => {
      Assert.ok(true, "Should be called");
      Assert.equal(param1, 5, "first parameter is correct");
      Assert.equal(param2, "test", "second parameter is correct");
      resolve();
    }, 100, target, 5, "test");

    Assert.equal(typeof timeout2, "number", "setTimeout returns a number");
    Assert.ok(timeout2 > 0, "setTimeout returns a positive number");
    Assert.notEqual(timeout1, timeout2, "Calling setTimeout again returns a different value");
  });
});

add_task(async function test_setInterval() {
  let interval1 = imported.setInterval(() => do_throw("Should not be called!"), 100);
  Assert.equal(typeof interval1, "number", "setInterval returns a number");
  Assert.ok(interval1 > 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise((resolve) => {
    imported.setInterval((param1, param2) => {
      Assert.ok(true, "Should be called");
      Assert.equal(param1, 15, "first parameter is correct");
      Assert.equal(param2, "hola", "second parameter is correct");
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
  Assert.equal(typeof interval1, "number", "setInterval returns a number");
  Assert.ok(interval1 > 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise((resolve) => {
    imported.setIntervalWithTarget((param1, param2) => {
      Assert.ok(true, "Should be called");
      Assert.equal(param1, 15, "first parameter is correct");
      Assert.equal(param2, "hola", "second parameter is correct");
      if (calls >= EXPECTED_CALLS) {
        resolve();
      }
      calls++;
    }, 100, target, 15, "hola");
  });
});

add_task(async function test_setTimeoutNonFunction() {
  Assert.throws(() => { imported.setTimeout({}, 0); },
                /callback is not a function in setTimeout/);
});

add_task(async function test_setIntervalNonFunction() {
  Assert.throws(() => { imported.setInterval({}, 0); },
                /callback is not a function in setInterval/);
});

add_task(async function test_setTimeoutWithTargetNonFunction() {
  Assert.throws(() => { imported.setTimeoutWithTarget({}, 0); },
                /callback is not a function in setTimeout/);
});

add_task(async function test_setIntervalWithTargetNonFunction() {
  Assert.throws(() => { imported.setIntervalWithTarget({}, 0); },
                /callback is not a function in setInterval/);
});
