"use strict";

add_task(async function sameCaseAsMatch() {
  await runTest("moz");
});

add_task(async function differentCaseFromMatch() {
  await runTest("MOZ");
});

async function runTest(searchStr) {
  let matches = ["mozilla.org", "example.com"];
  let result = new AutoCompleteResultBase(matches);
  result.defaultIndex = 0;

  let search = new AutoCompleteSearchBase("search", result);
  registerAutoCompleteSearch(search);

  let input = new AutoCompleteInputBase([search.name]);
  input.completeSelectedIndex = true;
  input.completeDefaultIndex = true;

  // Start off with the search string in the input.  The selection must be
  // collapsed and the caret must be at the end to trigger autofill below.
  input.textValue = searchStr;
  input.selectTextRange(searchStr.length, searchStr.length);
  Assert.equal(
    input.selectionStart,
    searchStr.length,
    "Selection should start at the end of the input"
  );
  Assert.equal(
    input.selectionEnd,
    searchStr.length,
    "Selection should end at the end of the input"
  );

  let controller = Cc["@mozilla.org/autocomplete/controller;1"].createInstance(
    Ci.nsIAutoCompleteController
  );
  controller.input = input;
  input.controller = controller;

  // Start a search.
  await new Promise(resolve => {
    controller.startSearch(searchStr);
    input.onSearchComplete = () => {
      // The first match should have autofilled, but the case of the search
      // string should be preserved.
      let expectedValue = searchStr + matches[0].substr(searchStr.length);
      Assert.equal(input.textValue, expectedValue, "Should have autofilled");
      Assert.equal(
        input.selectionStart,
        searchStr.length,
        "Selection should start after search string"
      );
      Assert.equal(
        input.selectionEnd,
        expectedValue.length,
        "Selection should end at the end of the input"
      );
      resolve();
    };
  });

  // Key down to select the second match in the popup.
  // Hardcode KeyboardEvent.DOM_VK_DOWN, because we can't easily
  // include KeyboardEvent here.
  controller.handleKeyNavigation(0x28 /* KeyboardEvent.DOM_VK_DOWN */);
  let expectedValue = matches[1];
  Assert.equal(
    input.textValue,
    expectedValue,
    "Should have filled second match"
  );
  Assert.equal(
    input.selectionStart,
    expectedValue.length,
    "Selection should start at the end of the input"
  );
  Assert.equal(
    input.selectionEnd,
    expectedValue.length,
    "Selection should end at the end of the input"
  );

  // Key up to select the first match again.  The input should be restored
  // exactly as it was when the first match was autofilled above: the search
  // string's case should be preserved, and the selection should be preserved.
  // Hardcode KeyboardEvent.DOM_VK_UP, because we can't easily
  // include KeyboardEvent here.
  controller.handleKeyNavigation(0x26 /* KeyboardEvent.DOM_VK_UP */);
  expectedValue = searchStr + matches[0].substr(searchStr.length);
  Assert.equal(
    input.textValue,
    expectedValue,
    "Should have filled first match again"
  );
  Assert.equal(
    input.selectionStart,
    searchStr.length,
    "Selection should start after search string again"
  );
  Assert.equal(
    input.selectionEnd,
    expectedValue.length,
    "Selection should end at the end of the input again"
  );
}
