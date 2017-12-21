/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 expandtab
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This test ensures that when removing a folder within a transaction, undoing
 * the transaction restores it with the same id (as received by the observers).
 */

add_task(async function test_undo_folder_remove_within_transaction() {
  const TITLE = "test folder";

  // Create two test folders; remove the first one.  This ensures that undoing
  // the removal will not get the same id by chance (the insert id's can be
  // reused in SQLite).
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: TITLE,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let id = await PlacesUtils.promiseItemId(folder.guid);

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test folder 2",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let transaction = PlacesUtils.bookmarks.getRemoveFolderTransaction(id);
  transaction.doTransaction();

  // Now check to make sure it gets added with the right id
  PlacesUtils.bookmarks.addObserver({
    onItemAdded(aItemId, aFolder, aIndex, aItemType, aURI, aTitle) {
      Assert.equal(aItemId, id);
      Assert.equal(aTitle, TITLE);
    }
  });
  transaction.undoTransaction();
});
