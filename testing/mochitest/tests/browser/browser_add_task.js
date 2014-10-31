/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let test1Complete = false;
let test2Complete = false;

function executeWithTimeout() {
  return new Promise(resolve =>
    executeSoon(function() {
      ok(true, "we get here after a timeout");
      resolve();
    })
  );
}

add_task(function asyncTest_no1() {
  yield executeWithTimeout();
  test1Complete = true;
});

add_task(function asyncTest_no2() {
  yield executeWithTimeout();
  test2Complete = true;
});

add_task(function() {
  ok(test1Complete, "We have been through test 1");
  ok(test2Complete, "We have been through test 2");
});
