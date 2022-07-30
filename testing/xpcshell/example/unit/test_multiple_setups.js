/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let someVar = 0;

add_setup(() => (someVar = 1));
add_setup(() => (someVar = 2));

add_task(async function test_setup_ordering() {
  Assert.equal(someVar, 2, "Setups should have run in order.");
});
