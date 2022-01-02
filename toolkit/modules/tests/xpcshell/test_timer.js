/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests exports from Timer.jsm

var imported = {};
ChromeUtils.import("resource://gre/modules/Timer.jsm", imported);

add_task(async function test_setTimeout() {
  let timeout1 = imported.setTimeout(
    () => do_throw("Should not be called"),
    100
  );
  Assert.equal(typeof timeout1, "number", "setTimeout returns a number");
  Assert.greater(timeout1, 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise(resolve => {
    let timeout2 = imported.setTimeout(
      (param1, param2) => {
        Assert.ok(true, "Should be called");
        Assert.equal(param1, 5, "first parameter is correct");
        Assert.equal(param2, "test", "second parameter is correct");
        resolve();
      },
      100,
      5,
      "test"
    );

    Assert.equal(typeof timeout2, "number", "setTimeout returns a number");
    Assert.greater(timeout2, 0, "setTimeout returns a positive number");
    Assert.notEqual(
      timeout1,
      timeout2,
      "Calling setTimeout again returns a different value"
    );
  });
});

add_task(async function test_setTimeoutWithTarget() {
  let target = Services.mainThreadEventTarget;
  let timeout1 = imported.setTimeoutWithTarget(
    () => do_throw("Should not be called"),
    100,
    target
  );
  Assert.equal(typeof timeout1, "number", "setTimeout returns a number");
  Assert.greater(timeout1, 0, "setTimeout returns a positive number");

  imported.clearTimeout(timeout1);

  await new Promise(resolve => {
    let timeout2 = imported.setTimeoutWithTarget(
      (param1, param2) => {
        Assert.ok(true, "Should be called");
        Assert.equal(param1, 5, "first parameter is correct");
        Assert.equal(param2, "test", "second parameter is correct");
        resolve();
      },
      100,
      target,
      5,
      "test"
    );

    Assert.equal(typeof timeout2, "number", "setTimeout returns a number");
    Assert.greater(timeout2, 0, "setTimeout returns a positive number");
    Assert.notEqual(
      timeout1,
      timeout2,
      "Calling setTimeout again returns a different value"
    );
  });
});

add_task(async function test_setInterval() {
  let interval1 = imported.setInterval(
    () => do_throw("Should not be called!"),
    100
  );
  Assert.equal(typeof interval1, "number", "setInterval returns a number");
  Assert.greater(interval1, 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise(resolve => {
    let interval2 = imported.setInterval(
      (param1, param2) => {
        Assert.ok(true, "Should be called");
        Assert.equal(param1, 15, "first parameter is correct");
        Assert.equal(param2, "hola", "second parameter is correct");
        if (calls >= EXPECTED_CALLS) {
          imported.clearInterval(interval2);
          resolve();
        }
        calls++;
      },
      100,
      15,
      "hola"
    );
  });
});

add_task(async function test_setIntervalWithTarget() {
  let target = Services.mainThreadEventTarget;
  let interval1 = imported.setIntervalWithTarget(
    () => do_throw("Should not be called!"),
    100,
    target
  );
  Assert.equal(typeof interval1, "number", "setInterval returns a number");
  Assert.greater(interval1, 0, "setTimeout returns a positive number");

  imported.clearInterval(interval1);

  const EXPECTED_CALLS = 5;
  let calls = 0;

  await new Promise(resolve => {
    let interval2 = imported.setIntervalWithTarget(
      (param1, param2) => {
        Assert.ok(true, "Should be called");
        Assert.equal(param1, 15, "first parameter is correct");
        Assert.equal(param2, "hola", "second parameter is correct");
        if (calls >= EXPECTED_CALLS) {
          imported.clearInterval(interval2);
          resolve();
        }
        calls++;
      },
      100,
      target,
      15,
      "hola"
    );
  });
});

add_task(async function test_setTimeoutNonFunction() {
  Assert.throws(() => {
    imported.setTimeout({}, 0);
  }, /callback is not a function in setTimeout/);
});

add_task(async function test_setIntervalNonFunction() {
  Assert.throws(() => {
    imported.setInterval({}, 0);
  }, /callback is not a function in setInterval/);
});

add_task(async function test_setTimeoutWithTargetNonFunction() {
  Assert.throws(() => {
    imported.setTimeoutWithTarget({}, 0);
  }, /callback is not a function in setTimeout/);
});

add_task(async function test_setIntervalWithTargetNonFunction() {
  Assert.throws(() => {
    imported.setIntervalWithTarget({}, 0);
  }, /callback is not a function in setInterval/);
});

add_task(async function test_requestIdleCallback() {
  let request1 = imported.requestIdleCallback(() =>
    do_throw("Should not be called")
  );
  Assert.equal(
    typeof request1,
    "number",
    "requestIdleCallback returns a number"
  );
  Assert.greater(request1, 0, "setTimeout returns a positive number");

  imported.cancelIdleCallback(request1);

  await new Promise(resolve => {
    let request2 = imported.requestIdleCallback(
      deadline => {
        Assert.ok(true, "Should be called");
        Assert.equal(
          typeof deadline.didTimeout,
          "boolean",
          "deadline parameter has .didTimeout property"
        );
        Assert.equal(
          typeof deadline.timeRemaining(),
          "number",
          "deadline parameter has .timeRemaining() function"
        );
        resolve();
      },
      { timeout: 100 }
    );

    Assert.equal(
      typeof request2,
      "number",
      "requestIdleCallback returns a number"
    );
    Assert.greater(
      request2,
      0,
      "requestIdleCallback returns a positive number"
    );
    Assert.notEqual(
      request1,
      request2,
      "Calling requestIdleCallback again returns a different value"
    );
  });
});
