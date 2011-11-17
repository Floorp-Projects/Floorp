/*
 * Make sure history is being recorded when we visit websites.
 */

var testURL_01 = baseURI + "browser_blank_01.html";

// A queue to order the tests and a handle for each test
var gTests = [];
var gCurrentTest = null;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

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
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

/**
 * One-time observer callback.
 */
function waitForObserve(name, callback) {
  var observerService = Cc["@mozilla.org/observer-service;1"]
                        .getService(Ci.nsIObserverService);
  var observer = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
    observe: function(subject, topic, data) {
      observerService.removeObserver(observer, name);
      observer = null;
      callback(subject, topic, data);
    }
  };

  observerService.addObserver(observer, name, false);
}

//------------------------------------------------------------------------------

gTests.push({
  desc: "Test history being added with page visit",
  _currentTab: null,

  run: function() {
    this._currentTab = Browser.addTab(testURL_01, true);
    waitForObserve("uri-visit-saved", function(subject, topic, data) {
      let uri = subject.QueryInterface(Ci.nsIURI);
      ok(uri.spec == testURL_01, "URI was saved to history");
      Browser.closeTab(gCurrentTest._currentTab);
      runNextTest();
    });
  }
});
