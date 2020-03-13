/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_fetchTags() {
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
  Assert.deepEqual(tags, [{ name: "2", count: 1 }]);

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

add_task(async function test_fetch_by_tags() {
  Assert.throws(
    () => PlacesUtils.bookmarks.fetch({ tags: "" }),
    /Invalid value for property 'tags'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.fetch({ tags: [] }),
    /Invalid value for property 'tags'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.fetch({ tags: null }),
    /Invalid value for property 'tags'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.fetch({ tags: [""] }),
    /Invalid value for property 'tags'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.fetch({ tags: ["valid", null] }),
    /Invalid value for property 'tags'/
  );

  info("Add bookmarks with tags.");
  let bm1 = await PlacesUtils.bookmarks.insert({
    url: "http://bacon.org/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  PlacesUtils.tagging.tagURI(Services.io.newURI(bm1.url.href), [
    "egg",
    "ratafià",
  ]);
  let bm2 = await PlacesUtils.bookmarks.insert({
    url: "http://mushroom.org/",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  PlacesUtils.tagging.tagURI(Services.io.newURI(bm2.url.href), ["egg"]);

  info("Fetch a single tag.");
  let bms = [];
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch({ tags: ["egg"] }, b => bms.push(b)))
      .guid,
    bm2.guid,
    "Found the expected recent bookmark"
  );
  Assert.deepEqual(
    bms.map(b => b.guid),
    [bm2.guid, bm1.guid],
    "Found the expected bookmarks"
  );

  info("Fetch multiple tags.");
  bms = [];
  Assert.equal(
    (
      await PlacesUtils.bookmarks.fetch({ tags: ["egg", "ratafià"] }, b =>
        bms.push(b)
      )
    ).guid,
    bm1.guid,
    "Found the expected recent bookmark"
  );
  Assert.deepEqual(
    bms.map(b => b.guid),
    [bm1.guid],
    "Found the expected bookmarks"
  );

  info("Fetch a nonexisting tag.");
  bms = [];
  Assert.equal(
    await PlacesUtils.bookmarks.fetch({ tags: ["egg", "tomato"] }, b =>
      bms.push(b)
    ),
    null,
    "Should not find any bookmark"
  );
  Assert.deepEqual(bms, [], "Should not find any bookmark");

  info("Check case insensitive");
  bms = [];
  Assert.equal(
    (
      await PlacesUtils.bookmarks.fetch({ tags: ["eGg", "raTafiÀ"] }, b =>
        bms.push(b)
      )
    ).guid,
    bm1.guid,
    "Found the expected recent bookmark"
  );
  Assert.deepEqual(
    bms.map(b => b.guid),
    [bm1.guid],
    "Found the expected bookmarks"
  );
});
