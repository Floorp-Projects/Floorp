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
// Case: Test adding tags to bookmark
gTests.push({
  desc: "Test adding tags to a bookmark",
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
    
    var bookmarkItem = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_02));
    ok(bookmarkItem != -1, testURL_02 + " should be added.");
    
    BookmarkList.show();  
    BookmarkList.toggleManage();

    waitFor(gCurrentTest.onEditorReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },
  
  onEditorReady: function() {    
    var bookmarkitems = document.getElementById("bookmark-items");
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
    
    var tagstextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "tags");
    tagstextbox.value = "tagone, tag two, tag-three, tag4";
    
    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    var tagsarray = PlacesUtils.tagging.getTagsForURI(uri(testURL_02), {});
    is(tagsarray.length, 4, "All tags are associated with specified bookmark");
    
    BookmarkList.close();
    Browser.closeTab(this._currenttab);
    
    runNextTest();
  }  
});

//------------------------------------------------------------------------------
// Case: Test editing tags to bookmark
gTests.push({
  desc: "Test editing tags to bookmark",  

  run: function() {
    BookmarkList.show();  
    BookmarkList.toggleManage();
    
    waitFor(gCurrentTest.onEditorReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },

  onEditorReady: function() {
    var taggeduri = PlacesUtils.tagging.getURIsForTag("tag-three");
    is(taggeduri[0].spec, testURL_02, "Old tag still associated with bookmark");
    
    var bookmarkitems = document.getElementById("bookmark-items");
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
    
    var tagstextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "tags");
    tagstextbox.value = "tagone, tag two, edited-tag-three, tag4";
    
    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    var untaggeduri = PlacesUtils.tagging.getURIsForTag("tag-three");
    is(untaggeduri, "", "Old tag is not associated with any bookmark");
    taggeduri = PlacesUtils.tagging.getURIsForTag("edited-tag-three");
    is(taggeduri[0].spec, testURL_02, "New tag is added to bookmark");
    var tagsarray = PlacesUtils.tagging.getTagsForURI(uri(testURL_02), {});
    is(tagsarray.length, 4, "Bookmark still has same number of tags");
    
    BookmarkList.close();
    
    runNextTest();
  }
});


//------------------------------------------------------------------------------
// Case: Test removing tags from bookmark
gTests.push({
  desc: "Test removing tags from a bookmark",
  _currenttab: null,

  run: function() {
    BookmarkList.show();  
    BookmarkList.toggleManage();
    
    waitFor(gCurrentTest.onEditorReady, function() { return document.getElementById("bookmark-items").manageUI == true; });
  },

  onEditorReady: function() {
    var bookmarkitems = document.getElementById("bookmark-items");
    var bookmarkitem = document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
    
    var tagstextbox = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "tags");
    tagstextbox.value = "tagone, tag two, tag4";
    
    var donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    var untaggeduri = PlacesUtils.tagging.getURIsForTag("edited-tag-three");
    is(untaggeduri, "", "Old tag is not associated with any bookmark");
    var tagsarray = PlacesUtils.tagging.getTagsForURI(uri(testURL_02), {});
    is(tagsarray.length, 3, "Tag is successfully deleted");
    
    BookmarkList.close();
    
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

