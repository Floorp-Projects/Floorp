/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that backups properly include all of the bookmarks if the hierarchy
 * in the database is unordered so that a hierarchy is defined before its
 * ancestor in the bookmarks table.
 */
add_task(async function() {
  let bms = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "bookmark",
        url: "http://mozilla.org",
      },
      {
        title: "f2",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
      {
        title: "f1",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      },
    ],
  });

  let bookmark = bms[0];
  let folder2 = bms[1];
  let folder1 = bms[2];
  bookmark.parentGuid = folder2.guid;
  await PlacesUtils.bookmarks.update(bookmark);

  folder2.parentGuid = folder1.guid;
  await PlacesUtils.bookmarks.update(folder2);

  // Create a backup.
  await PlacesBackups.create();

  // Remove the bookmarks, then restore the backup.
  await PlacesUtils.bookmarks.remove(folder1);
  await BookmarkJSONUtils.importFromFile(
    await PlacesBackups.getMostRecentBackup(),
    { replace: true }
  );

  info("Checking first level");
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid)
    .root;
  let level1 = root.getChild(0);
  Assert.equal(level1.title, "f1");
  info("Checking second level");
  PlacesUtils.asContainer(level1).containerOpen = true;
  let level2 = level1.getChild(0);
  Assert.equal(level2.title, "f2");
  info("Checking bookmark");
  PlacesUtils.asContainer(level2).containerOpen = true;
  bookmark = level2.getChild(0);
  Assert.equal(bookmark.title, "bookmark");
  level2.containerOpen = false;
  level1.containerOpen = false;
  root.containerOpen = false;
});
