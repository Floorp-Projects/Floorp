/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* invalid_input_throws() {
  Assert.throws(() => PlacesUtils.bookmarks.update(),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.bookmarks.update(null),
                /Input should be a valid object/);
  Assert.throws(() => PlacesUtils.bookmarks.update({}),
                /The following properties were expected/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "test" }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: null }),
                /Invalid value for property 'guid'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: 123 }),
                /Invalid value for property 'guid'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ parentGuid: "test" }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ parentGuid: null }),
                /Invalid value for property 'parentGuid'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ parentGuid: 123 }),
                /Invalid value for property 'parentGuid'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ index: "1" }),
                /Invalid value for property 'index'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ index: -10 }),
                /Invalid value for property 'index'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ dateAdded: -10 }),
                /Invalid value for property 'dateAdded'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ dateAdded: "today" }),
                /Invalid value for property 'dateAdded'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ dateAdded: Date.now() }),
                /Invalid value for property 'dateAdded'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ lastModified: -10 }),
                /Invalid value for property 'lastModified'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ lastModified: "today" }),
                /Invalid value for property 'lastModified'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ lastModified: Date.now() }),
                /Invalid value for property 'lastModified'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ type: -1 }),
                /Invalid value for property 'type'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ type: 100 }),
                /Invalid value for property 'type'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ type: "bookmark" }),
                /Invalid value for property 'type'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ url: 10 }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ url: "http://te st" }),
                /Invalid value for property 'url'/);
  let longurl = "http://www.example.com/";
  for (let i = 0; i < 65536; i++) {
    longurl += "a";
  }
  Assert.throws(() => PlacesUtils.bookmarks.update({ url: longurl }),
                /Invalid value for property 'url'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ url: NetUtil.newURI(longurl) }),
                /Invalid value for property 'url'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ url: "te st" }),
                /Invalid value for property 'url'/);

  Assert.throws(() => PlacesUtils.bookmarks.insert({ title: -1 }),
                /Invalid value for property 'title'/);
  Assert.throws(() => PlacesUtils.bookmarks.insert({ title: undefined }),
                /Invalid value for property 'title'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ keyword: 10 }),
                /Invalid value for property 'keyword'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ keyword: null }),
                /Invalid value for property 'keyword'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "123456789012" }),
                /Not enough properties to update/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "123456789012",
                                                     parentGuid: "012345678901" }),
                /The following properties were expected: index/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "123456789012",
                                                     index: 1 }),
                /The following properties were expected: parentGuid/);
});

add_task(function* nonexisting_bookmark_throws() {
  try {
    yield PlacesUtils.bookmarks.update({ guid: "123456789012",
                                         title: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided GUID/.test(ex));
  }
});

add_task(function* invalid_properties_for_existing_bookmark() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: unfiledGuid,
                                                url: "http://example.com/" });

  try {
    yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         type: PlacesUtils.bookmarks.TYPE_FOLDER });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/The bookmark type cannot be changed/.test(ex));
  }

  try {
    yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         dateAdded: new Date() });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/The bookmark dateAdded cannot be changed/.test(ex));
  }

  try {
    yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         dateAdded: new Date() });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/The bookmark dateAdded cannot be changed/.test(ex));
  }

  try {
    yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         parentGuid: "123456789012",
                                         index: 1 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided parentGuid/.test(ex));
  }

  let past = new Date(Date.now() - 86400000);
  try {
    yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         lastModified: past });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'lastModified'/.test(ex));
  }

  let folder = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                    parentGuid: unfiledGuid });
  try {
    yield PlacesUtils.bookmarks.update({ guid: folder.guid,
                                         url: "http://example.com/" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'url'/.test(ex));
  }
  try {
    yield PlacesUtils.bookmarks.update({ guid: folder.guid,
                                         keyword: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'keyword'/.test(ex));
  }

  let separator = yield PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                                       parentGuid: unfiledGuid });
  try {
    yield PlacesUtils.bookmarks.update({ guid: separator.guid,
                                         url: "http://example.com/" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'url'/.test(ex));
  }
  try {
    yield PlacesUtils.bookmarks.update({ guid: separator.guid,
                                         keyword: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'keyword'/.test(ex));
  }
  try {
    yield PlacesUtils.bookmarks.update({ guid: separator.guid,
                                         title: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'title'/.test(ex));
  }
});

add_task(function* long_title_trim() {
  let longtitle = "a";
  for (let i = 0; i < 4096; i++) {
    longtitle += "a";
  }
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                title: "title" });
  checkBookmarkObject(bm);

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: longtitle });
  let newTitle = bm.title;
  Assert.equal(newTitle.length, 4096, "title should have been trimmed");

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.title, newTitle);
});

add_task(function* update_lastModified() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let yesterday = new Date(Date.now() - 86400000);
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                title: "title",
                                                dateAdded: yesterday });
  checkBookmarkObject(bm);
  Assert.deepEqual(bm.lastModified, yesterday);

  let time = new Date();
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: time });
  checkBookmarkObject(bm);
  Assert.deepEqual(bm.lastModified, time);

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.deepEqual(bm.lastModified, time);

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: yesterday });
  Assert.deepEqual(bm.lastModified, yesterday);

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "title2" });
  Assert.ok(bm.lastModified >= time);

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "" });
  Assert.ok(!("title" in bm));

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.ok(!("title" in bm));
});

