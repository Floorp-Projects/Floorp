/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the requestAutocomplete user interface.
 */

"use strict";

/**
 * Open the requestAutocomplete UI and test that selecting a profile results in
 * the correct data being sent back to the opener.
 */
add_task(function* test_select_profile() {
  // Request an e-mail address.
  let data = { "": { "": { "email": null } } };
  let { uiWindow, promiseResult } = yield FormAutofillTest.showUI(data);

  // Accept the dialog.
  let acceptButton = uiWindow.document.getElementById("accept");
  EventUtils.synthesizeMouseAtCenter(acceptButton, {}, uiWindow);

  let result = yield promiseResult;
  Assert.equal(result.email, "email@example.org");
});

/**
 * Open the requestAutocomplete UI and cancel the dialog.
 */
add_task(function* test_cancel() {
  // Request an e-mail address.
  let data = { "": { "": { "email": null } } };
  let { uiWindow, promiseResult } = yield FormAutofillTest.showUI(data);

  // Cancel the dialog.
  let acceptButton = uiWindow.document.getElementById("cancel");
  EventUtils.synthesizeMouseAtCenter(acceptButton, {}, uiWindow);

  let result = yield promiseResult;
  Assert.ok(result.canceled);
});

add_task(terminationTaskFn);
