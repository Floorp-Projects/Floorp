var testURL = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

function pageLoaded(url) {
  return function() {
    let tab = gCurrentTest._tab;
    return !tab.isLoading() && tab.browser.currentURI.spec == url;
  }
}
  
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
  _tab: null,

  run: function() {
    Browser.addTab("about:blank", true);
    this._tab = Browser.addTab(testURL, true);

    // Wait for the tab to load, then do the test
    waitFor(gCurrentTest.onPageReady, pageLoaded(testURL));
  },

  onPageReady: function() {
    // Add some data
    ss.setTabValue(gCurrentTest._tab.chromeTab, "test1", "hello");
    is(ss.getTabValue(gCurrentTest._tab.chromeTab, "test1"), "hello", "Set/Get tab value matches");

    // Close tab and then undo the close
    gCurrentTest.numTabs = Browser.tabs.length;
    gCurrentTest.numClosed = ss.getClosedTabCount(window);

    Browser.closeTab(gCurrentTest._tab);

    is(Browser.tabs.length, gCurrentTest.numTabs - 1, "Tab was closed");
    is(ss.getClosedTabCount(window), gCurrentTest.numClosed + 1, "Tab was stored");

    // SessionStore works with chrome tab elements, not JS tab objects.
    // Map the _tab from chrome to JS
    gCurrentTest._tab = Browser.getTabFromChrome(ss.undoCloseTab(window, 0));

    // Wait for the tab to load, then do the test
    waitFor(gCurrentTest.onPageUndo, pageLoaded(testURL));
  },

  onPageUndo: function() {
    is(Browser.tabs.length, gCurrentTest.numTabs, "Tab was reopened");
    is(ss.getClosedTabCount(window), gCurrentTest.numClosed, "Tab was removed from store");

    is(ss.getTabValue(gCurrentTest._tab.chromeTab, "test1"), "hello", "Set/Get tab value matches after un-close");

    ss.deleteTabValue(gCurrentTest._tab.chromeTab, "test1");
    is(ss.getTabValue(gCurrentTest._tab.chromeTab, "test1"), "", "Set/Get tab value matches after removing value");

    // Shutdown
    Browser.closeTab(gCurrentTest._tab);
    runNextTest();
  }  
});
