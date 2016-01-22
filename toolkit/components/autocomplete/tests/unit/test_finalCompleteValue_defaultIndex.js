function AutoCompleteResult(aResultValues) {
  this.defaultIndex = 0;
  this._values = aResultValues.map(x => x[0]);
  this._finalCompleteValues = aResultValues.map(x => x[1]);
}
AutoCompleteResult.prototype = Object.create(AutoCompleteResultBase.prototype);

function AutoCompleteInput(aSearches) {
  this.searches = aSearches;
  this.popup.selectedIndex = 0;
  this.completeSelectedIndex = true;
  this.completeDefaultIndex = true;
}
AutoCompleteInput.prototype = Object.create(AutoCompleteInputBase.prototype);

add_test(function test_handleEnter() {
  let results = [
    ["mozilla.com", "https://www.mozilla.com"],
    ["gomozilla.org", "http://www.gomozilla.org"],
  ];
  doSearch("moz", results, controller => {
    let input = controller.input;
    Assert.equal(input.textValue, "mozilla.com");
    Assert.equal(controller.getFinalCompleteValueAt(0), results[0][1]);
    Assert.equal(controller.getFinalCompleteValueAt(1), results[1][1]);
    Assert.equal(input.popup.selectedIndex, 0);

    controller.handleEnter(false);
    // Verify that the keyboard-selected thing got inserted,
    // and not the mouse selection:
    Assert.equal(controller.input.textValue, "https://www.mozilla.com");
  });
});

function doSearch(aSearchString, aResults, aOnCompleteCallback) {
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult(aResults)
  );
  registerAutoCompleteSearch(search);

  let input = new AutoCompleteInput([ search.name ]);
  input.textValue = aSearchString;
  // Needed for defaultIndex completion.
  input.selectTextRange(aSearchString.length, aSearchString.length);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].
                   getService(Ci.nsIAutoCompleteController);
  controller.input = input;
  controller.startSearch(aSearchString);

  input.onSearchComplete = function onSearchComplete() {
    aOnCompleteCallback(controller);

    unregisterAutoCompleteSearch(search);
    run_next_test();
  };
}
