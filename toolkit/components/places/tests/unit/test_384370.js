/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const DESCRIPTION_ANNO = "bookmarkProperties/description";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";

do_check_eq(typeof PlacesUtils, "object");

// main
function run_test() {
  do_test_pending();

  /*
    HTML+FEATURES SUMMARY:
    - import legacy bookmarks
    - export as json, import, test (tests integrity of html > json)
    - export as html, import, test (tests integrity of json > html)

    BACKUP/RESTORE SUMMARY:
    - create a bookmark in each root
    - tag multiple URIs with multiple tags
    - export as json, import, test
  */

  // import the importer
  Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

  // file pointer to legacy bookmarks file
  //var bookmarksFileOld = do_get_file("bookmarks.large.html");
  var bookmarksFileOld = do_get_file("bookmarks.preplaces.html");
  // file pointer to a new places-exported json file
  var jsonFile = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  jsonFile.append("bookmarks.exported.json");

  // create bookmarks.exported.json
  if (jsonFile.exists())
    jsonFile.remove(false);
  jsonFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, 0600);
  if (!jsonFile.exists())
    do_throw("couldn't create file: bookmarks.exported.json");

  // Test importing a pre-Places canonical bookmarks file.
  // 1. import bookmarks.preplaces.html
  // Note: we do not empty the db before this import to catch bugs like 380999
  try {
    BookmarkHTMLUtils.importFromFile(bookmarksFileOld, true)
                     .then(after_import, do_report_unexpected_exception);
  } catch(ex) { do_throw("couldn't import legacy bookmarks file: " + ex); }

  function after_import() {
    populate();

    // 2. run the test-suite
    validate();
  
    promiseAsyncUpdates().then(function testJsonExport() {
      // Test exporting a Places canonical json file.
      // 1. export to bookmarks.exported.json
      try {
        PlacesUtils.backups.saveBookmarksToJSONFile(jsonFile);
      } catch(ex) { do_throw("couldn't export to file: " + ex); }
      LOG("exported json");

      // 2. empty bookmarks db
      // 3. import bookmarks.exported.json
      try {
        PlacesUtils.restoreBookmarksFromJSONFile(jsonFile);
      } catch(ex) { do_throw("couldn't import the exported file: " + ex); }
      LOG("imported json");

      // 4. run the test-suite
      validate();
      LOG("validated import");
  
      promiseAsyncUpdates().then(do_test_finished);
    });
  }
}

var tagData = [
  { uri: uri("http://slint.us"), tags: ["indie", "kentucky", "music"] },
  { uri: uri("http://en.wikipedia.org/wiki/Diplodocus"), tags: ["dinosaur", "dj", "rad word"] }
];

var bookmarkData = [
  { uri: uri("http://slint.us"), title: "indie, kentucky, music" },
  { uri: uri("http://en.wikipedia.org/wiki/Diplodocus"), title: "dinosaur, dj, rad word" }
];

/*
populate data in each folder
(menu is populated via the html import)
*/
function populate() {
  // add tags
  for each(let {uri: u, tags: t} in tagData)
    PlacesUtils.tagging.tagURI(u, t);
  
  // add unfiled bookmarks
  for each(let {uri: u, title: t} in bookmarkData) {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.unfiledBookmarksFolder,
                                         u, PlacesUtils.bookmarks.DEFAULT_INDEX, t);
  }

  // add to the toolbar
  for each(let {uri: u, title: t} in bookmarkData) {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarks.toolbarFolder,
                                         u, PlacesUtils.bookmarks.DEFAULT_INDEX, t);
  }
}

function validate() {
  testCanonicalBookmarks(PlacesUtils.bookmarks.bookmarksMenuFolder);
  testToolbarFolder();
  testUnfiledBookmarks();
  testTags();
}

