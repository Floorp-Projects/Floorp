/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function AutoCompleteResult(aValues, aFinalCompleteValues) {
  this._values = aValues;
  this._finalCompleteValues = aFinalCompleteValues;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
  this.popup.selectedIndex = 0;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

function run_test() {
  run_next_test();
}

add_test(function test_handleEnter() {
  doSearch("moz", "mozilla.com", "http://www.mozilla.com", function(aController) {
    do_check_eq(aController.input.textValue, "moz");
    do_check_eq(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    aController.handleEnter(false);
    do_check_eq(aController.input.textValue, "http://www.mozilla.com");
  });
});

function doSearch(aSearchString, aResultValue, aFinalCompleteValue, aOnCompleteCallback) {
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult([ aResultValue ], [ aFinalCompleteValue ])
  );
  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);  
  
  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput([ search.name ]);
  input.textValue = aSearchString;

  controller.input = input;
  controller.startSearch(aSearchString);

  input.onSearchComplete = function onSearchComplete() {
    aOnCompleteCallback(controller);

    // Clean up.
    unregisterAutoCompleteSearch(search);
    run_next_test();
  };
}
