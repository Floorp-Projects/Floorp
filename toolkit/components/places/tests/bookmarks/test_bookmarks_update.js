/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function invalid_input_throws() {
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

  Assert.throws(() => PlacesUtils.bookmarks.update({ title: -1 }),
                /Invalid value for property 'title'/);
  Assert.throws(() => PlacesUtils.bookmarks.update({ title: {} }),
                /Invalid value for property 'title'/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "123456789012" }),
                /Not enough properties to update/);

  Assert.throws(() => PlacesUtils.bookmarks.update({ guid: "123456789012",
                                                     parentGuid: "012345678901" }),
                /The following properties were expected: index/);
});

add_task(async function move_roots_fail() {
  let guids = [PlacesUtils.bookmarks.unfiledGuid,
               PlacesUtils.bookmarks.menuGuid,
               PlacesUtils.bookmarks.toolbarGuid,
               PlacesUtils.bookmarks.tagsGuid,
               PlacesUtils.bookmarks.mobileGuid];
  for (let guid of guids) {
    Assert.rejects(PlacesUtils.bookmarks.update({
      guid,
      index: -1,
      parentGuid: PlacesUtils.bookmarks.rootGuid,
    }), /It's not possible to move Places root folders\./,
    `Should reject when attempting to move ${guid}`);
  }
});

add_task(async function nonexisting_bookmark_throws() {
  try {
    await PlacesUtils.bookmarks.update({ guid: "123456789012",
                                         title: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided GUID/.test(ex));
  }
});

add_task(async function invalid_properties_for_existing_bookmark() {
  let bm = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/" });

  try {
    await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         type: PlacesUtils.bookmarks.TYPE_FOLDER });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/The bookmark type cannot be changed/.test(ex));
  }

  try {
    await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         parentGuid: "123456789012",
                                         index: 1 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/No bookmarks found for the provided parentGuid/.test(ex));
  }

  let past = new Date(Date.now() - 86400000);
  try {
    await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                         lastModified: past });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'lastModified'/.test(ex));
  }

  let folder = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                    parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  try {
    await PlacesUtils.bookmarks.update({ guid: folder.guid,
                                         url: "http://example.com/" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'url'/.test(ex));
  }

  let separator = await PlacesUtils.bookmarks.insert({ type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
                                                       parentGuid: PlacesUtils.bookmarks.unfiledGuid });
  try {
    await PlacesUtils.bookmarks.update({ guid: separator.guid,
                                         url: "http://example.com/" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'url'/.test(ex));
  }
  try {
    await PlacesUtils.bookmarks.update({ guid: separator.guid,
                                         title: "test" });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Invalid value for property 'title'/.test(ex));
  }
});

add_task(async function long_title_trim() {
  let longtitle = "a";
  for (let i = 0; i < 4096; i++) {
    longtitle += "a";
  }
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                title: "title" });
  checkBookmarkObject(bm);

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: longtitle });
  let newTitle = bm.title;
  Assert.equal(newTitle.length, 4096, "title should have been trimmed");

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.title, newTitle);
});

add_task(async function update_lastModified() {
  let yesterday = new Date(Date.now() - 86400000);
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER,
                                                title: "title",
                                                dateAdded: yesterday });
  checkBookmarkObject(bm);
  Assert.deepEqual(bm.lastModified, yesterday);

  let time = new Date();
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: time });
  checkBookmarkObject(bm);
  Assert.deepEqual(bm.lastModified, time);

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.deepEqual(bm.lastModified, time);

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            lastModified: yesterday });
  Assert.deepEqual(bm.lastModified, yesterday);

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "title2" });
  Assert.ok(bm.lastModified >= time);

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            title: "" });
  Assert.strictEqual(bm.title, "");

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.strictEqual(bm.title, "");
});

add_task(async function update_url() {
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
                                                url: "http://example.com/",
                                                title: "title" });
  checkBookmarkObject(bm);
  let lastModified = bm.lastModified;
  let frecency = frecencyForUrl(bm.url);
  Assert.ok(frecency > 0, "Check frecency has been updated");

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            url: "http://mozilla.org/" });
  checkBookmarkObject(bm);
  Assert.ok(bm.lastModified >= lastModified);
  Assert.equal(bm.url.href, "http://mozilla.org/");

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.url.href, "http://mozilla.org/");
  Assert.ok(bm.lastModified >= lastModified);

  Assert.equal(frecencyForUrl("http://example.com/"), frecency, "Check frecency for example.com");
  Assert.equal(frecencyForUrl("http://mozilla.org/"), frecency, "Check frecency for mozilla.org");
});

add_task(async function update_index() {
  let parent = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let f1 = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f1.index, 0);
  let f2 = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f2.index, 1);
  let f3 = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(f3.index, 2);
  let lastModified = f1.lastModified;

  f1 = await PlacesUtils.bookmarks.update({ guid: f1.guid,
                                            parentGuid: f1.parentGuid,
                                            index: 1});
  checkBookmarkObject(f1);
  Assert.equal(f1.index, 1);
  Assert.ok(f1.lastModified >= lastModified);

  parent = await PlacesUtils.bookmarks.fetch(f1.parentGuid);
  Assert.deepEqual(parent.lastModified, f1.lastModified);

  f2 = await PlacesUtils.bookmarks.fetch(f2.guid);
  Assert.equal(f2.index, 0);

  f3 = await PlacesUtils.bookmarks.fetch(f3.guid);
  Assert.equal(f3.index, 2);

  f3 = await PlacesUtils.bookmarks.update({ guid: f3.guid,
                                            index: 0 });
  f1 = await PlacesUtils.bookmarks.fetch(f1.guid);
  Assert.equal(f1.index, 2);

  f2 = await PlacesUtils.bookmarks.fetch(f2.guid);
  Assert.equal(f2.index, 1);
});

