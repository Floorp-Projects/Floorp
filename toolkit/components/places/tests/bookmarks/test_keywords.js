/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function check_bookmark_keyword(aItemId, aKeyword)
{
  let keyword = aKeyword ? aKeyword.toLowerCase() : null;
  do_check_eq(PlacesUtils.bookmarks.getKeywordForBookmark(aItemId),
              keyword);
}

function check_uri_keyword(aURI, aKeyword)
{
  let keyword = aKeyword ? aKeyword.toLowerCase() : null;
  do_check_eq(PlacesUtils.bookmarks.getKeywordForURI(aURI),
              keyword);

  if (aKeyword) {
    // This API can't tell which uri the user wants, so it returns a random one.
    let re = /http:\/\/test[0-9]\.mozilla\.org/;
    let url = PlacesUtils.bookmarks.getURIForKeyword(aKeyword).spec;
    do_check_true(re.test(url));
    // Check case insensitivity.
    url = PlacesUtils.bookmarks.getURIForKeyword(aKeyword.toUpperCase()).spec
    do_check_true(re.test(url));
  }
}

function check_orphans()
{
  stmt = DBConn().createStatement(
    "SELECT id FROM moz_keywords k WHERE NOT EXISTS ("
  +    "SELECT id FROM moz_bookmarks WHERE keyword_id = k.id "
  + ")"
  );
  try {
    do_check_false(stmt.executeStep());
  } finally {
    stmt.finalize();
  }

  print("Check there are no orphan database entries");
  let stmt = DBConn().createStatement(
    "SELECT b.id FROM moz_bookmarks b "
  + "LEFT JOIN moz_keywords k ON b.keyword_id = k.id "
  + "WHERE keyword_id NOTNULL AND k.id ISNULL"
  );
  try {
    do_check_false(stmt.executeStep());
  } finally {
    stmt.finalize();
  }
}

const URIS = [
  uri("http://test1.mozilla.org/"),
  uri("http://test2.mozilla.org/"),
];

add_test(function test_addBookmarkWithKeyword()
{
  check_uri_keyword(URIS[0], null);

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URIS[0],
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  check_bookmark_keyword(itemId, "keyword");
  check_uri_keyword(URIS[0], "keyword");

  promiseAsyncUpdates().then(function() {
    check_orphans();
    run_next_test();
  });
});

add_test(function test_addBookmarkToURIHavingKeyword()
{
  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URIS[0],
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  // The uri has a keyword, but this specific bookmark has not.
  check_bookmark_keyword(itemId, null);
  check_uri_keyword(URIS[0], "keyword");

  promiseAsyncUpdates().then(function() {
    check_orphans();
    run_next_test();
  });
});

add_test(function test_addSameKeywordToOtherURI()
{
  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URIS[1],
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test2");
  check_bookmark_keyword(itemId, null);
  check_uri_keyword(URIS[1], null);

  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "kEyWoRd");
  check_bookmark_keyword(itemId, "kEyWoRd");
  check_uri_keyword(URIS[1], "kEyWoRd");

  // Check case insensitivity.
  check_uri_keyword(URIS[0], "kEyWoRd");
  check_bookmark_keyword(itemId, "keyword");
  check_uri_keyword(URIS[1], "keyword");
  check_uri_keyword(URIS[0], "keyword");

  promiseAsyncUpdates().then(function() {
    check_orphans();
    run_next_test();
  });
});

add_test(function test_removeBookmarkWithKeyword()
{
  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         URIS[1],
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");
  check_bookmark_keyword(itemId, "keyword");
  check_uri_keyword(URIS[1], "keyword");

  // The keyword should not be removed from other bookmarks.
  PlacesUtils.bookmarks.removeItem(itemId);

  check_uri_keyword(URIS[1], "keyword");
  check_uri_keyword(URIS[0], "keyword");

  promiseAsyncUpdates().then(function() {
    check_orphans();
    run_next_test();
  });
});

add_test(function test_removeFolderWithKeywordedBookmarks()
{
  // Keyword should be removed as well.
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);

  check_uri_keyword(URIS[1], null);
  check_uri_keyword(URIS[0], null);

  promiseAsyncUpdates().then(function() {
    check_orphans();
    run_next_test();
  });
});

function run_test()
{
  run_next_test();
}
