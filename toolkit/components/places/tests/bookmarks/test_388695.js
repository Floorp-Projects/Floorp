/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch (ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

var gTestRoot;
var gURI;
var gItemId1;
var gItemId2;

// main
function run_test() {
  gURI = uri("http://foo.tld.com/");
  gTestRoot = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                 bmsvc.DEFAULT_INDEX);

  // test getBookmarkIdsForURI
  // getBookmarkIdsForURI sorts by the most recently added/modified (descending)
  //
  // we cannot rely on dateAdded growing when doing so in a simple iteration,
  // see PR_Now() documentation
  do_test_pending();

  gItemId1 = bmsvc.insertBookmark(gTestRoot, gURI, bmsvc.DEFAULT_INDEX, "");
  do_timeout(100, phase2);
}

function phase2() {
  gItemId2 = bmsvc.insertBookmark(gTestRoot, gURI, bmsvc.DEFAULT_INDEX, "");
  var b = bmsvc.getBookmarkIdsForURI(gURI);
  do_check_eq(b[0], gItemId2);
  do_check_eq(b[1], gItemId1);
  do_timeout(100, phase3);
}

function phase3() {
  // trigger last modified change
  bmsvc.setItemTitle(gItemId1, "");
  var b = bmsvc.getBookmarkIdsForURI(gURI);
  do_check_eq(b[0], gItemId1);
  do_check_eq(b[1], gItemId2);
  do_test_finished();
}
