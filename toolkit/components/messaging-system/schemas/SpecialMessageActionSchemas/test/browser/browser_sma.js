/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_unknown_event() {
  let error;
  try {
    await SpecialMessageActions.handleAction(
      { type: "UNKNOWN_EVENT_123" },
      gBrowser
    );
  } catch (e) {
    error = e;
  }
  ok(error, "should throw if an unexpected event is handled");
  Assert.equal(
    error.message,
    "Special message action with type UNKNOWN_EVENT_123 is unsupported."
  );
});
