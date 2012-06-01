/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const bmsvc = PlacesUtils.bookmarks;
const testFolderId = PlacesUtils.bookmarksMenuFolderId;

// main
function run_test() {
  var testURI = uri("http://foo.com");

  /*
  1. Create a bookmark for a URI, with a keyword and post data.
  2. Create a bookmark for the same URI, with a different keyword and different post data.
  3. Confirm that our method for getting a URI+postdata retains bookmark affinity.
  */
  var bm1 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm1, "foo");
  PlacesUtils.setPostDataForBookmark(bm1, "pdata1");
  var bm2 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm2, "bar");
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
  bmsvc.removeItem(bm1);
  bmsvc.removeItem(bm2);

  /*
  1. Create two bookmarks with the same URI and keyword.
  2. Confirm that the most recently created one is returned for that keyword.
  */
  var bm1 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm1, "foo");
  PlacesUtils.setPostDataForBookmark(bm1, "pdata1");
  var bm2 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm2, "foo");
  PlacesUtils.setPostDataForBookmark(bm2, "pdata2");

  var bm1da = bmsvc.getItemDateAdded(bm1);
  var bm1lm = bmsvc.getItemLastModified(bm1);
  LOG("bm1 dateAdded: " + bm1da + ", lastModified: " + bm1lm);
  var bm2da = bmsvc.getItemDateAdded(bm2);
  var bm2lm = bmsvc.getItemLastModified(bm2);
  LOG("bm2 dateAdded: " + bm2da + ", lastModified: " + bm2lm);
  do_check_true(bm1da <= bm2da);
  do_check_true(bm1lm <= bm2lm);

  [url, postdata] = PlacesUtils.getURLAndPostDataForKeyword("foo");
  do_check_eq(testURI.spec, url);
  do_check_eq(postdata, "pdata2");

  // cleanup
  bmsvc.removeItem(bm1);
  bmsvc.removeItem(bm2);

  /*
  1. Create two bookmarks with the same URI and keyword.
  2. Modify the first-created bookmark.
  3. Confirm that the most recently modified one is returned for that keyword.
  */
  var bm1 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm1, "foo");
  PlacesUtils.setPostDataForBookmark(bm1, "pdata1");
  var bm2 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm2, "foo");
  PlacesUtils.setPostDataForBookmark(bm2, "pdata2");

  // modify the older bookmark
  bmsvc.setItemTitle(bm1, "change");

  var bm1da = bmsvc.getItemDateAdded(bm1);
  var bm1lm = bmsvc.getItemLastModified(bm1);
  LOG("bm1 dateAdded: " + bm1da + ", lastModified: " + bm1lm);
  var bm2da = bmsvc.getItemDateAdded(bm2);
  var bm2lm = bmsvc.getItemLastModified(bm2);
  LOG("bm2 dateAdded: " + bm2da + ", lastModified: " + bm2lm);
  do_check_true(bm1da <= bm2da);
  // the last modified for bm1 should be at least as big as bm2
  // but could be equal if the test runs faster than our PRNow()
  // granularity
  do_check_true(bm1lm >= bm2lm);

  // we need to ensure that bm1 last modified date is greater
  // that the modified date of bm2, otherwise in case of a "tie"
  // bm2 will win, as it has a bigger item id
  if (bm1lm == bm2lm) 
    bmsvc.setItemLastModified(bm1, bm2lm + 1);

  [url, postdata] = PlacesUtils.getURLAndPostDataForKeyword("foo");
  do_check_eq(testURI.spec, url);
  do_check_eq(postdata, "pdata1");

  // cleanup
  bmsvc.removeItem(bm1);
  bmsvc.removeItem(bm2);

  /*
  Test that id breaks ties:
  1. Create two bookmarks with the same URI and keyword, dateAdded and lastModified.
  2. Confirm that the most recently created one is returned for that keyword.
  */
  var testDate = Date.now() * 1000;
  var bm1 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm1, "foo");
  PlacesUtils.setPostDataForBookmark(bm1, "pdata1");
  bmsvc.setItemDateAdded(bm1, testDate);
  bmsvc.setItemLastModified(bm1, testDate);

  var bm2 = bmsvc.insertBookmark(testFolderId, testURI, -1, "blah");
  bmsvc.setKeywordForBookmark(bm2, "foo");
  PlacesUtils.setPostDataForBookmark(bm2, "pdata2");
  bmsvc.setItemDateAdded(bm2, testDate);
  bmsvc.setItemLastModified(bm2, testDate);

  var bm1da = bmsvc.getItemDateAdded(bm1, testDate);
  var bm1lm = bmsvc.getItemLastModified(bm1);
  LOG("bm1 dateAdded: " + bm1da + ", lastModified: " + bm1lm);
  var bm2da = bmsvc.getItemDateAdded(bm2);
  var bm2lm = bmsvc.getItemLastModified(bm2);
  LOG("bm2 dateAdded: " + bm2da + ", lastModified: " + bm2lm);

  do_check_eq(bm1da, bm2da);
  do_check_eq(bm1lm, bm2lm);


  var ids = bmsvc.getBookmarkIdsForURI(testURI);
  do_check_eq(ids[0], bm2);
  do_check_eq(ids[1], bm1);

  [url, postdata] = PlacesUtils.getURLAndPostDataForKeyword("foo");
  do_check_eq(testURI.spec, url);
  do_check_eq(postdata, "pdata2");

  // cleanup
  bmsvc.removeItem(bm1);
  bmsvc.removeItem(bm2);
}
