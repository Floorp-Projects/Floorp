/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get bookmark service
var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
            getService(Ci.nsINavBookmarksService);

/**
 * Ensures that the Places APIs recognize that aBookmarkedUri is bookmarked
 * via aBookmarkId and that aUnbookmarkedUri is not bookmarked at all.
 *
 * @param aBookmarkId
 *        an item ID whose corresponding URI is aBookmarkedUri
 * @param aBookmarkedUri
 *        a bookmarked URI that has a corresponding item ID aBookmarkId
 * @param aUnbookmarkedUri
 *        a URI that is not currently bookmarked at all
 */
function checkUris(aBookmarkId, aBookmarkedUri, aUnbookmarkedUri) {
  // Ensure that aBookmarkedUri equals some URI that is bookmarked
  var uri = bmsvc.getBookmarkedURIFor(aBookmarkedUri);
  do_check_neq(uri, null);
  do_check_true(uri.equals(aBookmarkedUri));

  // Ensure that aBookmarkedUri is considered bookmarked
  do_check_true(bmsvc.isBookmarked(aBookmarkedUri));

  // Ensure that the URI corresponding to aBookmarkId equals aBookmarkedUri
  do_check_true(bmsvc.getBookmarkURI(aBookmarkId).equals(aBookmarkedUri));

  // Ensure that aUnbookmarkedUri does not equal any URI that is bookmarked
  uri = bmsvc.getBookmarkedURIFor(aUnbookmarkedUri);
  do_check_eq(uri, null);

  // Ensure that aUnbookmarkedUri is not considered bookmarked
  do_check_false(bmsvc.isBookmarked(aUnbookmarkedUri));
}

// main
function run_test() {
  // Create a folder
  var folderId = bmsvc.createFolder(bmsvc.toolbarFolder,
                                    "test",
                                    bmsvc.DEFAULT_INDEX);

  // Create 2 URIs
  var uri1 = uri("http://www.dogs.com");
  var uri2 = uri("http://www.cats.com");

  // Bookmark the first one
  var bookmarkId = bmsvc.insertBookmark(folderId,
                                        uri1,
                                        bmsvc.DEFAULT_INDEX,
                                        "Dogs");

  // uri1 is bookmarked via bookmarkId, uri2 is not
  checkUris(bookmarkId, uri1, uri2);

  // Change the URI of the bookmark to uri2
  bmsvc.changeBookmarkURI(bookmarkId, uri2);

  // uri2 is now bookmarked via bookmarkId, uri1 is not
  checkUris(bookmarkId, uri2, uri1);
}
