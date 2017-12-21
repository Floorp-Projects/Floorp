/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


function AutoCompleteImmediateSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteImmediateSearch.prototype = Object.create(AutoCompleteSearchBase.prototype);
AutoCompleteImmediateSearch.prototype.searchType =
  Ci.nsIAutoCompleteSearchDescriptor.SEARCH_TYPE_IMMEDIATE;
AutoCompleteImmediateSearch.prototype.QueryInterface =
  XPCOMUtils.generateQI([Ci.nsIFactory,
                         Ci.nsIAutoCompleteSearch,
                         Ci.nsIAutoCompleteSearchDescriptor]);

function AutoCompleteDelayedSearch(aName, aResult) {
  this.name = aName;
  this._result = aResult;
}
AutoCompleteDelayedSearch.prototype = Object.create(AutoCompleteSearchBase.prototype);

function AutoCompleteResult(aValues, aDefaultIndex) {
  this._values = aValues;
  this.defaultIndex = aDefaultIndex;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

/**
 * An immediate search should be executed synchronously.
 */
add_test(function test_immediate_search() {
  let inputStr = "moz";

  let immediateSearch = new AutoCompleteImmediateSearch(
    "immediate", new AutoCompleteResult(["moz-immediate"], 0));
  registerAutoCompleteSearch(immediateSearch);
  let delayedSearch = new AutoCompleteDelayedSearch(
    "delayed", new AutoCompleteResult(["moz-delayed"], 0));
  registerAutoCompleteSearch(delayedSearch);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  let input = new AutoCompleteInputBase([delayedSearch.name,
                                         immediateSearch.name]);
  input.completeDefaultIndex = true;
  input.textValue = inputStr;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  let strLen = inputStr.length;
  input.selectTextRange(strLen, strLen);

  controller.input = input;
  controller.startSearch(inputStr);

  // Immediately check the result, the immediate search should have finished.
  Assert.equal(input.textValue, "moz-immediate");

  // Wait for both queries to finish.
  input.onSearchComplete = function() {
    // Sanity check.
    Assert.equal(input.textValue, "moz-immediate");

    unregisterAutoCompleteSearch(immediateSearch);
    unregisterAutoCompleteSearch(delayedSearch);
    run_next_test();
  };
});

/**
 * An immediate search should be executed before any delayed search.
 */
add_test(function test_immediate_search_notimeout() {
  let inputStr = "moz";

  let immediateSearch = new AutoCompleteImmediateSearch(
    "immediate", new AutoCompleteResult(["moz-immediate"], 0));
  registerAutoCompleteSearch(immediateSearch);

  let delayedSearch = new AutoCompleteDelayedSearch(
    "delayed", new AutoCompleteResult(["moz-delayed"], 0));
  registerAutoCompleteSearch(delayedSearch);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  let input = new AutoCompleteInputBase([delayedSearch.name,
                                         immediateSearch.name]);
  input.completeDefaultIndex = true;
  input.textValue = inputStr;
  input.timeout = 0;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  let strLen = inputStr.length;
  input.selectTextRange(strLen, strLen);

  controller.input = input;
  let complete = false;
  input.onSearchComplete = function() {
    complete = true;
  };
  controller.startSearch(inputStr);
  Assert.ok(complete);

  // Immediately check the result, the immediate search should have finished.
  Assert.equal(input.textValue, "moz-immediate");

  unregisterAutoCompleteSearch(immediateSearch);
  unregisterAutoCompleteSearch(delayedSearch);
  run_next_test();
});

/**
 * A delayed search should be executed synchronously with a zero timeout.
 */
add_test(function test_delayed_search_notimeout() {
  let inputStr = "moz";

  let delayedSearch = new AutoCompleteDelayedSearch(
    "delayed", new AutoCompleteResult(["moz-delayed"], 0));
  registerAutoCompleteSearch(delayedSearch);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

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
  input.onSearchComplete = function() {
    complete = true;
  };
  controller.startSearch(inputStr);
  Assert.ok(complete);

  // Immediately check the result, the delayed search should have finished.
  Assert.equal(input.textValue, "moz-delayed");

  unregisterAutoCompleteSearch(delayedSearch);
  run_next_test();
});
