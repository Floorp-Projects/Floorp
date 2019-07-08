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

add_test(function test_handleEnter_mouse() {
  doSearch("moz", "mozilla.com", "http://www.mozilla.com", function(
    aController
  ) {
    Assert.equal(aController.input.textValue, "moz");
    Assert.equal(
      aController.getFinalCompleteValueAt(0),
      "http://www.mozilla.com"
    );
    // Keyboard interaction is tested by test_finalCompleteValueSelectedIndex.js
    // so here just test popup selection.
    aController.handleEnter(true);
    Assert.equal(aController.input.textValue, "http://www.mozilla.com");
  });
});

function doSearch(
  aSearchString,
  aResultValue,
  aFinalCompleteValue,
  aOnCompleteCallback
) {
  let search = new AutoCompleteSearchBase(
    "search",
    new AutoCompleteResult([aResultValue], [aFinalCompleteValue])
  );
  registerAutoCompleteSearch(search);

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].getService(
    Ci.nsIAutoCompleteController
  );

  // Make an AutoCompleteInput that uses our searches and confirms results.
  let input = new AutoCompleteInput([search.name]);
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
