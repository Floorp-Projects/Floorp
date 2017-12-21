/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function AutoCompleteResult(aValues, aFinalCompleteValues) {
  this._values = aValues;
  this._finalCompleteValues = aFinalCompleteValues;
  this.defaultIndex = 0;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
  this.popup.selectedIndex = -1;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

add_test(function test_handleEnterWithDirectMatchCompleteSelectedIndex() {
  doSearch("moz", "mozilla.com", "http://www.mozilla.com",
    { forceComplete: true, completeSelectedIndex: true }, function(aController) {
    Assert.equal(aController.input.textValue, "moz");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    aController.handleEnter(false);
    // After enter the final complete value should be shown in the input.
    Assert.equal(aController.input.textValue, "http://www.mozilla.com");
  });
});

add_test(function test_handleEnterWithDirectMatch() {
  doSearch("mozilla", "mozilla.com", "http://www.mozilla.com",
    { forceComplete: true, completeDefaultIndex: true }, function(aController) {
    // Should autocomplete the search string to a suggestion.
    Assert.equal(aController.input.textValue, "mozilla.com");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    aController.handleEnter(false);
    // After enter the final complete value should be shown in the input.
    Assert.equal(aController.input.textValue, "http://www.mozilla.com");
  });
});

add_test(function test_handleEnterWithNoMatch() {
  doSearch("mozilla", "mozilla.com", "http://www.mozilla.com",
    { forceComplete: true, completeDefaultIndex: true }, function(aController) {
    // Should autocomplete the search string to a suggestion.
    Assert.equal(aController.input.textValue, "mozilla.com");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    // Now input something that does not match...
    aController.input.textValue = "mozillax";
    // ... and confirm. We don't want one of the values from the previous
    // results to be taken, since what's now in the input field doesn't match.
    aController.handleEnter(false);
    Assert.equal(aController.input.textValue, "mozillax");
  });
});

add_test(function test_handleEnterWithIndirectMatch() {
  doSearch("com", "mozilla.com", "http://www.mozilla.com",
    { forceComplete: true, completeDefaultIndex: true }, function(aController) {
    // Should autocomplete the search string to a suggestion.
    Assert.equal(aController.input.textValue, "com >> mozilla.com");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    aController.handleEnter(false);
    // After enter the final complete value from the suggestion should be shown
    // in the input.
    Assert.equal(aController.input.textValue, "http://www.mozilla.com");
  });
});

function doSearch(aSearchString, aResultValue, aFinalCompleteValue,
                  aInputProps, aOnCompleteCallback) {
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult([ aResultValue ], [ aFinalCompleteValue ])
  );
  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput([ search.name ]);
  for (var p in aInputProps) {
    input[p] = aInputProps[p];
  }
  input.textValue = aSearchString;
  // Place the cursor at the end of the input so that completion to
  // default index will kick in.
  input.selectTextRange(aSearchString.length, aSearchString.length);

  controller.input = input;
  controller.startSearch(aSearchString);

  input.onSearchComplete = function onSearchComplete() {
    aOnCompleteCallback(controller);

    // Clean up.
    unregisterAutoCompleteSearch(search);
    run_next_test();
  };
}
