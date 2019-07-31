/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function invalid_input_throws() {
  Assert.throws(
    () => PlacesUtils.bookmarks.insert(),
    /Input should be a valid object/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert(null),
    /Input should be a valid object/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({}),
    /The following properties were expected/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ guid: "test" }),
    /Invalid value for property 'guid'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ guid: null }),
    /Invalid value for property 'guid'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ guid: 123 }),
    /Invalid value for property 'guid'/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ parentGuid: "test" }),
    /Invalid value for property 'parentGuid'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ parentGuid: null }),
    /Invalid value for property 'parentGuid'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ parentGuid: 123 }),
    /Invalid value for property 'parentGuid'/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ index: "1" }),
    /Invalid value for property 'index'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ index: -10 }),
    /Invalid value for property 'index'/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ dateAdded: -10 }),
    /Invalid value for property 'dateAdded'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ dateAdded: "today" }),
    /Invalid value for property 'dateAdded'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ dateAdded: Date.now() }),
    /Invalid value for property 'dateAdded'/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ lastModified: -10 }),
    /Invalid value for property 'lastModified'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ lastModified: "today" }),
    /Invalid value for property 'lastModified'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ lastModified: Date.now() }),
    /Invalid value for property 'lastModified'/
  );
  let time = new Date();

  let past = new Date(time - 86400000);
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ lastModified: past }),
    /Invalid value for property 'lastModified'/
  );

  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ type: -1 }),
    /Invalid value for property 'type'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ type: 100 }),
    /Invalid value for property 'type'/
  );
  Assert.throws(
    () => PlacesUtils.bookmarks.insert({ type: "bookmark" }),
    /Invalid value for property 'type'/
  );

  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        title: -1,
      }),
    /Invalid value for property 'title'/
  );

  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: 10,
      }),
    /Invalid value for property 'url'/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: "http://te st",
      }),
    /Invalid value for property 'url'/
  );
  let longurl = "http://www.example.com/";
  for (let i = 0; i < 65536; i++) {
    longurl += "a";
  }
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: longurl,
      }),
    /Invalid value for property 'url'/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: NetUtil.newURI(longurl),
      }),
    /Invalid value for property 'url'/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url: "te st",
      }),
    /Invalid value for property 'url'/
  );
});

add_task(async function invalid_properties_for_bookmark_type() {
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        url: "http://www.moz.com/",
      }),
    /Invalid value for property 'url'/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        url: "http://www.moz.com/",
      }),
    /Invalid value for property 'url'/
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
        title: "test",
      }),
    /Invalid value for property 'title'/
  );
});

add_task(async function test_insert_into_root_throws() {
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        url: "http://example.com",
      }),
    /Invalid value for property 'parentGuid'/,
    "Should throw when inserting a bookmark into the root."
  );
  Assert.throws(
    () =>
      PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.rootGuid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
      }),
    /Invalid value for property 'parentGuid'/,
    "Should throw when inserting a folder into the root."
  );
});

add_task(async function long_title_trim() {
  let longtitle = "a";
  for (let i = 0; i < 4096; i++) {
    longtitle += "a";
  }
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: longtitle,
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 0);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.equal(bm.title.length, 4096, "title should have been trimmed");
  Assert.ok(!("url" in bm), "url should not be set");
});

add_task(async function create_separator() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 1);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.strictEqual(bm.title, "");
});

add_task(async function create_separator_w_title_fail() {
  try {
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      title: "a separator",
    });
    Assert.ok(false, "Trying to set title for a separator should reject");
  } catch (ex) {}
});

add_task(async function create_separator_invalid_parent_fail() {
  try {
    await PlacesUtils.bookmarks.insert({
      parentGuid: "123456789012",
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      title: "a separator",
    });
    Assert.ok(
      false,
      "Trying to create an item in a non existing parent reject"
    );
  } catch (ex) {}
});

add_task(async function create_separator_given_guid() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    guid: "123456789012",
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.guid, "123456789012");
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 2);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.strictEqual(bm.title, "");
});

add_task(async function create_item_given_guid_no_type_fail() {
  try {
    await PlacesUtils.bookmarks.insert({ parentGuid: "123456789012" });
    Assert.ok(
      false,
      "Trying to create an item with a given guid but no type should reject"
    );
  } catch (ex) {}
});

add_task(async function create_separator_big_index() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    index: 9999,
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.index, 3);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.strictEqual(bm.title, "");
});

add_task(async function create_separator_given_dateAdded() {
  let time = new Date();
  let past = new Date(time - 86400000);
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    dateAdded: past,
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.dateAdded, past);
  Assert.equal(bm.lastModified, past);
});

add_task(async function create_folder() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.dateAdded, bm.lastModified);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.strictEqual(bm.title, "");

  // And then create a nested folder.
  let parentGuid = bm.guid;
  bm = await PlacesUtils.bookmarks.insert({
    parentGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "a folder",
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, parentGuid);
  Assert.equal(bm.index, 0);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.strictEqual(bm.title, "a folder");
});

add_task(async function create_bookmark() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });
  let parentGuid = bm.guid;

  bm = await PlacesUtils.bookmarks.insert({
    parentGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://example.com/",
    title: "a bookmark",
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, parentGuid);
  Assert.equal(bm.index, 0);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm.url.href, "http://example.com/");
  Assert.equal(bm.title, "a bookmark");

  // Check parent lastModified.
  let parent = await PlacesUtils.bookmarks.fetch({ guid: bm.parentGuid });
  Assert.deepEqual(parent.lastModified, bm.dateAdded);

  bm = await PlacesUtils.bookmarks.insert({
    parentGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: new URL("http://example.com/"),
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, parentGuid);
  Assert.equal(bm.index, 1);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm.url.href, "http://example.com/");
  Assert.strictEqual(bm.title, "");
});

add_task(async function create_bookmark_frecency() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
    url: "http://example.com/",
    title: "a bookmark",
  });
  checkBookmarkObject(bm);

  await PlacesTestUtils.promiseAsyncUpdates();
  Assert.ok(frecencyForUrl(bm.url) > 0, "Check frecency has been updated");
});

add_task(async function create_bookmark_without_type() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://example.com/",
    title: "a bookmark",
  });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm.url.href, "http://example.com/");
  Assert.equal(bm.title, "a bookmark");
});

add_task(async function test_url_with_apices() {
  // Apices may confuse code and cause injection if mishandled.
  const url = `javascript:alert("%s");alert('%s');`;
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  // Just a sanity check, this should not throw.
  await PlacesUtils.history.remove(url);
  let bm = await PlacesUtils.bookmarks.fetch({ url });
  await PlacesUtils.bookmarks.remove(bm);
});
