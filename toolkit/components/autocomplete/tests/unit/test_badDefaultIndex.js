/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A results that wants to defaultComplete to 0, but it has no matches,
 * though it notifies SUCCESS to the controller.
 */
function AutoCompleteNoMatchResult() {
  this.defaultIndex = 0;
}
AutoCompleteNoMatchResult.prototype = Object.create(AutoCompleteResultBase.prototype);

/**
 * A results that wants to defaultComplete to an index greater than the number
 * of matches.
 */
function AutoCompleteBadIndexResult(aValues, aDefaultIndex) {
  do_check_true(aValues.length <= aDefaultIndex);
  this._values = aValues;
  this.defaultIndex = aDefaultIndex;
}
AutoCompleteBadIndexResult.prototype = Object.create(AutoCompleteResultBase.prototype);

add_test(function autocomplete_noMatch_success() {
  const INPUT_STR = "moz";

  let searchNoMatch =
    new AutoCompleteSearchBase("searchNoMatch",
                               new AutoCompleteNoMatchResult());
  registerAutoCompleteSearch(searchNoMatch);

  // Make an AutoCompleteInput that uses our search and confirms results.
  let input = new AutoCompleteInputBase([searchNoMatch.name]);
  input.completeDefaultIndex = true;
  input.textValue = INPUT_STR;

  // Caret must be at the end for autoFill to happen.
  let strLen = INPUT_STR.length;
  input.selectTextRange(strLen, strLen);
  do_check_eq(input.selectionStart, strLen);
  do_check_eq(input.selectionEnd, strLen);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = input;
  controller.startSearch(INPUT_STR);

  input.onSearchComplete = function () {
    // Should not try to autoFill to an empty value.
    do_check_eq(input.textValue, "moz");

    // Clean up.
    unregisterAutoCompleteSearch(searchNoMatch);
    run_next_test();
  };
});

add_test(function autocomplete_defaultIndex_exceeds_matchCount() {
  const INPUT_STR = "moz";

  // Result returning matches, but a bad defaultIndex.
  let searchBadIndex =
    new AutoCompleteSearchBase("searchBadIndex",
                               new AutoCompleteBadIndexResult(["mozillaTest"], 1));
  registerAutoCompleteSearch(searchBadIndex);

  // Make an AutoCompleteInput that uses our search and confirms results.
  let input = new AutoCompleteInputBase([searchBadIndex.name]);
  input.completeDefaultIndex = true;
  input.textValue = INPUT_STR;

  // Caret must be at the end for autoFill to happen.
  let strLen = INPUT_STR.length;
  input.selectTextRange(strLen, strLen);
  do_check_eq(input.selectionStart, strLen);
  do_check_eq(input.selectionEnd, strLen);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = input;
  controller.startSearch(INPUT_STR);

  input.onSearchComplete = function () {
    // Should not try to autoFill to an empty value.
    do_check_eq(input.textValue, "moz");

    // Clean up.
    unregisterAutoCompleteSearch(searchBadIndex);
    run_next_test();
  };
});

function run_test() {
  run_next_test();
}
