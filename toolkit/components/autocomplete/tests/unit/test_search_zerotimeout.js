/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function AutoCompleteDelayedSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteDelayedSearch.prototype = Object.create(
  AutoCompleteSearchBase.prototype
);

function AutoCompleteResult(aValues, aDefaultIndex) {
  this._values = aValues;
  this.defaultIndex = aDefaultIndex;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

/**
 * A delayed search should be executed synchronously with a zero timeout.
 */
add_test(function test_delayed_search_notimeout() {
  let inputStr = "moz";

  let delayedSearch = new AutoCompleteDelayedSearch(
    "delayed",
    new AutoCompleteResult(["moz-delayed"], 0)
  );
  registerAutoCompleteSearch(delayedSearch);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );

  let input = new AutoCompleteInputBase([delayedSearch.name]);
  input.completeDefaultIndex = true;
  input.textValue = inputStr;
  input.timeout = 0;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  let strLen = inputStr.length;
  input.selectTextRange(strLen, strLen);

  controller.input = input;
  let complete = false;
  input.onSearchComplete = function () {
    complete = true;
  };
  controller.startSearch(inputStr);
  Assert.ok(complete);

  // Immediately check the result, the delayed search should have finished.
  Assert.equal(input.textValue, "moz-delayed");

  unregisterAutoCompleteSearch(delayedSearch);
  run_next_test();
});