// Tests a bookmarks datastore that has a set of bookmarks, etc
// that flex each supported field and feature.
function testCanonicalBookmarks() {
  // query to see if the deleted folder and items have been imported
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.bookmarksMenuFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;

  // Count expected bookmarks in the menu root.
  do_check_eq(rootNode.childCount, 3);

  // check separator
  var testSeparator = rootNode.getChild(1);
  do_check_eq(testSeparator.type, testSeparator.RESULT_TYPE_SEPARATOR);

  // get test folder
  var testFolder = rootNode.getChild(2);
  do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
  do_check_eq(testFolder.title, "test");

  /*
  // add date 
  do_check_eq(PlacesUtils.bookmarks.getItemDateAdded(testFolder.itemId)/1000000, 1177541020);
  // last modified
  do_check_eq(PlacesUtils.bookmarks.getItemLastModified(testFolder.itemId)/1000000, 1177541050);
  */

  testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
  do_check_eq(testFolder.hasChildren, true);
  // folder description
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testFolder.itemId,
                                                          DESCRIPTION_ANNO));
  do_check_eq("folder test comment",
              PlacesUtils.annotations.getItemAnnotation(testFolder.itemId, DESCRIPTION_ANNO));
  // open test folder, and test the children
  testFolder.containerOpen = true;
  var cc = testFolder.childCount;
  // XXX Bug 380468
  // do_check_eq(cc, 2);
  do_check_eq(cc, 1);

  // test bookmark 1
  var testBookmark1 = testFolder.getChild(0);
  // url
  do_check_eq("http://test/post", testBookmark1.uri);
  // title
  do_check_eq("test post keyword", testBookmark1.title);
  // keyword
  do_check_eq("test", PlacesUtils.bookmarks.getKeywordForBookmark(testBookmark1.itemId));
  // sidebar
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId,
                                                          LOAD_IN_SIDEBAR_ANNO));
  /*
  // add date 
  do_check_eq(testBookmark1.dateAdded/1000000, 1177375336);

  // last modified
  do_check_eq(testBookmark1.lastModified/1000000, 1177375423);
  */

  // post data
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId, POST_DATA_ANNO));
  do_check_eq("hidden1%3Dbar&text1%3D%25s",
              PlacesUtils.annotations.getItemAnnotation(testBookmark1.itemId, POST_DATA_ANNO));

  // last charset
  var testURI = PlacesUtils._uri(testBookmark1.uri);
  do_check_eq("ISO-8859-1", PlacesUtils.history.getCharsetForURI(testURI));

  // description 
  do_check_true(PlacesUtils.annotations.itemHasAnnotation(testBookmark1.itemId,
                                                          DESCRIPTION_ANNO));
  do_check_eq("item description",
              PlacesUtils.annotations.getItemAnnotation(testBookmark1.itemId,
                                                        DESCRIPTION_ANNO));

  // clean up
  testFolder.containerOpen = false;
  rootNode.containerOpen = false;
}

function testToolbarFolder() {
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.toolbarFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());

  var toolbar = result.root;
  toolbar.containerOpen = true;

  // child count (add 2 for pre-existing items)
  do_check_eq(toolbar.childCount, bookmarkData.length + 2);
  
  // livemark
  var livemark = toolbar.getChild(1);
  // title
  do_check_eq("Latest Headlines", livemark.title);

  PlacesUtils.livemarks.getLivemark(
    { id: livemark.itemId },
    function (aStatus, aLivemark) {
      do_check_true(Components.isSuccessCode(aStatus));
      do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
                  aLivemark.siteURI.spec);
      do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
                  aLivemark.feedURI.spec);
    }
  );

  // test added bookmark data
  var child = toolbar.getChild(2);
  do_check_eq(child.uri, bookmarkData[0].uri.spec);
  do_check_eq(child.title, bookmarkData[0].title);
  child = toolbar.getChild(3);
  do_check_eq(child.uri, bookmarkData[1].uri.spec);
  do_check_eq(child.title, bookmarkData[1].title);

  toolbar.containerOpen = false;
}

function testUnfiledBookmarks() {
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.bookmarks.unfiledBookmarksFolder], 1);
  var result = PlacesUtils.history.executeQuery(query, PlacesUtils.history.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;
  // child count (add 1 for pre-existing item)
  do_check_eq(rootNode.childCount, bookmarkData.length + 1);
  for (var i = 1; i < rootNode.childCount; i++) {
    var child = rootNode.getChild(i);
    dump(bookmarkData[i - 1].uri.spec + " == " + child.uri + "?\n");
    do_check_true(bookmarkData[i - 1].uri.equals(uri(child.uri)));
    do_check_eq(child.title, bookmarkData[i - 1].title);
    /* WTF
    if (child.tags)
      do_check_eq(child.tags, bookmarkData[i].title);
    */
  }
  rootNode.containerOpen = false;
}

function testTags() {
  for each(let {uri: u, tags: t} in tagData) {
    var i = 0;
    dump("test tags for " + u.spec + ": " + t + "\n");
    var tt = PlacesUtils.tagging.getTagsForURI(u);
    dump("true tags for " + u.spec + ": " + tt + "\n");
    do_check_true(t.every(function(el) {
      i++;
      return tt.indexOf(el) > -1;
    }));
    do_check_eq(i, t.length);
  }
}
