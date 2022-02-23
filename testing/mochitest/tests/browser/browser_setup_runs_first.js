/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let someVar = 1;

add_task(() => {
  is(someVar, 2, "Should get updated by setup which runs first.");
});

add_setup(() => {
  someVar = 2;
});
