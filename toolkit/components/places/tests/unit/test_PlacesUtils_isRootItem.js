"use strict";

const GUIDS = [
  PlacesUtils.bookmarks.rootGuid,
  ...PlacesUtils.bookmarks.userContentRoots,
  PlacesUtils.bookmarks.tagsGuid,
];

add_task(async function test_isRootItem() {
  for (let guid of GUIDS) {
    Assert.ok(PlacesUtils.isRootItem(guid), `Should correctly identify root item ${guid}`);
  }

  Assert.ok(!PlacesUtils.isRootItem("fakeguid1234"), "Should not identify other items as root.");
});
