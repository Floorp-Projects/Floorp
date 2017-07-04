/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var gAccumulator = {
  get callback() {
    this.results = [];
    return result => this.results.push(result);
  }
};

add_task(async function invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.fetch(),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch(null),
                /Input should be a valid object/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: "123456789012",
                                                    parentGuid: "012345678901" }),
                /The following properties were expected: index/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: "123456789012",
                                                    index: 0 }),
                /The following properties were expected: parentGuid/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({}),
                /Unexpected number of conditions provided: 0/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK }),
                /Unexpected number of conditions provided: 0/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: "123456789012",
                                                    parentGuid: "012345678901",
                                                    index: 0 }),
                /Unexpected number of conditions provided: 2/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: "123456789012",
                                                    url: "http://example.com"}),
                /Unexpected number of conditions provided: 2/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch("test"),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch(123),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: "test" }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: null }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ guid: 123 }),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: "test",
                                                    index: 0 }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: null,
                                                    index: 0 }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: 123,
                                                    index: 0 }),
                /Invalid value for property 'parentGuid'/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: "123456789012",
                                                    index: "0" }),
                /Invalid value for property 'index'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: "123456789012",
                                                    index: null }),
                /Invalid value for property 'index'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ parentGuid: "123456789012",
                                                    index: -10 }),
                /Invalid value for property 'index'/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch({ url: "http://te st/" }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ url: null }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch({ url: -10 }),
                /Invalid value for property 'url'/);

  Assert.throws(() => PlacesUtils.bookmarks.fetch("123456789012", "test"),
                /onResult callback must be a valid function/);
  Assert.throws(() => PlacesUtils.bookmarks.fetch("123456789012", {}),
                /onResult callback must be a valid function/);
});

add_task(async function fetch_nonexistent_guid() {
  let bm = await PlacesUtils.bookmarks.fetch({ guid: "123456789012" },
                                               gAccumulator.callback);
  Assert.equal(bm, null);
  Assert.equal(gAccumulator.results.length, 0);
});

add_task(async function fetch_bookmark() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch(bm1.guid,
                                              gAccumulator.callback);
  checkBookmarkObject(bm2);
  Assert.equal(gAccumulator.results.length, 1);
  checkBookmarkObject(gAccumulator.results[0]);
  Assert.deepEqual(gAccumulator.results[0], bm1);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://example.com/");
  Assert.equal(bm2.title, "a bookmark");

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_bookmar_empty_title() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.index, 0);
  Assert.strictEqual(bm2.title, "");

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_folder() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "a folder" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_FOLDER);
  Assert.equal(bm2.title, "a folder");
  Assert.ok(!("url" in bm2));

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_folder_empty_title() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                 title: "" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.index, 0);
  Assert.strictEqual(bm2.title, "");

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_separator() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch(bm1.guid);
  checkBookmarkObject(bm2);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_SEPARATOR);
  Assert.ok(!("url" in bm2));
  Assert.strictEqual(bm2.title, "");

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_byposition_nonexisting_parentGuid() {
  let bm = await PlacesUtils.bookmarks.fetch({ parentGuid: "123456789012",
                                               index: 0 },
                                             gAccumulator.callback);
  Assert.equal(bm, null);
  Assert.equal(gAccumulator.results.length, 0);
});

add_task(async function fetch_byposition_nonexisting_index() {
  let bm = await PlacesUtils.bookmarks.fetch({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                               index: 100 },
                                             gAccumulator.callback);
  Assert.equal(bm, null);
  Assert.equal(gAccumulator.results.length, 0);
});

add_task(async function fetch_byposition() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch({ parentGuid: bm1.parentGuid,
                                                index: bm1.index },
                                              gAccumulator.callback);
  checkBookmarkObject(bm2);
  Assert.equal(gAccumulator.results.length, 1);
  checkBookmarkObject(gAccumulator.results[0]);
  Assert.deepEqual(gAccumulator.results[0], bm1);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 0);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://example.com/");
  Assert.equal(bm2.title, "a bookmark");
});

