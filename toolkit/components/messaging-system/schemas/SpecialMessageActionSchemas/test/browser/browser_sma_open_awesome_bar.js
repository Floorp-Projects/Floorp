/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_OPEN_AWESOME_BAR() {
  await SMATestUtils.executeAndValidateAction({ type: "OPEN_AWESOME_BAR" });
  Assert.ok(gURLBar.focused, "Focus should be on awesome bar");
});
