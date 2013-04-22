/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";

// An object representing the contents of bookmarks.preplaces.html.
let test_bookmarks = {
  menu: [
    { title: "Mozilla Firefox",
      children: [
        { title: "Help and Tutorials",
          url: "http://en-us.www.mozilla.com/en-US/firefox/help/",
          icon: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg=="
        },
        { title: "Customize Firefox",
          url: "http://en-us.www.mozilla.com/en-US/firefox/customize/",
          icon: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg=="
        },
        { title: "Get Involved",
          url: "http://en-us.www.mozilla.com/en-US/firefox/community/",
          icon: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg=="
        },
        { title: "About Us",
          url: "http://en-us.www.mozilla.com/en-US/about/",
          icon: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg=="
        }
      ]
    },
    {
      type: Ci.nsINavHistoryResultNode.RESULT_TYPE_SEPARATOR
    },
    { title: "test",
      description: "folder test comment",
      dateAdded: 1177541020000000,
      lastModified: 1177541050000000,
      children: [
        { title: "test post keyword",
          description: "item description",
          dateAdded: 1177375336000000,
          lastModified: 1177375423000000,
          keyword: "test",
          sidebar: true,
          postData: "hidden1%3Dbar&text1%3D%25s",
          charset: "ISO-8859-1"
        }
      ]
    }
  ],
  toolbar: [
    { title: "Getting Started",
      url: "http://en-us.www.mozilla.com/en-US/firefox/central/",
      icon: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABGdBTUEAAK/INwWK6QAAABl0RVh0U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAHWSURBVHjaYvz//z8DJQAggJiQOe/fv2fv7Oz8rays/N+VkfG/iYnJfyD/1+rVq7ffu3dPFpsBAAHEAHIBCJ85c8bN2Nj4vwsDw/8zQLwKiO8CcRoQu0DxqlWrdsHUwzBAAIGJmTNnPgYa9j8UqhFElwPxf2MIDeIrKSn9FwSJoRkAEEAM0DD4DzMAyPi/G+QKY4hh5WAXGf8PDQ0FGwJ22d27CjADAAIIrLmjo+MXA9R2kAHvGBA2wwx6B8W7od6CeQcggKCmCEL8bgwxYCbUIGTDVkHDBia+CuotgACCueD3TDQN75D4xmAvCoK9ARMHBzAw0AECiBHkAlC0Mdy7x9ABNA3obAZXIAa6iKEcGlMVQHwWyjYuL2d4v2cPg8vZswx7gHyAAAK7AOif7SAbOqCmn4Ha3AHFsIDtgPq/vLz8P4MSkJ2W9h8ggBjevXvHDo4FQUQg/kdypqCg4H8lUIACnQ/SOBMYI8bAsAJFPcj1AAEEjwVQqLpAbXmH5BJjqI0gi9DTAAgDBBCcAVLkgmQ7yKCZxpCQxqUZhAECCJ4XgMl493ug21ZD+aDAXH0WLM4A9MZPXJkJIIAwTAR5pQMalaCABQUULttBGCCAGCnNzgABBgAMJ5THwGvJLAAAAABJRU5ErkJggg=="
    },
    { title: "Latest Headlines",
      url: "http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
      feedUrl: "http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml"
    }
  ],
  unfiled: [
    { title: "Example.tld",
      url: "http://example.tld/"
    }
  ]
};

// Pre-Places bookmarks.html file pointer.
let gBookmarksFileOld;
// Places bookmarks.html file pointer.
let gBookmarksFileNew;

Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

function run_test()
{
  run_next_test();
}

add_task(function setup() {
  // Avoid creating smart bookmarks during the test.
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", -1);

  // File pointer to legacy bookmarks file.
  gBookmarksFileOld = do_get_file("bookmarks.preplaces.html");

  // File pointer to a new Places-exported bookmarks file.
  gBookmarksFileNew = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  gBookmarksFileNew.append("bookmarks.exported.html");
  if (gBookmarksFileNew.exists()) {
    gBookmarksFileNew.remove(false);
  }

  // This test must be the first one, since it setups the new bookmarks.html.
  // Test importing a pre-Places canonical bookmarks file.
  // 1. import bookmarks.preplaces.html
  // 2. run the test-suite
  // Note: we do not empty the db before this import to catch bugs like 380999
  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileOld, true);
  yield promiseAsyncUpdates();
  yield testImportedBookmarks();

  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);
  yield promiseAsyncUpdates();
  remove_all_bookmarks();
});

