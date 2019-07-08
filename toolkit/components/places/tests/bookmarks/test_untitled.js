add_task(async function test_untitled_visited_bookmark() {
  let fxURI = uri("http://getfirefox.com");

  await PlacesUtils.history.insert({
    url: fxURI,
    title: "Get Firefox!",
    visits: [
      {
        date: new Date(),
        transition: PlacesUtils.history.TRANSITIONS.TYPED,
      },
    ],
  });

  let { root: node } = PlacesUtils.getFolderContents(
    PlacesUtils.bookmarks.toolbarGuid
  );

  try {
    let fxBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: fxURI,
    });
    strictEqual(fxBmk.title, "", "Visited bookmark should not have title");

    await PlacesTestUtils.promiseAsyncUpdates();

    let fxBmkId = await PlacesUtils.promiseItemId(fxBmk.guid);
    strictEqual(
      PlacesUtils.bookmarks.getItemTitle(fxBmkId),
      "",
      "Should return empty string for untitled visited bookmark"
    );

    let fxBmkNode = node.getChild(0);
    equal(fxBmkNode.itemId, fxBmkId, "Visited bookmark ID should match");
    strictEqual(
      fxBmkNode.title,
      "",
      "Visited bookmark node should not have title"
    );
  } finally {
    node.containerOpen = false;
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_untitled_unvisited_bookmark() {
  let tbURI = uri("http://getthunderbird.com");

  let { root: node } = PlacesUtils.getFolderContents(
    PlacesUtils.bookmarks.toolbarGuid
  );

  try {
    let tbBmk = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      url: tbURI,
    });
    strictEqual(tbBmk.title, "", "Unvisited bookmark should not have title");

    await PlacesTestUtils.promiseAsyncUpdates();

    let tbBmkId = await PlacesUtils.promiseItemId(tbBmk.guid);
    strictEqual(
      PlacesUtils.bookmarks.getItemTitle(tbBmkId),
      "",
      "Should return empty string for untitled unvisited bookmark"
    );

    let tbBmkNode = node.getChild(0);
    equal(tbBmkNode.itemId, tbBmkId, "Unvisited bookmark ID should match");
    strictEqual(
      tbBmkNode.title,
      "",
      "Unvisited bookmark node should not have title"
    );
  } finally {
    node.containerOpen = false;
  }

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_untitled_folder() {
  let { root: node } = PlacesUtils.getFolderContents(
    PlacesUtils.bookmarks.toolbarGuid
  );

  try {
    let folder = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
    });

    await PlacesTestUtils.promiseAsyncUpdates();

    let folderId = await PlacesUtils.promiseItemId(folder.guid);
    strictEqual(
      PlacesUtils.bookmarks.getItemTitle(folderId),
      "",
      "Should return empty string for untitled folder"
    );

    let folderNode = node.getChild(0);
    equal(folderNode.itemId, folderId, "Folder ID should match");
    strictEqual(folderNode.title, "", "Folder node should not have title");
  } finally {
    node.containerOpen = false;
  }

  await PlacesUtils.bookmarks.eraseEverything();
});
