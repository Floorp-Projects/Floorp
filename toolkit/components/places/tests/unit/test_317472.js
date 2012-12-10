/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const charset = "UTF-8";
const CHARSET_ANNO = "URIProperties/characterSet";

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var annosvc = Cc["@mozilla.org/browser/annotation-service;1"].
                getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get services\n");
}

const TEST_URI = uri("http://foo.com");
const TEST_BOOKMARKED_URI = uri("http://bar.com");

function run_test()
{
  run_next_test();
}

add_task(function test_execute()
{
  // add pages to history
  yield promiseAddVisits(TEST_URI);
  yield promiseAddVisits(TEST_BOOKMARKED_URI);

  // create bookmarks on TEST_BOOKMARKED_URI
  var bm1 = bmsvc.insertBookmark(bmsvc.unfiledBookmarksFolder,
                                 TEST_BOOKMARKED_URI, bmsvc.DEFAULT_INDEX,
                                 TEST_BOOKMARKED_URI.spec);
  var bm2 = bmsvc.insertBookmark(bmsvc.toolbarFolder,
                                 TEST_BOOKMARKED_URI, bmsvc.DEFAULT_INDEX,
                                 TEST_BOOKMARKED_URI.spec);

  // set charset on not-bookmarked page
  histsvc.setCharsetForURI(TEST_URI, charset);
  // set charset on bookmarked page
  histsvc.setCharsetForURI(TEST_BOOKMARKED_URI, charset);

  // check that we have created a page annotation
  do_check_eq(annosvc.getPageAnnotation(TEST_URI, CHARSET_ANNO), charset);

  // get charset from not-bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_URI), charset);
  // get charset from bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);

  yield promiseClearHistory();

  // ensure that charset has gone for not-bookmarked page
  do_check_neq(histsvc.getCharsetForURI(TEST_URI), charset);

  // check that page annotation has been removed
  try {
    annosvc.getPageAnnotation(TEST_URI, CHARSET_ANNO);
    do_throw("Charset page annotation has not been removed correctly");
  } catch (e) {}

  // ensure that charset still exists for bookmarked page
  do_check_eq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);

  // remove charset from bookmark and check that has gone
  histsvc.setCharsetForURI(TEST_BOOKMARKED_URI, "");
  do_check_neq(histsvc.getCharsetForURI(TEST_BOOKMARKED_URI), charset);
});