add_task(function test_import_new()
{
  // Test importing a Places bookmarks.html file.
  // 1. import bookmarks.exported.html
  // 2. run the test-suite
  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  yield promiseAsyncUpdates();

  yield testImportedBookmarks();
  yield promiseAsyncUpdates();

  remove_all_bookmarks();
});

add_task(function test_emptytitle_export()
{
  // Test exporting and importing with an empty-titled bookmark.
  // 1. import bookmarks
  // 2. create an empty-titled bookmark.
  // 3. export to bookmarks.exported.html
  // 4. empty bookmarks db
  // 5. import bookmarks.exported.html
  // 6. run the test-suite
  // 7. remove the empty-titled bookmark
  // 8. export to bookmarks.exported.html
  // 9. empty bookmarks db and continue

  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);

  const NOTITLE_URL = "http://notitle.mozilla.org/";
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                NetUtil.newURI(NOTITLE_URL),
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "");
  test_bookmarks.unfiled.push({ title: "", url: NOTITLE_URL });

  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);
  remove_all_bookmarks();

  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  yield promiseAsyncUpdates();
  yield testImportedBookmarks();

  // Cleanup.
  test_bookmarks.unfiled.pop();
  PlacesUtils.bookmarks.removeItem(id);

  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);
  yield promiseAsyncUpdates();
  remove_all_bookmarks();
});

add_task(function test_import_chromefavicon()
{
  // Test exporting and importing with a bookmark pointing to a chrome favicon.
  // 1. import bookmarks
  // 2. create a bookmark pointing to a chrome favicon.
  // 3. export to bookmarks.exported.html
  // 4. empty bookmarks db
  // 5. import bookmarks.exported.html
  // 6. run the test-suite
  // 7. remove the bookmark pointing to a chrome favicon.
  // 8. export to bookmarks.exported.html
  // 9. empty bookmarks db and continue

  const PAGE_URI = NetUtil.newURI("http://example.com/chromefavicon_page");
  const CHROME_FAVICON_URI = NetUtil.newURI("chrome://global/skin/icons/information-16.png");
  const CHROME_FAVICON_URI_2 = NetUtil.newURI("chrome://global/skin/icons/error-16.png");

  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  let id = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                                PAGE_URI,
                                                PlacesUtils.bookmarks.DEFAULT_INDEX,
                                                "Test");

  let deferred = Promise.defer();
  PlacesUtils.favicons.setAndFetchFaviconForPage(
                                  PAGE_URI, CHROME_FAVICON_URI, true,
                                  PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                  deferred.resolve);
  yield deferred.promise;

  deferred = Promise.defer();
  PlacesUtils.favicons.getFaviconDataForPage(PAGE_URI,
    function (aURI, aDataLen, aData, aMimeType) deferred.resolve(aData));
  let data = yield deferred.promise;

  let base64Icon = "data:image/png;base64," +
      base64EncodeString(String.fromCharCode.apply(String, data));

  test_bookmarks.unfiled.push(
    { title: "Test", url: PAGE_URI.spec, icon: base64Icon });

  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);

  // Change the favicon to check it's really imported again later.
  deferred = Promise.defer();
  PlacesUtils.favicons.setAndFetchFaviconForPage(
                                  PAGE_URI, CHROME_FAVICON_URI_2, true,
                                  PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
                                  deferred.resolve);
  yield deferred.promise;

  remove_all_bookmarks();

  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  yield promiseAsyncUpdates();
  yield testImportedBookmarks();

  // Cleanup.
  test_bookmarks.unfiled.pop();
  PlacesUtils.bookmarks.removeItem(id);

  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);
  yield promiseAsyncUpdates();
  remove_all_bookmarks();
});

add_task(function test_import_ontop()
{
  // Test importing the exported bookmarks.html file *on top of* the existing
  // bookmarks.
  // 1. empty bookmarks db
  // 2. import the exported bookmarks file
  // 3. export to file
  // 3. import the exported bookmarks file
  // 4. run the test-suite

  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  yield BookmarkHTMLUtils.exportToFile(gBookmarksFileNew);
  yield BookmarkHTMLUtils.importFromFile(gBookmarksFileNew, true);
  yield promiseAsyncUpdates();
  yield testImportedBookmarks();
  yield promiseAsyncUpdates();
  remove_all_bookmarks();
});

function testImportedBookmarks()
{
  for (let group in test_bookmarks) {
    do_print("[testImportedBookmarks()] Checking group '" + group + "'");

    let root;
    switch (group) {
      case "menu":
        root = PlacesUtils.getFolderContents(PlacesUtils.bookmarksMenuFolderId).root;
        break;
      case "toolbar":
        root = PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId).root;
        break;
      case "unfiled":
        root = PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
        break;
    }

    let items = test_bookmarks[group];
    do_check_eq(root.childCount, items.length);

    for (let key in items) {
      yield checkItem(items[key], root.getChild(key));
    }

    root.containerOpen = false;
  }
}

