/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

- add a folder
- add a folder-shortcut to the new folder
- query for the shortcut
- remove the folder-shortcut
- confirm the shortcut is removed from the query results

*/

function run_test() {
    const IDX = PlacesUtils.bookmarks.DEFAULT_INDEX;
    var folderId =
      PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId, "", IDX);

    var queryId =
      PlacesUtils.bookmarks.insertBookmark(PlacesUtils.toolbarFolderId,
                                           uri("place:folder=" + folderId), IDX, "");

    var root = PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId, false, true).root;

    var oldCount = root.childCount;

    PlacesUtils.bookmarks.removeItem(queryId);

    do_check_eq(root.childCount, oldCount-1);

    root.containerOpen = false;
}
