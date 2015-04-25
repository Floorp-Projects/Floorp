/*
 * This test ensures that importing/exporting to HTML does not stop
 * if a malformed uri is found.
 */

const DESCRIPTION_ANNO = "bookmarkProperties/description";
const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";

const TEST_FAVICON_PAGE_URL = "http://en-US.www.mozilla.com/en-US/firefox/central/";
const TEST_FAVICON_DATA_SIZE = 580;

function run_test() {
  run_next_test();
}

add_task(function* test_corrupt_file() {
  // avoid creating the places smart folder during tests
  Services.prefs.setIntPref("browser.places.smartBookmarksVersion", -1);

  // Import bookmarks from the corrupt file.
  let corruptHtml = OS.Path.join(do_get_cwd().path, "bookmarks.corrupt.html");
  yield BookmarkHTMLUtils.importFromFile(corruptHtml, true);

  // Check that bookmarks that are not corrupt have been imported.
  yield PlacesTestUtils.promiseAsyncUpdates();
  yield database_check();
});

add_task(function* test_corrupt_database() {
  // Create corruption in the database, then export.
  let corruptBookmark = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.toolbarGuid,
                                                             url: "http://test.mozilla.org",
                                                             title: "We love belugas" });
  let db = yield PlacesUtils.promiseWrappedConnection();
  yield db.execute("UPDATE moz_bookmarks SET fk = NULL WHERE guid = :guid",
                   { guid: corruptBookmark.guid });

  let bookmarksFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.exported.html");
  if ((yield OS.File.exists(bookmarksFile)))
    yield OS.File.remove(bookmarksFile);
  yield BookmarkHTMLUtils.exportToFile(bookmarksFile);

  // Import again and check for correctness.
  yield PlacesUtils.bookmarks.eraseEverything();
  yield BookmarkHTMLUtils.importFromFile(bookmarksFile, true);
  yield PlacesTestUtils.promiseAsyncUpdates();
  yield database_check();
});

/*
 * Check for imported bookmarks correctness
 *
 * @return {Promise}
 * @resolves When the checks are finished.
 * @rejects Never.
 */
let database_check = Task.async(function* () {
  // BOOKMARKS MENU
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarksMenuFolderId).root;
  Assert.equal(root.childCount, 2);

  let folderNode = root.getChild(1);
  Assert.equal(folderNode.type, folderNode.RESULT_TYPE_FOLDER);
  Assert.equal(folderNode.title, "test");
  Assert.equal(PlacesUtils.bookmarks.getItemDateAdded(folderNode.itemId), 1177541020000000);
  Assert.equal(PlacesUtils.bookmarks.getItemLastModified(folderNode.itemId), 1177541050000000);
  Assert.equal("folder test comment",
               PlacesUtils.annotations.getItemAnnotation(folderNode.itemId,
                                                         DESCRIPTION_ANNO));
  // open test folder, and test the children
  PlacesUtils.asQuery(folderNode);
  Assert.equal(folderNode.hasChildren, true);
  folderNode.containerOpen = true;
  Assert.equal(folderNode.childCount, 1);

  let bookmarkNode = folderNode.getChild(0);
  Assert.equal("http://test/post", bookmarkNode.uri);
  Assert.equal("test post keyword", bookmarkNode.title);

  let entry = yield PlacesUtils.keywords.fetch({ url: bookmarkNode.uri });
  Assert.equal("test", entry.keyword);
  Assert.equal("hidden1%3Dbar&text1%3D%25s", entry.postData);

  Assert.ok(PlacesUtils.annotations.itemHasAnnotation(bookmarkNode.itemId,
                                                      LOAD_IN_SIDEBAR_ANNO));
  Assert.equal(bookmarkNode.dateAdded, 1177375336000000);
  Assert.equal(bookmarkNode.lastModified, 1177375423000000);

  Assert.equal((yield PlacesUtils.getCharsetForURI(NetUtil.newURI(bookmarkNode.uri))),
               "ISO-8859-1");

  Assert.equal("item description",
               PlacesUtils.annotations.getItemAnnotation(bookmarkNode.itemId,
                                                         DESCRIPTION_ANNO));

  // clean up
  folderNode.containerOpen = false;
  root.containerOpen = false;

  // BOOKMARKS TOOLBAR
  root = PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId).root;
  Assert.equal(root.childCount, 3);

  // For now some promises are resolved later, so we can't guarantee an order.
  let foundLivemark = false;
  for (let i = 0; i < root.childCount; ++i) {
    let node = root.getChild(i);
    if (node.title == "Latest Headlines") {
      foundLivemark = true;
      Assert.equal("Latest Headlines", node.title);

      let livemark = yield PlacesUtils.livemarks.getLivemark({ guid: node.bookmarkGuid });
      Assert.equal("http://en-us.fxfeeds.mozilla.com/en-US/firefox/livebookmarks/",
                   livemark.siteURI.spec);
      Assert.equal("http://en-us.fxfeeds.mozilla.com/en-US/firefox/headlines.xml",
                   livemark.feedURI.spec);
    }
  }
  Assert.ok(foundLivemark);

  // cleanup
  root.containerOpen = false;

  // UNFILED BOOKMARKS
  root = PlacesUtils.getFolderContents(PlacesUtils.unfiledBookmarksFolderId).root;
  Assert.equal(root.childCount, 1);
  root.containerOpen = false;

  // favicons
  yield new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(uri(TEST_FAVICON_PAGE_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        // aURI should never be null when aDataLen > 0.
        Assert.notEqual(aURI, null);
        // Favicon data is stored in the bookmarks file as a "data:" URI.  For
        // simplicity, instead of converting the data we receive to a "data:" URI
        // and comparing it, we just check the data size.
        Assert.equal(TEST_FAVICON_DATA_SIZE, aDataLen);
        resolve();
      });
  });
});
