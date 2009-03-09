/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dietrich Ayala <dietrich@mozilla.com> (Original Author)
 *  Drew Willcoxon <adw@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
function checkUris(aBookmarkId, aBookmarkedUri, aUnbookmarkedUri)
{
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
