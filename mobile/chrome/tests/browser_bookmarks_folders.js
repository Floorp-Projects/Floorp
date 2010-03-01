/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */

var thread = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager).currentThread;
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
var gTests = [];
var gCurrentTest = null;

//------------------------------------------------------------------------------
// Entry point (must be named 'test')
function test() {
  // test testing dependencies
  ok(isnot, "Mochitest must be in context");
  ok(thread, "nsIThreadManager must be in context");
  ok(PlacesUtils, "PlacesUtils must be in context");
  ok(EventUtils, "EventUtils must be in context");
  ok(gIOService, "nsIIOService must be in context");
  ok(chromeWindow, "ChromeWindow must be in context");
  
  ok(true, "*** Starting test browser_bookmark_folders.js\n"); 
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
    PlacesUtils.bookmarks.removeFolderChildren(BookmarkList.mobileRoot);
    ok(true, "*** ALL TESTS COMPLETED ***");
  }
}

//------------------------------------------------------------------------------
// Case: Test adding folder
gTests.push({
  desc: "Test adding folder",
  isCompleted: false,
  
  run: function() {  
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "class", "bookmark-item");
    is(bookmarkitem, null, "folder does not exist yet");
    
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var newfolderbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "class", "bookmark-folder-new");
    EventUtils.synthesizeMouse(newfolderbutton, newfolderbutton.clientWidth / 2, newfolderbutton.clientHeight / 2, {});

    bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "New folder");
    var nametextbox = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "name");
    nametextbox.value = "Test Folder";

    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    is(PlacesUtils.bookmarks.getItemType(bookmarkitem.itemId), PlacesUtils.bookmarks.TYPE_FOLDER, "bookmark item is a folder");
    is(PlacesUtils.bookmarks.getItemTitle(bookmarkitem.itemId), "Test Folder", "folder is created");
    chromeWindow.BookmarkList.close();
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test editing folder
gTests.push({
  desc: "Test editing folder",
  isCompleted: false,
  
  run: function() {  
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "Test Folder");
    
    var editbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "edit-button");
    editbutton.click();
    
    var nametextbox = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "name");
    nametextbox.value = "Edited Test Folder";

    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();
    
    is(PlacesUtils.bookmarks.getItemTitle(bookmarkitem.itemId), "Edited Test Folder", "folder is successfully edited");
    chromeWindow.BookmarkList.close();
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test removing folder
gTests.push({
  desc: "Test removing folder",
  isCompleted: false,
  
  run: function() {  
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();
    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "Edited Test Folder");
    var folderid = bookmarkitem.itemId;

    var closeimagebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "close-button");
    // Close button is an image so not loaded immediately, thus need to listen for its load event before usage
    var handleevent2 = function() {
      closeimagebutton.removeEventListener("load", handleevent2, true);      
      EventUtils.synthesizeMouse(closeimagebutton, closeimagebutton.clientWidth / 2, closeimagebutton.clientHeight / 2, {});
      
      // To verify a folder is deleted is to call a function using its ID and look for an exception.
      // Thus need to use a catch block to verify deleted folder.
      try {
        var title = PlacesUtils.bookmarks.getItemTitle(folderid);
        ok(false, "folder is not removed"); 
      } catch(error) {
        ok(error.message, "folder is removed, folder ID is not longer valid");
      }
      chromeWindow.BookmarkList.close();
      
      gCurrentTest.isCompleted = true;
    };
    closeimagebutton.addEventListener("load", handleevent2, true);
  },
  
});

