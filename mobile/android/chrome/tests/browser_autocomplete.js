let testURL = chromeRoot + "browser_autocomplete.html";
messageManager.loadFrameScript(chromeRoot + "remote_autocomplete.js", true);

let newTab = null;

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

function test() {
  // This test is async
  waitForExplicitFinish();

  // Ensure the form helper is initialized
  try {
    FormHelperUI.enabled;
  }
  catch(e) {
    FormHelperUI.init();
  }

  // Need to wait until the page is loaded
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (newTab && newTab.browser.currentURI.spec != "about:blank") {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);
      BrowserUI.closeAutoComplete(true);
      setTimeout(runNextTest, 0);
    }
  });

  newTab = Browser.addTab(testURL, true);
}


//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Cleanup. All tests are completed at this point
    try {
      // Add any cleanup code here

      // Close our tab when finished
      Browser.closeTab(newTab);
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

function waitForAutocomplete(aCallback) {
  window.addEventListener("contentpopupshown", function(aEvent) {
    window.removeEventListener(aEvent.type, arguments.callee, false);
    setTimeout(function() {
      aCallback(FormHelperUI._currentElement.list);
    }, 0);
  }, false);
};

let data = [
  { label: "foo", value: "foo" },
  { label: "Somewhat bar", value: "bar" },
  { label: "foobar", value: "_" }
];

//------------------------------------------------------------------------------
// Case: Click on a datalist element and show suggestions
gTests.push({
  desc: "Click on a datalist element and show suggestions",

  run: function() {
    waitForAutocomplete(gCurrentTest.checkData);
    AsyncTests.waitFor("TestRemoteAutocomplete:Click",
                        { id: "input-datalist-1" }, function(json) {});
  },

  // Check that the data returned by the autocomplete handler on the content
  // side is correct
  checkData: function(aOptions) {
    for (let i = 0; i < aOptions.length; i++) {
      let option = aOptions[i];
      let valid = data[i];

      is(option.label, valid.label, "Label should be equal (" + option.label + ", " + valid.label +")");
      is(option.value, valid.value, "Value should be equal (" + option.value + ", " + valid.value +")");
    }

    // Wait until suggestions box has been popupated
    waitFor(gCurrentTest.checkUI, function() {
      let suggestionsBox = document.getElementById("form-helper-suggestions");
      return suggestionsBox.childNodes.length;
    });
  },

  // Check that the UI reflect the specificity of the data
  checkUI: function() {
    let suggestionsBox = document.getElementById("form-helper-suggestions");
    let suggestions = suggestionsBox.childNodes;

    for (let i = 0; i < suggestions.length; i++) {
      let suggestion = suggestions[i];
      let valid = data[i];
      let label = suggestion.getAttribute("value");
      let value = suggestion.getAttribute("data");

      is(label, valid.label, "Label should be equal (" + label + ", " + valid.label +")");
      is(value, valid.value, "Value should be equal (" + value + ", " + valid.value +")");
    }

    gCurrentTest.checkUIClick(0);
  },

  // Ensure that clicking on a given datalist element set the right value in
  // the input box
  checkUIClick: function(aIndex) {
    let suggestionsBox = document.getElementById("form-helper-suggestions");

    let suggestion = suggestionsBox.childNodes[aIndex];
    if (!suggestion) {
      gCurrentTest.finish();
      return;
    }

    // Use the form helper autocompletion helper
    FormHelperUI.doAutoComplete(suggestion);

    AsyncTests.waitFor("TestRemoteAutocomplete:Check", { id: "input-datalist-1" }, function(json) {
      is(json.result, suggestion.getAttribute("data"), "The target input value should be set to " + data);
      gCurrentTest.checkUIClick(aIndex + 1);
    });
  },

  finish: function() {
    // Close the form assistant
    FormHelperUI.hide();


    AsyncTests.waitFor("TestRemoteAutocomplete:Reset", { id: "input-datalist-1" }, function(json) {
      runNextTest();
    });
  }
});

//------------------------------------------------------------------------------
// Case: Check arrows visibility
gTests.push({
  desc: "Check arrows visibility",

  run: function() {
    let popup = document.getElementById("form-helper-suggestions-container");
    popup.addEventListener("contentpopupshown", function(aEvent) {
      aEvent.target.removeEventListener(aEvent.type, arguments.callee, false);
      waitFor(gCurrentTest.checkNoArrows, function() {
        return FormHelperUI._open;
      });
    }, false);

    AsyncTests.waitFor("TestRemoteAutocomplete:Click",
                        { id: "input-datalist-3" }, function(json) {});
  },

  checkNoArrows: function() {
    let scrollbox = document.getElementById("form-helper-suggestions");
    todo_is(scrollbox._scrollButtonUp.collapsed, true, "Left button should be collapsed");
    todo_is(scrollbox._scrollButtonDown.collapsed, true, "Right button should be collapsed");
    gCurrentTest.finish();
  },

  finish: function() {
    // Close the form assistant
    FormHelperUI.hide();

    runNextTest();
  }
});

