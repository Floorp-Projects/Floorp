/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Test that inserting a new bookmark will set lastModified to the same
  * values as dateAdded.
  */
// main
function run_test() {
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  var itemId = bs.insertBookmark(bs.bookmarksMenuFolder,
                                 uri("http://www.mozilla.org/"),
                                 bs.DEFAULT_INDEX,
                                 "itemTitle");
  var dateAdded = bs.getItemDateAdded(itemId);
  do_check_eq(dateAdded, bs.getItemLastModified(itemId));

  // Change lastModified, then change dateAdded.  LastModified should be set
  // to the new dateAdded.
  // This could randomly fail on virtual machines due to timing issues, so
  // we manually increase the time value.  See bug 500640 for details.
  bs.setItemLastModified(itemId, dateAdded + 1000);
  do_check_true(bs.getItemLastModified(itemId) === dateAdded + 1000);
  do_check_true(bs.getItemDateAdded(itemId) < bs.getItemLastModified(itemId));
  bs.setItemDateAdded(itemId, dateAdded + 2000);
  do_check_true(bs.getItemDateAdded(itemId) === dateAdded + 2000);
  do_check_eq(bs.getItemDateAdded(itemId), bs.getItemLastModified(itemId));

  bs.removeItem(itemId);
}
