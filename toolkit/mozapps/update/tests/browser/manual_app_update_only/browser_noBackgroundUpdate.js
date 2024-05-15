/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_manual_app_update_policy() {
  // Unfortunately, we can't really test the other background update entry
  // point, gAUS.notify, because it doesn't return anything and it would be
  // a bit overkill to edit the nsITimerCallback interface just for this test.
  // But the two entry points just immediately call the same function, so this
  // should probably be alright.
  is(
    gAUS.checkForBackgroundUpdates(),
    false,
    "gAUS.checkForBackgroundUpdates() should not proceed with update check"
  );
});