function testImportedBookmarksToFolder(aFolder)
{
  root = PlacesUtils.getFolderContents(aFolder).root;

  // Menu bookmarks are put directly into the folder, while other roots are
  // imported into subfolders.
  let rootFolderCount = test_bookmarks.menu.length;

  for (let i = 0; i < root.childCount; i++) {
    let child = root.getChild(i);
    // This check depends on all "menu" bookmarks being listed first in the imported file :-|
    if (i < rootFolderCount) {
      checkItem(test_bookmarks.menu[i], child);
    }
    else {
      let container = child.QueryInterface(Ci.nsINavHistoryContainerResultNode);
      let group = /Toolbar/.test(container.title) ? test_bookmarks.toolbar
                                                  : test_bookmarks.unfiled;
      container.containerOpen = true;
      do_print("[testImportedBookmarksToFolder()] Checking container '" + container.title + "'");
      for (let t = 0; t < container.childCount; t++) {
        checkItem(group[t], container.getChild(t));
      }
      container.containerOpen = false;
    }
  }

  root.containerOpen = false;
}

function checkItem(aExpected, aNode)
{
  let id = aNode.itemId;

  return Task.spawn(function() {
    for (prop in aExpected) {
      switch (prop) {
        case "type":
          do_check_eq(aNode.type, aExpected.type);
          break;
        case "title":
          do_check_eq(aNode.title, aExpected.title);
          break;
        case "description":
          do_check_eq(PlacesUtils.annotations
                                 .getItemAnnotation(id, DESCRIPTION_ANNO),
                      aExpected.description);
          break;
        case "dateAdded":
          do_check_eq(PlacesUtils.bookmarks.getItemDateAdded(id),
                      aExpected.dateAdded);
          break;
        case "lastModified":
          do_check_eq(PlacesUtils.bookmarks.getItemLastModified(id),
                      aExpected.lastModified);
          break;
        case "url":
          yield function() {
            let deferred = Promise.defer();
            PlacesUtils.livemarks.getLivemark(
              { id: id },
              function (aStatus, aLivemark) {
                if (!Components.isSuccessCode(aStatus)) {
                  do_check_eq(aNode.uri, aExpected.url);
                }
                deferred.resolve();
              }
            );
          return deferred.promise; }();
          break;
        case "icon":
          yield function() {
            let deferred = Promise.defer();
            PlacesUtils.favicons.getFaviconDataForPage(
              NetUtil.newURI(aExpected.url),
              function (aURI, aDataLen, aData, aMimeType) {
                let base64Icon = "data:image/png;base64," +
                  base64EncodeString(String.fromCharCode.apply(String, aData));
                do_check_true(base64Icon == aExpected.icon);
                deferred.resolve();
            });
            return deferred.promise; }();
          break;
        case "keyword":
          break;
        case "sidebar":
          do_check_eq(PlacesUtils.annotations
                                 .itemHasAnnotation(id, LOAD_IN_SIDEBAR_ANNO),
                      aExpected.sidebar);
          break;
        case "postData":
          do_check_eq(PlacesUtils.annotations
                                 .getItemAnnotation(id, PlacesUtils.POST_DATA_ANNO),
                      aExpected.postData);
          break;
        case "charset":
          let testURI = NetUtil.newURI(aNode.uri);
          do_check_eq((yield PlacesUtils.getCharsetForURI(testURI)), aExpected.charset);
          break;
        case "feedUrl":
          yield function() {
            let deferred = Promise.defer();
            PlacesUtils.livemarks.getLivemark(
              { id: id },
              function (aStatus, aLivemark) {
                do_check_true(Components.isSuccessCode(aStatus));
                do_check_eq(aLivemark.siteURI.spec, aExpected.url);
                do_check_eq(aLivemark.feedURI.spec, aExpected.feedUrl);
                deferred.resolve();
              }
            );
          return deferred.promise; }();
          break;
        case "children":
          let folder = aNode.QueryInterface(Ci.nsINavHistoryContainerResultNode);
          do_check_eq(folder.hasChildren, aExpected.children.length > 0);
          folder.containerOpen = true;
          do_check_eq(folder.childCount, aExpected.children.length);

          aExpected.children.forEach(function (item, index) checkItem(item, folder.getChild(index)));

          folder.containerOpen = false;
          break;
        default:
          throw new Error("Unknown property");
      }
    }
  });
}
