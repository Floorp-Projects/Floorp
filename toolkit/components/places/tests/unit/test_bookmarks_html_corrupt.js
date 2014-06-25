/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test ensures that importing/exporting to HTML does not stop
 * if a malformed uri is found.
 */

// Get Services
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var dbConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);
var icos = Cc["@mozilla.org/browser/favicon-service;1"].
           getService(Ci.nsIFaviconService);
var ps = Cc["@mozilla.org/preferences-service;1"].
         getService(Ci.nsIPrefBranch);

Cu.import("resource://gre/modules/BookmarkHTMLUtils.jsm");

const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
const POST_DATA_ANNO = "bookmarkProperties/POSTData";

const TEST_FAVICON_PAGE_URL = "http://en-US.www.mozilla.com/en-US/firefox/central/";
const TEST_FAVICON_DATA_SIZE = 580;

function run_test() {
  run_next_test();
}

add_task(function test_corrupt_file() {
  // avoid creating the places smart folder during tests
  ps.setIntPref("browser.places.smartBookmarksVersion", -1);

  // Import bookmarks from the corrupt file.
  yield BookmarkHTMLUtils.importFromFile(OS.Path.join(do_get_cwd().path, "bookmarks.corrupt.html"),
                                         true);

  // Check that bookmarks that are not corrupt have been imported.
  yield database_check();
});

add_task(function test_corrupt_database() {
  // Create corruption in the database, then export.
  var corruptItemId = bs.insertBookmark(bs.toolbarFolder,
                                        uri("http://test.mozilla.org"),
                                        bs.DEFAULT_INDEX, "We love belugas");
  var stmt = dbConn.createStatement("UPDATE moz_bookmarks SET fk = NULL WHERE id = :itemId");
  stmt.params.itemId = corruptItemId;
  stmt.execute();
  stmt.finalize();

  let bookmarksFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.exported.html");
  if ((yield OS.File.exists(bookmarksFile)))
    yield OS.File.remove(bookmarksFile);
  yield BookmarkHTMLUtils.exportToFile(bookmarksFile);

  // Import again and check for correctness.
  remove_all_bookmarks();
  yield BookmarkHTMLUtils.importFromFile(bookmarksFile, true);
  yield database_check();
});

/*
 * Check for imported bookmarks correctness
 *
 * @return {Promise}
 * @resolves When the checks are finished.
 * @rejects Never.
 */
function database_check() {
  return Task.spawn(function() {
    // BOOKMARKS MENU
    var query = hs.getNewQuery();
    query.setFolders([bs.bookmarksMenuFolder], 1);
    var options = hs.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var result = hs.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    do_check_eq(rootNode.childCount, 2);

    // get test folder
    var testFolder = rootNode.getChild(1);
    do_check_eq(testFolder.type, testFolder.RESULT_TYPE_FOLDER);
    do_check_eq(testFolder.title, "test");
    // add date
    do_check_eq(bs.getItemDateAdded(testFolder.itemId)/1000000, 1177541020);
    // last modified
    do_check_eq(bs.getItemLastModified(testFolder.itemId)/1000000, 1177541050);
    testFolder = testFolder.QueryInterface(Ci.nsINavHistoryQueryResultNode);
    do_check_eq(testFolder.hasChildren, true);
    // folder description
    do_check_true(as.itemHasAnnotation(testFolder.itemId,
                                       DESCRIPTION_ANNO));
    do_check_eq("folder test comment",
                as.getItemAnnotation(testFolder.itemId, DESCRIPTION_ANNO));
    // open test folder, and test the children
    testFolder.containerOpen = true;
    var cc = testFolder.childCount;
    do_check_eq(cc, 1);

    // test bookmark 1
    var testBookmark1 = testFolder.getChild(0);
    // url
    do_check_eq("http://test/post", testBookmark1.uri);
    // title
    do_check_eq("test post keyword", testBookmark1.title);
    // keyword
    do_check_eq("test", bs.getKeywordForBookmark(testBookmark1.itemId));
    // sidebar
    do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                       LOAD_IN_SIDEBAR_ANNO));
    // add date
    do_check_eq(testBookmark1.dateAdded/1000000, 1177375336);
    // last modified
    do_check_eq(testBookmark1.lastModified/1000000, 1177375423);
    // post data
    do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                       POST_DATA_ANNO));
    do_check_eq("hidden1%3Dbar&text1%3D%25s",
                as.getItemAnnotation(testBookmark1.itemId, POST_DATA_ANNO));
    // last charset
    var testURI = uri(testBookmark1.uri);
    do_check_eq((yield PlacesUtils.getCharsetForURI(testURI)), "ISO-8859-1");

    // description
    do_check_true(as.itemHasAnnotation(testBookmark1.itemId,
                                       DESCRIPTION_ANNO));
    do_check_eq("item description",
                as.getItemAnnotation(testBookmark1.itemId,
                                     DESCRIPTION_ANNO));

    // clean up
    testFolder.containerOpen = false;
    rootNode.containerOpen = false;

    // BOOKMARKS TOOLBAR
    query.setFolders([bs.toolbarFolder], 1);
    result = hs.executeQuery(query, hs.getNewQueryOptions());
    var toolbar = result.root;
    toolbar.containerOpen = true;
    do_check_eq(toolbar.childCount, 3);

    // livemark
    var livemark = toolbar.getChild(1);
    // title
    do_check_eq("Latest Headlines", livemark.title);

    let foundLivemark = yield PlacesUtils.livemarks.getLivemark({ id: livemark.itemId });
    do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
                foundLivemark.siteURI.spec);
    do_check_eq("http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
                foundLivemark.feedURI.spec);

    // cleanup
    toolbar.containerOpen = false;

    // UNFILED BOOKMARKS
    query.setFolders([bs.unfiledBookmarksFolder], 1);
    result = hs.executeQuery(query, hs.getNewQueryOptions());
    var unfiledBookmarks = result.root;
    unfiledBookmarks.containerOpen = true;
    do_check_eq(unfiledBookmarks.childCount, 1);
    unfiledBookmarks.containerOpen = false;

    // favicons
    let deferGetFaviconData = Promise.defer();
    icos.getFaviconDataForPage(uri(TEST_FAVICON_PAGE_URL),
      function DC_onComplete(aURI, aDataLen, aData, aMimeType) {
        // aURI should never be null when aDataLen > 0.
        do_check_neq(aURI, null);
        // Favicon data is stored in the bookmarks file as a "data:" URI.  For
        // simplicity, instead of converting the data we receive to a "data:" URI
        // and comparing it, we just check the data size.
        do_check_eq(TEST_FAVICON_DATA_SIZE, aDataLen);
        deferGetFaviconData.resolve();
      }
    );
    yield deferGetFaviconData.promise;
  });
}
