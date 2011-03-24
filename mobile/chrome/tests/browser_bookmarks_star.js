/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */

var testURL_01 = chromeRoot + "browser_blank_01.html";
var testURL_02 = chromeRoot + "browser_blank_02.html";

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
      PlacesUtils.bookmarks.removeFolderChildren(BookmarkList.panel.mobileRoot);
    }
    finally {
      // We must finialize the tests
      finish();
    }
  }
}

//------------------------------------------------------------------------------
// Case: Test appearance and behavior of the bookmark popup
gTests.push({
  desc: "Test appearance and behavior of the bookmark popup",
  _currentTab: null,

  run: function() {
    this._currentTab = Browser.addTab(testURL_02, true);

    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageLoad();
      }
    });
  },

  onPageLoad: function() {
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady1, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady1: function() {
    // Popup should no longer auto-hide, see bug 63204
    setTimeout(gCurrentTest.onPopupGone, 3000);
  },

  onPopupGone: function() {
    is(document.getElementById("bookmark-popup").hidden, false, "Bookmark popup should not be auto-hidden");
    waitFor(gCurrentTest.onPopupReady2, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady2: function() {
    // Let's make it disappear again by clicking the star again
    var starbutton = document.getElementById("tool-star");
    starbutton.click();
    
    waitFor(gCurrentTest.onPopupGone2, function() { return document.getElementById("bookmark-popup").hidden == true; });
  },

  onPopupGone2: function() {
    // Make sure it's hidden again
    is(document.getElementById("bookmark-popup").hidden, true, "Bookmark popup should be hidden by clicking star");

    // Let's make it appear again and continue the test
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady3, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady3: function() {
    // Let's make it disappear again by clicking somewhere
    let contentarea = document.getElementById("browsers");
    EventUtils.synthesizeMouse(contentarea, contentarea.clientWidth / 2, contentarea.clientHeight / 2, {});

    waitFor(gCurrentTest.onPopupGone3, function() { return document.getElementById("bookmark-popup").hidden == true; });
  },

  onPopupGone3: function() {
    // Make sure it's hidden again
    is(document.getElementById("bookmark-popup").hidden, true, "Bookmark popup should be hidden by clicking in content");
    
    BrowserUI.closeTab(this._currentTab);
    
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test adding tags via star icon
gTests.push({
  desc: "Test adding tags via star icon",
  _currentTab: null,

  run: function() {
    this._currentTab = Browser.addTab(testURL_02, true);

    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageLoad();
      }
    });
  },

  onPageLoad: function() {
    var starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady: function() {
    var editbutton = document.getElementById("bookmark-popup-edit");
    editbutton.click();

    waitFor(gCurrentTest.onEditorReady, function() { return document.getElementById("bookmark-item").isEditing == true; });
  },

  onEditorReady: function() {
    var bookmarkitem = document.getElementById("bookmark-item");
    bookmarkitem.tags = "tagone, tag two, tag-three, tag4";

    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    waitFor(gCurrentTest.onEditorDone, function() { return document.getElementById("bookmark-container").hidden == true; });
  },

  onEditorDone: function() {
    var tagsarray = PlacesUtils.tagging.getTagsForURI(makeURI(testURL_02), {});
    is(tagsarray.length, 4, "All tags are added.");
    
    BrowserUI.closeTab(this._currentTab);
    
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test editing uri via star icon
gTests.push({
  desc: "Test editing uri via star icon",
  _currentTab: null,

  run: function() {
    this._currentTab = Browser.addTab(testURL_02, true);

    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageLoad();
      }
    });
  },

  onPageLoad: function() {
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady: function() {
    let editbutton = document.getElementById("bookmark-popup-edit");
    editbutton.click();
    
    waitFor(gCurrentTest.onEditorReady, function() { return document.getElementById("bookmark-item").isEditing == true; });
  },

  onEditorReady: function() {
    let bookmarkitem = document.getElementById("bookmark-item");    
    bookmarkitem.spec = testURL_01;

    let donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    waitFor(gCurrentTest.onEditorDone, function() { return document.getElementById("bookmark-container").hidden == true; });
  },

  onEditorDone: function() {
    isnot(PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01)), -1, testURL_01 + " is now bookmarked");
    is(PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02)), -1, testURL_02 + " is no longer bookmarked");
    
    BrowserUI.closeTab(this._currentTab);
    
    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Case: Test removing existing bookmark via popup
gTests.push({
  desc: "Test removing existing bookmark via popup",
  _currentTab: null,
  run: function() {
    this._currentTab = Browser.addTab(testURL_01, true);

    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      if (gCurrentTest._currentTab.browser.currentURI.spec != "about:blank") {
        messageManager.removeMessageListener(aMessage.name, arguments.callee);
        gCurrentTest.onPageLoad();
      }
    });
  },

  onPageLoad: function() {
    var starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady: function() {
    var removebutton = document.getElementById("bookmark-popup-remove");
    removebutton.click();
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01));
    ok(bookmark == -1, testURL_01 + " should no longer in bookmark");

    BrowserUI.closeTab(this._currentTab);

    runNextTest();
  }
});
