/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FOLDER_TITLE = '"quoted folder"';

function checkQuotedFolder() {
  let toolbar = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.toolbarFolder).root;

  // test for our quoted folder
  Assert.equal(toolbar.childCount, 1);
  var folderNode = toolbar.getChild(0);
  Assert.equal(folderNode.type, folderNode.RESULT_TYPE_FOLDER);
  Assert.equal(folderNode.title, FOLDER_TITLE);

  // clean up
  toolbar.containerOpen = false;
}

add_task(async function() {
  // make json file
  let jsonFile = OS.Path.join(OS.Constants.Path.profileDir, "bookmarks.json");

  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: FOLDER_TITLE,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });

  checkQuotedFolder();

  // export json to file
  await BookmarkJSONUtils.exportToFile(jsonFile);

  await PlacesUtils.bookmarks.remove(folder.guid);

  // restore json file
  await BookmarkJSONUtils.importFromFile(jsonFile, { replace: true });

  checkQuotedFolder();

  // clean up
  await OS.File.remove(jsonFile);

});
