/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function AutoCompleteResult(aValues) {
  this._values = aValues;
  this.defaultIndex = -1;
  this._typeAheadResult = false;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

function AutoCompleteTypeAheadResult(aValues) {
  this._values = aValues;
  this.defaultIndex = 0;
  this._typeAheadResult = true;
}
AutoCompleteTypeAheadResult.prototype = Object.create(AutoCompleteResultBase.prototype);


/**
 * Test AutoComplete with multiple AutoCompleteSearch sources, with one of them
 * being hidden from the popup, but can still do typeahead completion.
 */
function run_test() {
  do_test_pending();

  var inputStr = "moz";

  // Type ahead result
  var searchTypeAhead = new AutoCompleteSearchBase("search1",
                                                   new AutoCompleteTypeAheadResult(["mozillaTest1"]));
  // Regular result
  var searchNormal = new AutoCompleteSearchBase("search2",
                                                new AutoCompleteResult(["mozillaTest2"]));

  // Register searches so AutoCompleteController can find them
  registerAutoCompleteSearch(searchNormal);
  registerAutoCompleteSearch(searchTypeAhead);

  // Make an AutoCompleteInput that uses our searches
  // and confirms results on search complete.
  var input = new AutoCompleteInputBase([searchTypeAhead.name, searchNormal.name]);
  input.completeDefaultIndex = true;
  input.textValue = inputStr;

  // Caret must be at the end. Autofill doesn't happen unless you're typing
  // characters at the end.
  var strLen = inputStr.length;
  input.selectTextRange(strLen, strLen);
  do_check_eq(input.selectionStart, strLen);
  do_check_eq(input.selectionEnd, strLen);

  var controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  controller.input = input;
  controller.startSearch(inputStr);

  input.onSearchComplete = function() {
    // Hidden results should still be able to do inline autocomplete
    do_check_eq(input.textValue, "mozillaTest1");

    // Now, let's fill the textbox with the first result of the popup.
    // The first search is marked as hidden, so we must always get the
    // second search.
    controller.handleEnter(true);
    do_check_eq(input.textValue, "mozillaTest2");

    // Only one item in the popup.
    do_check_eq(controller.matchCount, 1);

    // Unregister searches
    unregisterAutoCompleteSearch(searchNormal);
    unregisterAutoCompleteSearch(searchTypeAhead);
    do_test_finished();
  };
}
