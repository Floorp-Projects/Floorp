/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_cancel_event() {
  let error = null;
  try {
    await SMATestUtils.executeAndValidateAction({ type: "CANCEL" });
  } catch (e) {
    error = e;
  }
  ok(!error, "should not throw for CANCEL");
});