add_task(function* update_keyword() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                url: "http://example.com/",
                                                title: "title",
                                                keyword: "kw" });
  checkBookmarkObject(bm);
  let lastModified = bm.lastModified;

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            keyword: "kw2" });
  checkBookmarkObject(bm);
  Assert.ok(bm.lastModified >= lastModified);
  Assert.equal(bm.keyword, "kw2");

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.keyword, "kw2");
  lastModified = bm.lastModified;

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            keyword: "" });
  Assert.ok(!("keyword" in bm));

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.ok(!("keyword" in bm));
  Assert.ok(bm.lastModified >= lastModified);

  // Check orphan keyword has been removed from the database.
  let conn = yield PlacesUtils.promiseDBConnection();
  let rows = yield conn.executeCached(
    `SELECT id from moz_keywords WHERE keyword >= :keyword`, { keyword: "kw" });
  Assert.equal(rows.length, 0);
});

add_task(function* update_url() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                url: "http://example.com/",
                                                title: "title",
                                                keyword: "kw" });
  checkBookmarkObject(bm);
  let lastModified = bm.lastModified;
  let frecency = frecencyForUrl(bm.url);
  Assert.ok(frecency > 0, "Check frecency has been updated");

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            url: "http://mozilla.org/" });
  checkBookmarkObject(bm);
  Assert.ok(bm.lastModified >= lastModified);
  Assert.equal(bm.url.href, "http://mozilla.org/");

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.url.href, "http://mozilla.org/");
  Assert.ok(bm.lastModified >= lastModified);

  Assert.equal(frecencyForUrl("http://example.com/"), frecency, "Check frecency for example.com");
  Assert.equal(frecencyForUrl("http://mozilla.org/"), frecency, "Check frecency for mozilla.org");
});

add_task(function* update_index() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let parent = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let f1 = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f1.index, 0);
  let f2 = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f2.index, 1);
  let f3 = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f3.index, 2);
  let lastModified = f1.lastModified;

  f1 = yield PlacesUtils.bookmarks.update({ guid: f1.guid,
                                            parentGuid: f1.parentGuid,
                                            index: 1});
  checkBookmarkObject(f1);
  Assert.equal(f1.index, 1);
  Assert.ok(f1.lastModified >= lastModified);

  parent = yield PlacesUtils.bookmarks.fetch(f1.parentGuid);
  Assert.deepEqual(parent.lastModified, f1.lastModified);

  f2 = yield PlacesUtils.bookmarks.fetch(f2.guid);
  Assert.equal(f2.index, 0);

  f3 = yield PlacesUtils.bookmarks.fetch(f3.guid);
  Assert.equal(f3.index, 2);

  f3 = yield PlacesUtils.bookmarks.update({ guid: f3.guid,
                                            parentGuid: f1.parentGuid,
                                            index: 0 });
  f1 = yield PlacesUtils.bookmarks.fetch(f1.guid);
  Assert.equal(f1.index, 2);

  f2 = yield PlacesUtils.bookmarks.fetch(f2.guid);
  Assert.equal(f2.index, 1);
});

add_task(function* update_move_folder_into_descendant_throws() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let parent = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let descendant = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });

  try {
    yield PlacesUtils.bookmarks.update({ guid: parent.guid,
                                         parentGuid: parent.guid,
                                         index: 0 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Cannot insert a folder into itself or one of its descendants/.test(ex));
  }

  try {
    yield PlacesUtils.bookmarks.update({ guid: parent.guid,
                                         parentGuid: descendant.guid,
                                         index: 0 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Cannot insert a folder into itself or one of its descendants/.test(ex));
  }
});

add_task(function* update_move() {
  let unfiledGuid = yield PlacesUtils.promiseItemGuid(PlacesUtils.unfiledBookmarksFolderId);
  let parent = yield PlacesUtils.bookmarks.insert({ parentGuid: unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                url: "http://example.com/",
                                                type: PlacesUtils.bookmarks.TYPE_BOOKMARK }) ;
  let descendant = yield PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(descendant.index, 1);
  let lastModified = bm.lastModified;

  // This is moving to a nonexisting index by purpose, it will be appended.
  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: descendant.guid,
                                            index: 1 });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);
  Assert.ok(bm.lastModified >= lastModified);

  parent = yield PlacesUtils.bookmarks.fetch(parent.guid);
  descendant = yield PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.deepEqual(parent.lastModified, bm.lastModified);
  Assert.deepEqual(descendant.lastModified, bm.lastModified);
  Assert.equal(descendant.index, 0);

  bm = yield PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);

  bm = yield PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: parent.guid,
                                            index: 0 });
  Assert.equal(bm.parentGuid, parent.guid);
  Assert.equal(bm.index, 0);

  descendant = yield PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.equal(descendant.index, 1);
});

function run_test() {
  run_next_test();
}
