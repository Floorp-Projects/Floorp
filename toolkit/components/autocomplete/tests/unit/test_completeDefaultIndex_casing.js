/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function AutoCompleteResult(aValues) {
  this._values = aValues;
  this.defaultIndex = 0;
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
  this.popup.selectedIndex = -1;
  this.completeDefaultIndex = true;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

add_test(function test_keyNavigation() {
  doSearch("MOZ", "mozilla", function(aController) {
    Assert.equal(aController.input.textValue, "MOZilla");
    aController.handleKeyNavigation(Ci.nsIDOMKeyEvent.DOM_VK_RIGHT);
    Assert.equal(aController.input.textValue, "mozilla");
  });
});

add_test(function test_handleEnter() {
  doSearch("MOZ", "mozilla", function(aController) {
    Assert.equal(aController.input.textValue, "MOZilla");
    aController.handleEnter(false);
    Assert.equal(aController.input.textValue, "mozilla");
  });
});

function doSearch(aSearchString, aResultValue, aOnCompleteCallback) {
  let search = new AutoCompleteSearchBase("search",
                                          new AutoCompleteResult([ "mozilla", "toolkit" ], 0));
  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);

  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput([ search.name ]);
  input.textValue = aSearchString;

  // Caret must be at the end for autofill to happen.
  let strLen = aSearchString.length;
  input.selectTextRange(strLen, strLen);
  controller.input = input;
  controller.startSearch(aSearchString);

  input.onSearchComplete = function onSearchComplete() {
    aOnCompleteCallback(controller);

    // Clean up.
    unregisterAutoCompleteSearch(search);
    run_next_test();
  };
}
