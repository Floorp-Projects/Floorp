/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let someVar = 1;

add_setup(() => {
  someVar = 2;
});

/* eslint-disable mozilla/reject-addtask-only */
add_task(() => {
  is(someVar, 2, "Setup should have run, even though this is the only test.");
}).only();
