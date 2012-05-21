/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test ensures that when removing a folder within a transaction, undoing
 * the transaction restores it with the same id (as received by the observers).
 */

var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

function run_test()
{
  const TITLE = "test folder";

  // Create two test folders; remove the first one.  This ensures that undoing
  // the removal will not get the same id by chance (the insert id's can be
  // reused in SQLite).
  let id = bs.createFolder(bs.placesRoot, TITLE, -1);
  bs.createFolder(bs.placesRoot, "test folder 2", -1);
  let transaction = bs.getRemoveFolderTransaction(id);
  transaction.doTransaction();

  // Now check to make sure it gets added with the right id
  bs.addObserver({
    onItemAdded: function(aItemId, aFolder, aIndex, aItemType, aURI, aTitle)
    {
      do_check_eq(aItemId, id);
      do_check_eq(aTitle, TITLE);
    }
  }, false);
  transaction.undoTransaction();
}
