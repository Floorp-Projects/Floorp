/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */

var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService); 
var testURL_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
var testURL_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_02.html";

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
      PlacesUtils.bookmarks.removeFolderChildren(BookmarkList.mobileRoot);
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
  _currenttab: null,
  
  run: function() {
    this._currenttab = Browser.addTab(testURL_02, true);
    function handleEvent() {
      gCurrentTest._currenttab.browser.removeEventListener("load", handleEvent, true);
      gCurrentTest.onPageLoad();
    };
    this._currenttab.browser.addEventListener("load", handleEvent , true);
  },
  
  onPageLoad: function() {
    var starbutton = document.getElementById("tool-star");
    starbutton.click();
    
    waitFor(gCurrentTest.onPopupReady1, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },
  
  onPopupReady1: function() {
    // Popup should auto-hide after 2 seconds on the initial bookmark with star
    setTimeout(gCurrentTest.onPopupGone, 3000);
  },
  
  onPopupGone: function() {
    // Make sure it's hidden again
    is(document.getElementById("bookmark-popup").hidden, true, "Bookmark popup should be auto-hidden");
    
    // Let's make it appear again and continue the test
    var starbutton = document.getElementById("tool-star");
    starbutton.click();
    
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
    var starbutton = document.getElementById("tool-star");
    starbutton.click();
    
    waitFor(gCurrentTest.onPopupReady3, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },

  onPopupReady3: function() {
    // Let's make it disappear again by clicking somewhere
    var contentarea = document.getElementById("tile-container");
    EventUtils.synthesizeMouse(contentarea, contentarea.clientWidth / 2, contentarea.clientHeight / 2, {});
    
    waitFor(gCurrentTest.onPopupGone3, function() { return document.getElementById("bookmark-popup").hidden == true; });
  },
  
  onPopupGone3: function() {
    // Make sure it's hidden again
    is(document.getElementById("bookmark-popup").hidden, true, "Bookmark popup should be hidden by clicking in content");
    
    BrowserUI.closeTab(this._currenttab);
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test adding tags via star icon
gTests.push({
  desc: "Test adding tags via star icon",
  _currenttab: null,
  
  run: function() {
    this._currenttab = Browser.addTab(testURL_02, true);
    function handleEvent() {
      gCurrentTest._currenttab.browser.removeEventListener("load", handleEvent, true);
      gCurrentTest.onPageLoad();
    };
    this._currenttab.browser.addEventListener("load", handleEvent , true);
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
    var tagsarray = PlacesUtils.tagging.getTagsForURI(uri(testURL_02), {});
    is(tagsarray.length, 4, "All tags are added.");
    
    BrowserUI.closeTab(this._currenttab);
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test editing uri via star icon
gTests.push({
  desc: "Test editing uri via star icon",
  _currenttab: null,
  
  run: function() {
    this._currenttab = Browser.addTab(testURL_02, true);
    function handleEvent() {
      gCurrentTest._currenttab.browser.removeEventListener("load", handleEvent, true);
      gCurrentTest.onPageLoad();
    };
    this._currenttab.browser.addEventListener("load", handleEvent, true);
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
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});

    var uritextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "uri");
    uritextbox.value = testURL_01;

    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    waitFor(gCurrentTest.onEditorDone, function() { return document.getElementById("bookmark-container").hidden == true; });
  },

  onEditorDone: function() {
    isnot(PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01)), -1, testURL_01 + " is now bookmarked");
    is(PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_02)), -1, testURL_02 + " is no longer bookmarked");
    
    BrowserUI.closeTab(this._currenttab);
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test removing existing bookmark via popup
gTests.push({
  desc: "Test removing existing bookmark via popup",
  _currenttab: null,
  
  run: function() {
    this._currenttab = Browser.addTab(testURL_01, true);
    function handleEvent() {
      gCurrentTest._currenttab.browser.removeEventListener("load", handleEvent, true);
      gCurrentTest.onPageLoad();
    };
    this._currenttab.browser.addEventListener("load", handleEvent, true);
  },
  
  onPageLoad: function() {
    var starbutton = document.getElementById("tool-star");
    starbutton.click();    
    
    waitFor(gCurrentTest.onPopupReady, function() { return document.getElementById("bookmark-popup").hidden == false; });
  },
  
  onPopupReady: function() {
    var removebutton = document.getElementById("bookmark-popup-remove");
    removebutton.click();
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01));
    ok(bookmark == -1, testURL_01 + " should no longer in bookmark");

    BrowserUI.closeTab(this._currenttab);

    runNextTest();
  }
});

//------------------------------------------------------------------------------
// Helpers
function uri(spec) {
  return ioService.newURI(spec, null, null);
}

function waitFor(callback, test, timeout) {
  if (test()) {
    callback();
    return;
  }

  timeout = timeout || Date.now();
  if (Date.now() - timeout > 1000)
    throw "waitFor timeout";
  setTimeout(waitFor, 50, callback, test, timeout);
}

