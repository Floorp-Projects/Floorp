/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let someVar = 0;

add_task(async function test_first() {
  Assert.equal(someVar, 1, "I should run as the first test task.");
  someVar++;
});

add_setup(function setup() {
  Assert.equal(someVar, 0, "Should run setup first.");
  someVar++;
});

add_task(async function test_second() {
  Assert.equal(someVar, 2, "I should run as the second test task.");
});
