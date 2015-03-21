function run_test() {
  var testURI = uri("http://foo.com");

  /*
  1. Create a bookmark for a URI, with a keyword and post data.
  2. Create a bookmark for the same URI, with a different keyword and different post data.
  3. Confirm that our method for getting a URI+postdata retains bookmark affinity.
  */
  var bm1 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId, testURI, -1, "blah");
  PlacesUtils.bookmarks.setKeywordForBookmark(bm1, "foo");
  PlacesUtils.setPostDataForBookmark(bm1, "pdata1");
  var bm2 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId, testURI, -1, "blah");
  PlacesUtils.bookmarks.setKeywordForBookmark(bm2, "bar");
  PlacesUtils.setPostDataForBookmark(bm2, "pdata2");

  // check kw, pd for bookmark 1
  var url, postdata;
  [url, postdata] = PlacesUtils.getURLAndPostDataForKeyword("foo");
  do_check_eq(testURI.spec, url);
  do_check_eq(postdata, "pdata1");

  // check kw, pd for bookmark 2
  [url, postdata] = PlacesUtils.getURLAndPostDataForKeyword("bar");
  do_check_eq(testURI.spec, url);
  do_check_eq(postdata, "pdata2");

  // cleanup
  PlacesUtils.bookmarks.removeItem(bm1);
  PlacesUtils.bookmarks.removeItem(bm2);
}
