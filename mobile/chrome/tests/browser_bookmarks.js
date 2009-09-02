/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */
 
var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService); 
var thread = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager).currentThread;
var testURL_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
var testURL_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_02.html";
var chromeWindow = window;

//------------------------------------------------------------------------------
// TEST SKELETON: use this template to add new tests. This is based on 
// browser/components/places/tests/browser/browser_bookmarksProperties.js
/* 
gTests.push({
  desc: "Bug Description",
  isCompleted: false,
  
  run: function() {
    // Actual test ...
    
    // indicate test is completed
    gCurrentTest.isCompleted = true;
  },
  
});
*/

// A queue to order the tests and a handle for each test
gTests = [];
gCurrentTest = null;

//------------------------------------------------------------------------------
// Entry point (must be named 'test')
function test() {
  // test testing dependencies
  ok(isnot, "Mochitest must be in context");
  ok(ioService, "nsIIOService must be in context");
  ok(thread, "nsIThreadManager must be in context");
  ok(PlacesUtils, "PlacesUtils must be in context");
  ok(EventUtils, "EventUtils must be in context");
  ok(chromeWindow, "ChromeWindow must be in context");
  
  ok(true, "*** Starting test browser_bookmarks.js\n");  
  runNextTest();
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Clean up previous test
  if(gCurrentTest) {
    ok(true, "*** FINISHED TEST ***");
  }
  
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    ok(true, gCurrentTest.desc);
    gCurrentTest.run();
    while(!gCurrentTest.isCompleted) {
      thread.processNextEvent(true);
    }
    runNextTest();
  }
  else {
    // Cleanup. All tests are completed at this point
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.bookmarks.unfiledBookmarksFolder);
    ok(true, "*** ALL TESTS COMPLETED ***");
  }
}

//------------------------------------------------------------------------------
// Case: Test adding a bookmark with the Star button
gTests.push({
  desc: "Test adding a bookmark with the Star button",
  isCompleted: false,
  _currenttab: null,

  run: function() {
    _currenttab = chromeWindow.Browser.addTab(testURL_02, true);
    // Need to wait until the page is loaded
    _currenttab.browser.addEventListener("load", 
    function() {
      _currenttab.browser.removeEventListener("load", arguments.callee, true);
      gCurrentTest.verify();
    }, 
    true);
  },
  
  verify: function() {
    chromeWindow.BrowserUI.doCommand("cmd_star");
    
    var bookmarkItem = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_02));
    ok(bookmarkItem != -1, testURL_02 + " should be added.");
    is(PlacesUtils.bookmarks.getItemTitle(bookmarkItem),
      "Browser Blank Page 02", "the title should match."); 
    is(PlacesUtils.bookmarks.getBookmarkURI(bookmarkItem).spec,
      chromeWindow.Browser.selectedTab.browser.currentURI.spec, testURL_02 + " should be added.");  

    chromeWindow.Browser.closeTab(_currenttab);
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test clicking on a bookmark loads the web page
gTests.push({
  desc: "Test clicking on a bookmark loads the web page",
  isCompleted: false,

  run: function() {
    isnot(chromeWindow.Browser.selectedTab.browser.currentURI.spec, testURL_02, "Selected tab is not " + testURL_02);
    
    chromeWindow.BrowserUI.doCommand("cmd_newTab");
    chromeWindow.BrowserUI.showBookmarks();  
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");    
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    EventUtils.synthesizeMouse(bookmarkitem, bookmarkitem.clientWidth / 2, bookmarkitem.clientHeight / 2, {});
    
    chromeWindow.Browser.selectedTab.browser.addEventListener("load", 
    function() {
      chromeWindow.Browser.selectedTab.browser.removeEventListener("load", arguments.callee, true);
      is(chromeWindow.Browser.selectedTab.browser.currentURI.spec, testURL_02, "Selected tab is " + testURL_02);      
      chromeWindow.Browser.closeTab(chromeWindow.Browser.selectedTab);
      
      gCurrentTest.isCompleted = true;
    }, 
    true);
  },
  
});

//------------------------------------------------------------------------------
// Case: Test editing URI of existing bookmark
gTests.push({
  desc: "Test editing URI of existing bookmark",
  isCompleted: false,

  run: function() {
    chromeWindow.BrowserUI.showBookmarks();  
    chromeWindow.BookmarkList.toggleManage();
    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_02);
    var editbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "edit-button");    
    editbutton.click();

    var uritextbox = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "uri");
    uritextbox.value = testURL_01;
    
    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_02));
    is(bookmark, -1, testURL_02 + " should no longer in bookmark");
    bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01));
    isnot(bookmark, -1, testURL_01 + " is in bookmark");
    
    chromeWindow.BookmarkList.close();
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test editing title of existing bookmark
gTests.push({
  desc: "Test editing title of existing bookmark",
  isCompleted: false,
  
  run: function() {
    var newtitle = "Changed Title";
    chromeWindow.BrowserUI.showBookmarks();  
    chromeWindow.BookmarkList.toggleManage();
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01));
    is(PlacesUtils.bookmarks.getItemTitle(bookmark), "Browser Blank Page 02", "Title remains the same.");
    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_01);
    var editbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "edit-button");    
    editbutton.click();
    
    var titletextbox = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "name");
    titletextbox.value = newtitle;
    
    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    isnot(PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01)), -1, testURL_01 + " is still in bookmark.");
    is(PlacesUtils.bookmarks.getItemTitle(bookmark), newtitle, "Title is changed.");
    
    chromeWindow.BookmarkList.close();
    
    gCurrentTest.isCompleted = true;
  },

});

//------------------------------------------------------------------------------
// Case: Test removing existing bookmark
gTests.push({
  desc: "Test removing existing bookmark",
  isCompleted: false,
  _ximage: null,
  
  run: function() {
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();
    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "uri", testURL_01);
    // Close button is an image so not loaded immediately, thus need to listen for its load event before usage
    _ximage = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "close-button");
    _ximage.addEventListener("load", 
    function() {
      _ximage.removeEventListener("load", arguments.callee, true);
      gCurrentTest.verify();
    }, 
    true);
  },

  verify: function() {
    EventUtils.synthesizeMouse(_ximage, _ximage.clientWidth / 2, _ximage.clientHeight / 2, {});
    
    var bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_02));
    ok(bookmark == -1, testURL_02 + " should no longer in bookmark");
    bookmark = PlacesUtils.getMostRecentBookmarkForURI(uri(testURL_01));
    ok(bookmark == -1, testURL_01 + " should no longer in bookmark");

    chromeWindow.BookmarkList.close();

    gCurrentTest.isCompleted = true;
  },

});

//------------------------------------------------------------------------------
// Helpers
function uri(spec) {
  return ioService.newURI(spec, null, null);
}
