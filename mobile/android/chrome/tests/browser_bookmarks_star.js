/*
 * Bug 486490 - Fennec browser-chrome tests to verify correct implementation of chrome 
 *              code in mobile/chrome/content in terms of integration with Places
 *              component, specifically for bookmark management.
 */

let testURL_01 = chromeRoot + "browser_blank_01.html";
let testURL_02 = chromeRoot + "browser_blank_02.html";

function test() {
  BookmarkHelper.logging = true;

  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  runNextTest();
}

registerCleanupFunction(function() {
  BookmarkHelper.logging = false;
  PlacesUtils.bookmarks.removeFolderChildren(BookmarkList.panel.mobileRoot);
});

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

        // Wait a bit until Places is initialized
        waitFor(gCurrentTest.onPageLoad, function() {
          let mobileRoot = PlacesUtils.annotations.getItemsWithAnnotation("mobile/bookmarksRoot", {})[0];
          return mobileRoot;
        });
      }
    });
  },

  onPageLoad: function() {
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() { return BookmarkPopup.box.hidden == false; });
  },

  onPopupReady: function() {
    // Let's make it disappear again by clicking the star again
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupGone, function() { return BookmarkPopup.box.hidden == true; });
  },

  onPopupGone: function() {
    // Make sure it's hidden again
    is(BookmarkPopup.box.hidden, true, "Bookmark popup should be hidden by clicking star");

    // Let's make it appear again and continue the test
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady2, function() { return BookmarkPopup.box.hidden == false; });
  },

  onPopupReady2: function() {
    // Let's make it disappear again by clicking somewhere
    let contentarea = document.getElementById("browsers");
    EventUtils.synthesizeMouse(contentarea, contentarea.clientWidth / 2, contentarea.clientHeight / 2, {});

    waitFor(gCurrentTest.onPopupGone2, function() { return BookmarkPopup.box.hidden == true; });
  },

  onPopupGone2: function() {
    // Make sure it's hidden again
    is(BookmarkPopup.box.hidden, true, "Bookmark popup should be hidden by clicking in content");

    BookmarkHelper.removeBookmarksForURI(getBrowser().currentURI);
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
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() { return BookmarkPopup.box.hidden == false });
  },

  onPopupReady: function() {
    let editbutton = document.getElementById("bookmark-popup-edit");
    editbutton.click();

    waitFor(gCurrentTest.onEditorReady, function() {
      let item = document.getElementById("bookmark-item");
      return item && item.isEditing == true;
    });
  },

  onEditorReady: function() {
    let bookmarkitem = document.getElementById("bookmark-item");
    bookmarkitem.tags = "tagone, tag two, tag-three, tag4";

    let donebutton = document.getAnonymousElementByAttribute(bookmarkitem, "anonid", "done-button");
    donebutton.click();

    waitFor(gCurrentTest.onEditorDone, function() { return document.getElementById("bookmark-container").hidden == true; });
  },

  onEditorDone: function() {
    let uri = makeURI(testURL_02);
    let tagsarray = PlacesUtils.tagging.getTagsForURI(uri, {});
    is(tagsarray.length, 4, "All tags are added.");

    BookmarkHelper.removeBookmarksForURI(uri);
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

    waitFor(gCurrentTest.onPopupReady, function() {
      return BookmarkPopup.box.hidden == false &&
             PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_02)) != -1;
    });
  },

  onPopupReady: function() {
    let editbutton = document.getElementById("bookmark-popup-edit");
    editbutton.click();

    waitFor(gCurrentTest.onEditorReady, function() {
      let item = document.getElementById("bookmark-item");
      return item && item.isEditing == true;
    });
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

    BookmarkHelper.removeBookmarksForURI(makeURI(testURL_02));
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
    let starbutton = document.getElementById("tool-star");
    starbutton.click();

    waitFor(gCurrentTest.onPopupReady, function() {
      return BookmarkPopup.box.hidden == false &&
             PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01)) != -1;
    });
  },

  onPopupReady: function() {
    let removebutton = document.getElementById("bookmark-popup-remove");
    removebutton.click();

    let bookmark = PlacesUtils.getMostRecentBookmarkForURI(makeURI(testURL_01));
    ok(bookmark == -1, testURL_01 + " should no longer in bookmark");

    BrowserUI.closeTab(this._currentTab);

    runNextTest();
  }
});
