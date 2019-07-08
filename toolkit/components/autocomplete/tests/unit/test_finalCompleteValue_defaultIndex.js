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
  doSearch("moz", results, { selectedIndex: 0 }, controller => {
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

add_test(function test_handleEnter_otherSelected() {
  // The popup selection may not coincide with what is filled into the input
  // field, for example if the user changed it with the mouse and then pressed
  // Enter. In such a case we should still use the inputField value and not the
  // popup selected value.
  let results = [
    ["mozilla.com", "https://www.mozilla.com"],
    ["gomozilla.org", "http://www.gomozilla.org"],
  ];
  doSearch("moz", results, { selectedIndex: 1 }, controller => {
    let input = controller.input;
    Assert.equal(input.textValue, "mozilla.com");
    Assert.equal(controller.getFinalCompleteValueAt(0), results[0][1]);
    Assert.equal(controller.getFinalCompleteValueAt(1), results[1][1]);
    Assert.equal(input.popup.selectedIndex, 1);

    controller.handleEnter(false);
    // Verify that the keyboard-selected thing got inserted,
    // and not the mouse selection:
    Assert.equal(controller.input.textValue, "https://www.mozilla.com");
  });
});

add_test(function test_handleEnter_otherSelected_nocompleteselectedindex() {
  let results = [
    ["mozilla.com", "https://www.mozilla.com"],
    ["gomozilla.org", "http://www.gomozilla.org"],
  ];
  doSearch(
    "moz",
    results,
    { selectedIndex: 1, completeSelectedIndex: false },
    controller => {
      let input = controller.input;
      Assert.equal(input.textValue, "mozilla.com");
      Assert.equal(controller.getFinalCompleteValueAt(0), results[0][1]);
      Assert.equal(controller.getFinalCompleteValueAt(1), results[1][1]);
      Assert.equal(input.popup.selectedIndex, 1);

      controller.handleEnter(false);
      // Verify that the keyboard-selected result is inserted, not the
      // defaultComplete.
      Assert.equal(controller.input.textValue, "http://www.gomozilla.org");
    }
  );
});

function doSearch(aSearchString, aResults, aOptions, aOnCompleteCallback) {
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult(aResults)
  );
  registerAutoCompleteSearch(search);

  let input = new AutoCompleteInput([search.name]);
  input.textValue = aSearchString;
  if ("selectedIndex" in aOptions) {
    input.popup.selectedIndex = aOptions.selectedIndex;
  }
  if ("completeSelectedIndex" in aOptions) {
    input.completeSelectedIndex = aOptions.completeSelectedIndex;
  }
  // Needed for defaultIndex completion.
  input.selectTextRange(aSearchString.length, aSearchString.length);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );
  controller.input = input;
  controller.startSearch(aSearchString);

  input.onSearchComplete = function onSearchComplete() {
    aOnCompleteCallback(controller);

    unregisterAutoCompleteSearch(search);
    run_next_test();
  };
}
