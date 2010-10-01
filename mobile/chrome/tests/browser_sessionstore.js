var testURL = chromeRoot + "browser_blank_01.html";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;
var ss = null;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

  // Start the tests
  runNextTest();
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
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

//------------------------------------------------------------------------------
// Case: Loading a page and test setting tab values
gTests.push({
  desc: "Loading a page and test setting tab values",
  _currentTab: null,

  run: function() {
    Browser.addTab("about:blank", true);
    this._currentTab = Browser.addTab(testURL, true);

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageReady();
      }
    });
  },

  onPageReady: function() {
    // Add some data
    ss.setTabValue(gCurrentTest._currentTab.chromeTab, "test1", "hello");
    is(ss.getTabValue(gCurrentTest._currentTab.chromeTab, "test1"), "hello", "Set/Get tab value matches");

    // Close tab and then undo the close
    gCurrentTest.numTabs = Browser.tabs.length;
    gCurrentTest.numClosed = ss.getClosedTabCount(window);

    Browser.closeTab(gCurrentTest._currentTab);

    isnot(Browser.tabs.length, gCurrentTest.numTabs, "Tab was closed");

    // XXX The behavior is different depending if the tests is launch alone or with the testsuite
    todo_isnot(ss.getClosedTabCount(window), gCurrentTest.numClosed, "Tab was stored");

    // SessionStore works with chrome tab elements, not JS tab objects.
    // Map the _currentTab from chrome to JS
    gCurrentTest._currentTab = Browser.getTabFromChrome(ss.undoCloseTab(window, 0));

    // Need to wait until the page is loaded
    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageUndo();
      }
    });
  },

  onPageUndo: function() {
    is(Browser.tabs.length, gCurrentTest.numTabs, "Tab was reopened");
    // XXX The behavior is different depending if the tests is launch alone or with the testsuite
    todo_is(ss.getClosedTabCount(window), gCurrentTest.numClosed, "Tab was removed from store");

    is(ss.getTabValue(gCurrentTest._currentTab.chromeTab, "test1"), "hello", "Set/Get tab value matches after un-close");

    ss.deleteTabValue(gCurrentTest._currentTab.chromeTab, "test1");
    is(ss.getTabValue(gCurrentTest._currentTab.chromeTab, "test1"), "", "Set/Get tab value matches after removing value");

    // Shutdown
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});
