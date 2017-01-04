/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests a crash during startup found in bug 476292 that was caused by
 * getting the bookmarks service during nsNavHistory::Init when the bookmarks
 * service was created before the history service was.
 */

function run_test() {
  // First, we need to move our old database file into our test profile
  // directory.  This will trigger DATABASE_STATUS_UPGRADED (CREATE is not
  // sufficient since there will be no entries to update frecencies for, which
  // causes us to get the bookmarks service in the first place).
  let dbFile = do_get_file("bug476292.sqlite");
  let profD = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties).
             get(NS_APP_USER_PROFILE_50_DIR, Ci.nsIFile);
  dbFile.copyTo(profD, "places.sqlite");

  // Now get the bookmarks service.  This will crash when the bug exists.
  Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
    getService(Ci.nsINavBookmarksService);
}