add_task(async function update_move_folder_into_descendant_throws() {
  let parent = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let descendant = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });

  try {
    await PlacesUtils.bookmarks.update({ guid: parent.guid,
                                         parentGuid: parent.guid,
                                         index: 0 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Cannot insert a folder into itself or one of its descendants/.test(ex));
  }

  try {
    await PlacesUtils.bookmarks.update({ guid: parent.guid,
                                         parentGuid: descendant.guid,
                                         index: 0 });
    Assert.ok(false, "Should have thrown");
  } catch (ex) {
    Assert.ok(/Cannot insert a folder into itself or one of its descendants/.test(ex));
  }
});

add_task(async function update_move() {
  let parent = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                    type: PlacesUtils.bookmarks.TYPE_FOLDER }) ;
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                url: "http://example.com/",
                                                type: PlacesUtils.bookmarks.TYPE_BOOKMARK }) ;
  let descendant = await PlacesUtils.bookmarks.insert({ parentGuid: parent.guid,
                                                        type: PlacesUtils.bookmarks.TYPE_FOLDER });
  Assert.equal(descendant.index, 1);
  let lastModified = bm.lastModified;

  // This is moving to a nonexisting index by purpose, it will be appended.
  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: descendant.guid,
                                            index: 1 });
  checkBookmarkObject(bm);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);
  Assert.ok(bm.lastModified >= lastModified);

  parent = await PlacesUtils.bookmarks.fetch(parent.guid);
  descendant = await PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.deepEqual(parent.lastModified, bm.lastModified);
  Assert.deepEqual(descendant.lastModified, bm.lastModified);
  Assert.equal(descendant.index, 0);

  bm = await PlacesUtils.bookmarks.fetch(bm.guid);
  Assert.equal(bm.parentGuid, descendant.guid);
  Assert.equal(bm.index, 0);

  bm = await PlacesUtils.bookmarks.update({ guid: bm.guid,
                                            parentGuid: parent.guid,
                                            index: 0 });
  Assert.equal(bm.parentGuid, parent.guid);
  Assert.equal(bm.index, 0);

  descendant = await PlacesUtils.bookmarks.fetch(descendant.guid);
  Assert.equal(descendant.index, 1);
});

add_task(async function update_move_append() {
  let folder_a =
    await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                         type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(folder_a);
  let folder_b =
    await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                         type: PlacesUtils.bookmarks.TYPE_FOLDER });
  checkBookmarkObject(folder_b);

  /* folder_a: [sep_1, sep_2, sep_3], folder_b: [] */
  let sep_1 = await PlacesUtils.bookmarks.insert({ parentGuid: folder_a.guid,
                                                   type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(sep_1);
  let sep_2 = await PlacesUtils.bookmarks.insert({ parentGuid: folder_a.guid,
                                                   type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(sep_2);
  let sep_3 = await PlacesUtils.bookmarks.insert({ parentGuid: folder_a.guid,
                                                   type: PlacesUtils.bookmarks.TYPE_SEPARATOR });
  checkBookmarkObject(sep_3);

  function ensurePosition(info, parentGuid, index) {
    checkBookmarkObject(info);
    Assert.equal(info.parentGuid, parentGuid);
    Assert.equal(info.index, index);
  }

  // folder_a: [sep_2, sep_3, sep_1], folder_b: []
  sep_1.index = PlacesUtils.bookmarks.DEFAULT_INDEX;
  // Note sep_1 includes parentGuid even though we're not moving the item to
  // another folder
  sep_1 = await PlacesUtils.bookmarks.update(sep_1);
  ensurePosition(sep_1, folder_a.guid, 2);
  sep_2 = await PlacesUtils.bookmarks.fetch(sep_2.guid);
  ensurePosition(sep_2, folder_a.guid, 0);
  sep_3 = await PlacesUtils.bookmarks.fetch(sep_3.guid);
  ensurePosition(sep_3, folder_a.guid, 1);
  sep_1 = await PlacesUtils.bookmarks.fetch(sep_1.guid);
  ensurePosition(sep_1, folder_a.guid, 2);

  // folder_a: [sep_2, sep_1], folder_b: [sep_3]
  sep_3.index = PlacesUtils.bookmarks.DEFAULT_INDEX;
  sep_3.parentGuid = folder_b.guid;
  sep_3 = await PlacesUtils.bookmarks.update(sep_3);
  ensurePosition(sep_3, folder_b.guid, 0);
  sep_2 = await PlacesUtils.bookmarks.fetch(sep_2.guid);
  ensurePosition(sep_2, folder_a.guid, 0);
  sep_1 = await PlacesUtils.bookmarks.fetch(sep_1.guid);
  ensurePosition(sep_1, folder_a.guid, 1);
  sep_3 = await PlacesUtils.bookmarks.fetch(sep_3.guid);
  ensurePosition(sep_3, folder_b.guid, 0);

  // folder_a: [sep_1], folder_b: [sep_3, sep_2]
  sep_2.index = Number.MAX_SAFE_INTEGER;
  sep_2.parentGuid = folder_b.guid;
  sep_2 = await PlacesUtils.bookmarks.update(sep_2);
  ensurePosition(sep_2, folder_b.guid, 1);
  sep_1 = await PlacesUtils.bookmarks.fetch(sep_1.guid);
  ensurePosition(sep_1, folder_a.guid, 0);
  sep_3 = await PlacesUtils.bookmarks.fetch(sep_3.guid);
  ensurePosition(sep_3, folder_b.guid, 0);
  sep_2 = await PlacesUtils.bookmarks.fetch(sep_2.guid);
  ensurePosition(sep_2, folder_b.guid, 1);
});
