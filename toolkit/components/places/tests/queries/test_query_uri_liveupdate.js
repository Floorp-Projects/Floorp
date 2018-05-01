/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_results_as_tag_query() {
  let bms = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      { url: "http://tag1.moz.com/", tags: ["tag1"] },
      { url: "http://tag2.moz.com/", tags: ["tag2"] },
      { url: "place:tag=tag1" },
    ]
  });

  let root = PlacesUtils.getFolderContents(PlacesUtils.bookmarks.unfiledGuid, false, true).root;
  Assert.equal(root.childCount, 3, "We should get 3 results");
  let queryRoot = root.getChild(2);
  PlacesUtils.asContainer(queryRoot).containerOpen = true;

  Assert.equal(queryRoot.uri, "place:tag=tag1", "Found the query");
  Assert.equal(queryRoot.childCount, 1, "We should get 1 result");
  Assert.equal(queryRoot.getChild(0).uri, "http://tag1.moz.com/", "Found the tagged bookmark");

  await PlacesUtils.bookmarks.update({ guid: bms[2].guid, url: "place:tag=tag2" });
  Assert.equal(queryRoot.uri, "place:tag=tag2", "Found the query");
  Assert.equal(queryRoot.childCount, 1, "We should get 1 result");
  Assert.equal(queryRoot.getChild(0).uri, "http://tag2.moz.com/", "Found the tagged bookmark");

  queryRoot.containerOpen = false;
  root.containerOpen = false;
});