//------------------------------------------------------------------------------
// Case: Test moving a bookmark into a folder
gTests.push({
  desc: "Test moving a bookmark into a folder",
  isCompleted: false,
  _currenttab: null,
  
  run: function() {  
    _currenttab = chromeWindow.Browser.addTab(testURL_02, true);
    var handleevent1 = function() {
      _currenttab.browser.removeEventListener("load", handleevent1, true);
      gCurrentTest.verify();
    };
    _currenttab.browser.addEventListener("load", handleevent1 , true);
  },
  
  verify: function() {
  
    // creates a folder, and then moves the bookmark into the folder
    
    chromeWindow.BrowserUI.doCommand("cmd_star");
    
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();

    // Create new folder
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    var newfolderbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "class", "bookmark-folder-new");
    EventUtils.synthesizeMouse(newfolderbutton, newfolderbutton.clientWidth / 2, newfolderbutton.clientHeight / 2, {});

    var folderitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "New folder");
    var nametextbox = chromeWindow.document.getAnonymousElementByAttribute(folderitem, "anonid", "name");
    nametextbox.value = "Test Folder 1";
    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(folderitem, "anonid", "done-button");
    donebutton.click();

    var bookmarkitemid = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02));
    is(PlacesUtils.bookmarks.getFolderIdForItem(bookmarkitemid), BookmarkList.mobileRoot, "bookmark starts off in root");

    // Move bookmark
    var bookmarkitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "itemid", bookmarkitemid);
    var movebutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "folder-button");
    movebutton.click();
    
    var folderitems = chromeWindow.document.getElementById("folder-items");
    var destfolder = chromeWindow.document.getAnonymousElementByAttribute(folderitems, "itemid", folderitem.itemId);
    EventUtils.synthesizeMouse(destfolder, destfolder.clientWidth / 2, destfolder.clientHeight / 2, {});

    // Check that it moved
    is(PlacesUtils.bookmarks.getFolderIdForItem(bookmarkitemid), folderitem.itemId, "Bookmark is moved to a folder");
    
    chromeWindow.BookmarkList.close();
    chromeWindow.Browser.closeTab(_currenttab);
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test moving a folder into a folder
gTests.push({
  desc: "Test moving a folder into a folder",
  isCompleted: false,
  
  run: function() {
  
    // the test creates a new folder ("Test Folder 2"), and then move a previously created folder ("Test Folder 1") 
    // into the newly created folder
    
    chromeWindow.BrowserUI.showBookmarks();
    chromeWindow.BookmarkList.toggleManage();    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");

    // Create the new folder
    var newfolderbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "class", "bookmark-folder-new");
    EventUtils.synthesizeMouse(newfolderbutton, newfolderbutton.clientWidth / 2, newfolderbutton.clientHeight / 2, {});
    var folderitem2 = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "New folder");
    var nametextbox = chromeWindow.document.getAnonymousElementByAttribute(folderitem2, "anonid", "name");
    nametextbox.value = "Test Folder 2";
    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(folderitem2, "anonid", "done-button");
    donebutton.click();
    
    // Check the old folder
    var folderitem1 = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "Test Folder 1");
    var folderitem1id = folderitem1.itemId;
    is(foldetitem1id, BookmarksList.mobileRoot, "folder starts off in the root");

    // Move new folder into old folder
    var movebutton = chromeWindow.document.getAnonymousElementByAttribute(folderitem1, "anonid", "folder-button");
    movebutton.click();

    var folderitems = chromeWindow.document.getElementById("folder-items");
    var destfolder = chromeWindow.document.getAnonymousElementByAttribute(folderitems, "itemid", folderitem2.itemId);
    EventUtils.synthesizeMouse(destfolder, destfolder.clientWidth / 2, destfolder.clientHeight / 2, {});

    is(PlacesUtils.bookmarks.getFolderIdForItem(folderitem1id), folderitem2.itemId, "Folder is moved to another folder");
    
    chromeWindow.BookmarkList.close();
    
    gCurrentTest.isCompleted = true;
  },
  
});

//------------------------------------------------------------------------------
// Case: Test adding, editing, deleting a subfolder in a folder
gTests.push({
  desc: "Test adding a subfolder into a folder",
  isCompleted: false,
  
  run: function() {
    chromeWindow.BrowserUI.showBookmarks();
    
    var bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    
    // creates a folder in existing folder ("Test Folder 2")
    
    var folderitem = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "Test Folder 2");
    var folderitemid = folderitem.itemId;
    EventUtils.synthesizeMouse(folderitem, folderitem.clientWidth / 2, folderitem.clientHeight / 2, {});
    
    bookmarkitems = chromeWindow.document.getElementById("bookmark-items");
    chromeWindow.BookmarkList.toggleManage();
    
    var newfolderbutton = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "class", "bookmark-folder-new");
    EventUtils.synthesizeMouse(newfolderbutton, newfolderbutton.clientWidth / 2, newfolderbutton.clientHeight / 2, {});
    
    var newsubfolder = chromeWindow.document.getAnonymousElementByAttribute(bookmarkitems, "title", "New folder");
    var nametextbox = chromeWindow.document.getAnonymousElementByAttribute(newsubfolder, "anonid", "name");
    nametextbox.value = "Test Subfolder 1";
    
    var donebutton = chromeWindow.document.getAnonymousElementByAttribute(newsubfolder, "anonid", "done-button");
    donebutton.click();
    
    is(PlacesUtils.bookmarks.getFolderIdForItem(newsubfolder.itemId), folderitemid, "Subfolder is created");
    
    // edits the folder title
    
    var editbutton = chromeWindow.document.getAnonymousElementByAttribute(newsubfolder, "anonid", "edit-button");
    editbutton.click();
    
    nametextbox = chromeWindow.document.getAnonymousElementByAttribute(newsubfolder, "anonid", "name");
    nametextbox.value = "Edited Test Subfolder 1";    
    donebutton.click();
    
    is(PlacesUtils.bookmarks.getItemTitle(newsubfolder.itemId), "Edited Test Subfolder 1", "Subfolder is successfully edited");
    
    // removes the folder
    
    var closeimagebutton = chromeWindow.document.getAnonymousElementByAttribute(newsubfolder, "anonid", "close-button");
    var handleevent3 = function() {
      closeimagebutton.removeEventListener("load", handleevent3, true);
      EventUtils.synthesizeMouse(closeimagebutton, closeimagebutton.clientWidth / 2, closeimagebutton.clientHeight / 2, {});
      
      // To verify a folder is deleted is to call a function using its ID and look for an exception.
      // Thus need to use a catch block to verify deleted folder.
      try {
        var title = PlacesUtils.bookmarks.getItemTitle(newsubfolder.itemId);
        ok(false, "folder is not removed");
      } catch(error) {
        ok(error.message, "Subfolder is removed, folder ID is not longer valid");
      }
      chromeWindow.BookmarkList.close();
      
      gCurrentTest.isCompleted = true;
    };
    closeimagebutton.addEventListener("load", handleevent3, true);
  },
  
});
