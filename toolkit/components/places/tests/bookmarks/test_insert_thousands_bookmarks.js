/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test `insertTree()` with more bookmarks than the Sqlite variables limit.

add_task(async function () {
  const NUM_BOOKMARKS = 1000;
  await PlacesUtils.withConnectionWrapper("test", async db => {
    db.variableLimit = NUM_BOOKMARKS - 100;
    Assert.greater(
      NUM_BOOKMARKS,
      db.variableLimit,
      "Insert more bookmarks than the Sqlite variables limit."
    );
  });
  let children = [];
  for (let i = 0; i < NUM_BOOKMARKS; ++i) {
    children.push({ url: "http://www.mozilla.org/" + i });
  }
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children,
  });
});
