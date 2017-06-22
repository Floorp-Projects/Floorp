/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var test1Complete = false;
var test2Complete = false;

function executeWithTimeout() {
  return new Promise(resolve =>
    executeSoon(function() {
      ok(true, "we get here after a timeout");
      resolve();
    })
  );
}

add_task(async function asyncTest_no1() {
  await executeWithTimeout();
  test1Complete = true;
});

add_task(async function asyncTest_no2() {
  await executeWithTimeout();
  test2Complete = true;
});

add_task(function() {
  ok(test1Complete, "We have been through test 1");
  ok(test2Complete, "We have been through test 2");
});
