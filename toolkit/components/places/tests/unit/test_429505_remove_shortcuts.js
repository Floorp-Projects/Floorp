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

add_task(async function test_query_with_remove_shortcut() {
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "",
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  let folderId = await PlacesUtils.promiseItemId(folder.guid);

  let query = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "",
    url: `place:folder=${folderId}`,
  });

  var root = PlacesUtils.getFolderContents(PlacesUtils.toolbarFolderId, false, true).root;

  var oldCount = root.childCount;

  await PlacesUtils.bookmarks.remove(query.guid);

  do_check_eq(root.childCount, oldCount - 1);

  root.containerOpen = false;

  await PlacesTestUtils.promiseAsyncUpdates();
});
