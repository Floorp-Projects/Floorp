function AutoCompleteResult(aResultValues) {
  this._values = aResultValues.map(x => x[0]);
  this._finalCompleteValues = aResultValues.map(x => x[1]);
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

var selectByWasCalled = false;
function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
  this.popup.selectedIndex = 0;
  this.popup.selectBy = function(reverse, page) {
    Assert.equal(selectByWasCalled, false);
    selectByWasCalled = true;
    Assert.equal(reverse, false);
    Assert.equal(page, false);
    this.selectedIndex += (reverse ? -1 : 1) * (page ? 100 : 1);
  };
  this.completeSelectedIndex = true;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

function run_test() {
  run_next_test();
}

add_test(function test_handleEnter() {
  let results = [
    ["mozilla.com", "http://www.mozilla.com"],
    ["mozilla.org", "http://www.mozilla.org"],
  ];
  // First check the case where we do select a value with the keyboard:
  doSearch("moz", results, function(aController) {
    Assert.equal(aController.input.textValue, "moz");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    Assert.equal(aController.getFinalCompleteValueAt(1), "http://www.mozilla.org");

    Assert.equal(aController.input.popup.selectedIndex, 0);
    aController.handleKeyNavigation(Ci.nsIDOMKeyEvent.DOM_VK_DOWN);
    Assert.equal(aController.input.popup.selectedIndex, 1);
    // Simulate mouse interaction changing selectedIndex
    // ie NOT keyboard interaction:
    aController.input.popup.selectedIndex = 0;

    aController.handleEnter(false);
    // Verify that the keyboard-selected thing got inserted,
    // and not the mouse selection:
    Assert.equal(aController.input.textValue, "http://www.mozilla.org");
  });

  // Then the case where we do not:
  doSearch("moz", results, function(aController) {
    Assert.equal(aController.input.textValue, "moz");
    Assert.equal(aController.getFinalCompleteValueAt(0), "http://www.mozilla.com");
    Assert.equal(aController.getFinalCompleteValueAt(1), "http://www.mozilla.org");

    Assert.equal(aController.input.popup.selectedIndex, 0);
    aController.input.popupOpen = true;
    // Simulate mouse interaction changing selectedIndex
    // ie NOT keyboard interaction:
    aController.input.popup.selectedIndex = 1;
    Assert.equal(selectByWasCalled, false);
    Assert.equal(aController.input.popup.selectedIndex, 1);

    aController.handleEnter(false);
    // Verify that the input stayed the same, because no selection was made
    // with the keyboard:
    Assert.equal(aController.input.textValue, "moz");
  });
});

function doSearch(aSearchString, aResults, aOnCompleteCallback) {
  selectByWasCalled = false;
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult(aResults)
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

