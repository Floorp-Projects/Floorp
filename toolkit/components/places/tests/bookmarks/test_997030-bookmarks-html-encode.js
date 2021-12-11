/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Checks that we don't encodeURI twice when creating bookmarks.html.
 */
add_task(async function() {
  let url =
    "http://bt.ktxp.com/search.php?keyword=%E5%A6%84%E6%83%B3%E5%AD%A6%E7%94%9F%E4%BC%9A";
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmark",
    url,
  });

  let file = OS.Path.join(
    OS.Constants.Path.profileDir,
    "bookmarks.exported.997030.html"
  );
  await IOUtils.remove(file, { ignoreAbsent: true });
  await BookmarkHTMLUtils.exportToFile(file);

  // Remove the bookmarks, then restore the backup.
  await PlacesUtils.bookmarks.remove(bm);
  await BookmarkHTMLUtils.importFromFile(file, { replace: true });

  info("Checking first level");
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid)
    .root;
  let node = root.getChild(0);
  Assert.equal(node.uri, url);

  root.containerOpen = false;
  await PlacesUtils.bookmarks.eraseEverything();
});
