/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_eraseEverything() {
  info("Test folder with eraseEverything");
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "remove-folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [
          { url: "http://mozilla.org/", title: "title 1" },
          { url: "http://mozilla.org/", title: "title 2" },
          { title: "sub-folder", type: PlacesUtils.bookmarks.TYPE_FOLDER },
          { type: PlacesUtils.bookmarks.TYPE_SEPARATOR },
        ],
      },
    ],
  });

  let unfiled = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid)
    .root;
  Assert.equal(unfiled.childCount, 1, "There should be 1 folder");
  let folder = unfiled.getChild(0);
  // Test dateAdded and lastModified properties.
  Assert.equal(typeof folder.dateAdded, "number");
  Assert.ok(folder.dateAdded > 0);
  Assert.equal(typeof folder.lastModified, "number");
  Assert.ok(folder.lastModified > 0);

  let root = PlacesUtils.getFolderContents(folder.bookmarkGuid).root;
  Assert.equal(root.childCount, 4, "The folder should have 4 children");
  for (let i = 0; i < root.childCount; ++i) {
    let node = root.getChild(i);
    Assert.greater(node.itemId, 0, "The node should have an itemId");
  }
  Assert.equal(root.getChild(0).title, "title 1");
  Assert.equal(root.getChild(1).title, "title 2");
  await PlacesUtils.bookmarks.eraseEverything();
  Assert.equal(unfiled.childCount, 0);
  unfiled.containerOpen = false;
});

add_task(async function test_search_title() {
  let title = "ZZZXXXYYY";
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://mozilla.org/",
    title,
  });
  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "ZZZXXXYYY";
  let options = PlacesUtils.history.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  let node = root.getChild(0);
  Assert.equal(node.title, title);

  // Test dateAdded and lastModified properties.
  Assert.equal(typeof node.dateAdded, "number");
  Assert.ok(node.dateAdded > 0);
  Assert.equal(typeof node.lastModified, "number");
  Assert.ok(node.lastModified > 0);
  Assert.equal(node.bookmarkGuid, bm.guid);

  await PlacesUtils.bookmarks.remove(bm);
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;
});

add_task(async function test_long_title() {
  let title = Array(TITLE_LENGTH_MAX + 5).join("A");
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://mozilla.org/",
    title,
  });
  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid)
    .root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  let node = root.getChild(0);
  Assert.equal(node.title, title.substr(0, TITLE_LENGTH_MAX));

  // Update with another long title.
  let newTitle = Array(TITLE_LENGTH_MAX + 5).join("B");
  bm.title = newTitle;
  await PlacesUtils.bookmarks.update(bm);
  Assert.equal(node.title, newTitle.substr(0, TITLE_LENGTH_MAX));

  await PlacesUtils.bookmarks.remove(bm);
  Assert.equal(root.childCount, 0);
  root.containerOpen = false;
});
