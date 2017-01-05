/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the response sent when requestAutocomplete is canceled by the user.
 */

"use strict";

/* import-globals-from loader.js */

/**
 * The requestAutocomplete UI will not be displayed during these tests.
 */
add_task_in_parent_process(function* test_cancel_init() {
  FormAutofillTest.requestAutocompleteResponse = { canceled: true };
});

/**
 * Tests the case where the feature is canceled.
 */
add_task(function* test_cancel() {
  let promise = TestUtils.waitForEvent($("form"), "autocompleteerror");
  $("form").requestAutocomplete();
  let errorEvent = yield promise;

  Assert.equal(errorEvent.reason, "cancel");
});
