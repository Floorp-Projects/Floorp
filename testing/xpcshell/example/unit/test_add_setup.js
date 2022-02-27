/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let someVar = 1;

add_task(() => {
  Assert.ok(false, "I should not be called!");
});

/* eslint-disable mozilla/reject-addtask-only */
add_task(() => {
  Assert.equal(
    someVar,
    2,
    "Setup should have run, even though this is the only test."
  );
}).only();

add_setup(() => {
  someVar = 2;
});
