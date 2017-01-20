/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the requestAutocomplete user interface.
 */

"use strict";

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({
    set: [["dom.ipc.processCount", 1]]
  });
});

/**
 * Open the requestAutocomplete UI and test that selecting a profile results in
 * the correct data being sent back to the opener.
 */
add_task(function* test_select_profile() {
  // Request an e-mail address.
  let { uiWindow, promiseResult } = yield FormAutofillTest.showUI(
                                          TestData.requestEmailOnly);

  // Accept the dialog.
  let acceptButton = uiWindow.document.getElementById("accept");
  EventUtils.synthesizeMouseAtCenter(acceptButton, {}, uiWindow);

  let result = yield promiseResult;
  Assert.equal(result.fields.length, 1);
  Assert.equal(result.fields[0].section, "");
  Assert.equal(result.fields[0].addressType, "");
  Assert.equal(result.fields[0].contactType, "");
  Assert.equal(result.fields[0].fieldName, "email");
  Assert.equal(result.fields[0].value, "email@example.org");
});

/**
 * Open the requestAutocomplete UI and cancel the dialog.
 */
add_task(function* test_cancel() {
  // Request an e-mail address.
  let { uiWindow, promiseResult } = yield FormAutofillTest.showUI(
                                          TestData.requestEmailOnly);

  // Cancel the dialog.
  let acceptButton = uiWindow.document.getElementById("cancel");
  EventUtils.synthesizeMouseAtCenter(acceptButton, {}, uiWindow);

  let result = yield promiseResult;
  Assert.ok(result.canceled);
});

add_task(terminationTaskFn);