add_task(async function fetch_byposition_default_index() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://example.com/last",
                                                 title: "last child" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch({ parentGuid: bm1.parentGuid,
                                                index: PlacesUtils.bookmarks.DEFAULT_INDEX },
                                              gAccumulator.callback);
  checkBookmarkObject(bm2);
  Assert.equal(gAccumulator.results.length, 1);
  checkBookmarkObject(gAccumulator.results[0]);
  Assert.deepEqual(gAccumulator.results[0], bm1);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.equal(bm2.index, 1);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://example.com/last");
  Assert.equal(bm2.title, "last child");

  await PlacesUtils.bookmarks.remove(bm1.guid);
});

add_task(async function fetch_byurl_nonexisting() {
  let bm = await PlacesUtils.bookmarks.fetch({ url: "http://nonexisting.com/" },
                                             gAccumulator.callback);
  Assert.equal(bm, null);
  Assert.equal(gAccumulator.results.length, 0);
});

add_task(async function fetch_byurl() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://byurl.com/",
                                                 title: "a bookmark" });
  checkBookmarkObject(bm1);

  // Also ensure that fecth-by-url excludes the tags folder.
  PlacesUtils.tagging.tagURI(uri(bm1.url.href), ["Test Tag"]);

  let bm2 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback);
  checkBookmarkObject(bm2);
  Assert.equal(gAccumulator.results.length, 1);
  checkBookmarkObject(gAccumulator.results[0]);
  Assert.deepEqual(gAccumulator.results[0], bm1);

  Assert.deepEqual(bm1, bm2);
  Assert.equal(bm2.parentGuid, PlacesUtils.bookmarks.unfiledGuid);
  Assert.deepEqual(bm2.dateAdded, bm2.lastModified);
  Assert.equal(bm2.type, PlacesUtils.bookmarks.TYPE_BOOKMARK);
  Assert.equal(bm2.url.href, "http://byurl.com/");
  Assert.equal(bm2.title, "a bookmark");

  let bm3 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                 url: "http://byurl.com/",
                                                 title: "a bookmark" });
  let bm4 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback);
  checkBookmarkObject(bm4);
  Assert.deepEqual(bm3, bm4);
  Assert.equal(gAccumulator.results.length, 2);
  gAccumulator.results.forEach(checkBookmarkObject);
  Assert.deepEqual(gAccumulator.results[0], bm4);

  // After an update the returned bookmark should change.
  await PlacesUtils.bookmarks.update({ guid: bm1.guid, title: "new title" });
  let bm5 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback);
  checkBookmarkObject(bm5);
  // Cannot use deepEqual cause lastModified changed.
  Assert.equal(bm1.guid, bm5.guid);
  Assert.ok(bm5.lastModified > bm1.lastModified);
  Assert.equal(gAccumulator.results.length, 2);
  gAccumulator.results.forEach(checkBookmarkObject);
  Assert.deepEqual(gAccumulator.results[0], bm5);

  // cleanup
  PlacesUtils.tagging.untagURI(uri(bm1.url.href), ["Test Tag"]);
});

add_task(async function fetch_concurrent() {
  let bm1 = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                 url: "http://concurrent.url.com/" });
  checkBookmarkObject(bm1);

  let bm2 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback,
                                              { concurrent: true });
  checkBookmarkObject(bm2);
  Assert.equal(gAccumulator.results.length, 1);
  Assert.deepEqual(gAccumulator.results[0], bm1);
  Assert.deepEqual(bm1, bm2);
  let bm3 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback,
                                              { concurrent: false });
  checkBookmarkObject(bm3);
  Assert.equal(gAccumulator.results.length, 1);
  Assert.deepEqual(gAccumulator.results[0], bm1);
  Assert.deepEqual(bm1, bm3);
  let bm4 = await PlacesUtils.bookmarks.fetch({ url: bm1.url },
                                              gAccumulator.callback,
                                              {});
  checkBookmarkObject(bm4);
  Assert.equal(gAccumulator.results.length, 1);
  Assert.deepEqual(gAccumulator.results[0], bm1);
  Assert.deepEqual(bm1, bm4);
});
