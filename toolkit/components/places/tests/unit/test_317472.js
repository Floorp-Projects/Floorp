/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const charset = "UTF-8";
const CHARSET_ANNO = "URIProperties/characterSet";

const TEST_URI = uri("http://foo.com");
const TEST_BOOKMARKED_URI = uri("http://bar.com");

function run_test()
{
  run_next_test();
}

add_task(function* test_execute()
{
  // add pages to history
  yield PlacesTestUtils.addVisits(TEST_URI);
  yield PlacesTestUtils.addVisits(TEST_BOOKMARKED_URI);

  // create bookmarks on TEST_BOOKMARKED_URI
  var bm1 = PlacesUtils.bookmarks.insertBookmark(
              PlacesUtils.unfiledBookmarksFolderId,
              TEST_BOOKMARKED_URI, PlacesUtils.bookmarks.DEFAULT_INDEX,
              TEST_BOOKMARKED_URI.spec);
  var bm2 = PlacesUtils.bookmarks.insertBookmark(
              PlacesUtils.toolbarFolderId,
              TEST_BOOKMARKED_URI, PlacesUtils.bookmarks.DEFAULT_INDEX,
              TEST_BOOKMARKED_URI.spec);

  // set charset on not-bookmarked page
  yield PlacesUtils.setCharsetForURI(TEST_URI, charset);
  // set charset on bookmarked page
  yield PlacesUtils.setCharsetForURI(TEST_BOOKMARKED_URI, charset);

  // check that we have created a page annotation
  do_check_eq(PlacesUtils.annotations.getPageAnnotation(TEST_URI, CHARSET_ANNO), charset);

  // get charset from not-bookmarked page
  do_check_eq((yield PlacesUtils.getCharsetForURI(TEST_URI)), charset);

  // get charset from bookmarked page
  do_check_eq((yield PlacesUtils.getCharsetForURI(TEST_BOOKMARKED_URI)), charset);

  yield PlacesTestUtils.clearHistory();

  // ensure that charset has gone for not-bookmarked page
  do_check_neq((yield PlacesUtils.getCharsetForURI(TEST_URI)), charset);

  // check that page annotation has been removed
  try {
    PlacesUtils.annotations.getPageAnnotation(TEST_URI, CHARSET_ANNO);
    do_throw("Charset page annotation has not been removed correctly");
  } catch (e) {}

  // ensure that charset still exists for bookmarked page
  do_check_eq((yield PlacesUtils.getCharsetForURI(TEST_BOOKMARKED_URI)), charset);

  // remove charset from bookmark and check that has gone
  yield PlacesUtils.setCharsetForURI(TEST_BOOKMARKED_URI, "");
  do_check_neq((yield PlacesUtils.getCharsetForURI(TEST_BOOKMARKED_URI)), charset);
});
