/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let tags = await PlacesUtils.bookmarks.fetchTags();
  Assert.deepEqual(tags, []);

  let bm = await PlacesUtils.bookmarks.insert({
    url: "http://page1.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  PlacesUtils.tagging.tagURI(Services.io.newURI(bm.url.href), ["1", "2"]);
  tags = await PlacesUtils.bookmarks.fetchTags();
  Assert.deepEqual(tags, [
    { name: "1", count: 1 },
    { name: "2", count: 1 },
  ]);

  PlacesUtils.tagging.untagURI(Services.io.newURI(bm.url.href), ["1"]);
  tags = await PlacesUtils.bookmarks.fetchTags();
  Assert.deepEqual(tags, [
    { name: "2", count: 1 },
  ]);

  let bm2 = await PlacesUtils.bookmarks.insert({
    url: "http://page2.com/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  PlacesUtils.tagging.tagURI(Services.io.newURI(bm2.url.href), ["2", "3"]);
  tags = await PlacesUtils.bookmarks.fetchTags();
  Assert.deepEqual(tags, [
    { name: "2", count: 2 },
    { name: "3", count: 1 },
  ]);
});
