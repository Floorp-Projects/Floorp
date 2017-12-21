/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Both setItemTitle and insertBookmark should default to the empty string
 * for null titles.
 */

const bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);

const TEST_URL = "http://www.mozilla.org";

function run_test() {
  // Insert a bookmark with an empty title.
  var itemId = bs.insertBookmark(bs.toolbarFolder,
                                 uri(TEST_URL),
                                 bs.DEFAULT_INDEX,
                                 "");
  // Check returned title is an empty string.
  Assert.equal(bs.getItemTitle(itemId), "");
  // Set title to null.
  bs.setItemTitle(itemId, null);
  // Check returned title defaults to an empty string.
  Assert.equal(bs.getItemTitle(itemId), "");
  // Cleanup.
  bs.removeItem(itemId);

  // Insert a bookmark with a null title.
  itemId = bs.insertBookmark(bs.toolbarFolder,
                             uri(TEST_URL),
                             bs.DEFAULT_INDEX,
                             null);
  // Check returned title defaults to an empty string.
  Assert.equal(bs.getItemTitle(itemId), "");
  // Set title to an empty string.
  bs.setItemTitle(itemId, "");
  // Check returned title is an empty string.
  Assert.equal(bs.getItemTitle(itemId), "");
  // Cleanup.
  bs.removeItem(itemId);
}
