let testURL = chromeRoot + "browser_contentpopup.html";
messageManager.loadFrameScript(chromeRoot + "remote_contentpopup.js", true);

let newTab = null;

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

function test() {
  // This test is async
  waitForExplicitFinish();

  // Need to wait until the page is loaded
  messageManager.addMessageListener("pageshow", function(aMessage) {
    if (newTab && newTab.browser.currentURI.spec != "about:blank") {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);
      BrowserUI.closeAutoComplete(true);
      setTimeout(runNextTest, 0);
    }
  });

  waitForFirstPaint(function() {
    newTab = Browser.addTab(testURL, true);
  });
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
      // We must finalize the tests
      finish();
    }
  }
}

//------------------------------------------------------------------------------
// Case: Show/Hide the content popup helper
gTests.push({
  desc: "Show/Hide the content popup helper",

  run: function() {
    let popup = document.getElementById("form-helper-suggestions-container");
    popup.addEventListener("contentpopupshown", function(aEvent) {
      aEvent.target.removeEventListener(aEvent.type, arguments.callee, false);
      ok(!popup.hidden, "Content popup should be visible");
      waitFor(gCurrentTest.hidePopup, function() {
        return FormHelperUI._open;
      });
    }, false);

    AsyncTests.waitFor("TestRemoteAutocomplete:Click",
                        { id: "input-datalist-1" }, function(json) {});
  },

  hidePopup: function() {
    let popup = document.getElementById("form-helper-suggestions-container");
    popup.addEventListener("contentpopuphidden", function(aEvent) {
      popup.removeEventListener("contentpopuphidden", arguments.callee, false);
      ok(popup.hidden, "Content popup should be hidden");
      waitFor(gCurrentTest.finish, function() {
        return !FormHelperUI._open;
      });
    }, false);

    // Close the form assistant
    FormHelperUI.hide();
  },

  finish: function() {
    runNextTest();
  }
});

