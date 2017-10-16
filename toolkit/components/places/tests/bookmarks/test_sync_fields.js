// Tracks a set of bookmark guids and their syncChangeCounter field and
// provides a simple way for the test to check the correct fields had the
// counter incremented.
class CounterTracker {
  constructor() {
    this.tracked = new Map();
  }

  async _getCounter(guid) {
    let fields = await PlacesTestUtils.fetchBookmarkSyncFields(guid);
    if (!fields.length) {
      throw new Error(`Item ${guid} does not exist`);
    }
    return fields[0].syncChangeCounter;
  }

  // Call this after creating a new bookmark.
  async track(guid, name, expectedInitial = 1) {
    if (this.tracked.has(guid)) {
      throw new Error(`Already tracking item ${guid}`);
    }
    let initial = await this._getCounter(guid);
    Assert.equal(initial, expectedInitial, `Initial value of item '${name}' is correct`);
    this.tracked.set(guid, { name, value: expectedInitial });
  }

  // Call this to check *only* the specified IDs had a change increment, and
  // that none of the other "tracked" ones did.
  async check(...expectedToIncrement) {
    do_print(`Checking counter for items ${JSON.stringify(expectedToIncrement)}`);
    for (let [guid, entry] of this.tracked) {
      let { name, value } = entry;
      let newValue = await this._getCounter(guid);
      let desc = `record '${name}' (guid=${guid})`;
      if (expectedToIncrement.includes(guid)) {
        // Note we don't check specifically for +1, as some changes will
        // increment the counter by more than 1 (which is OK).
        Assert.ok(newValue > value,
                  `${desc} was expected to increment - was ${value}, now ${newValue}`);
        this.tracked.set(guid, { name, value: newValue });
      } else {
        Assert.equal(newValue, value, `${desc} was NOT expected to increment`);
      }
    }
  }
}

async function checkSyncFields(guid, expected) {
  let results = await PlacesTestUtils.fetchBookmarkSyncFields(guid);
  if (!results.length) {
    throw new Error(`Missing sync fields for ${guid}`);
  }
  for (let name in expected) {
    let expectedValue = expected[name];
    Assert.equal(results[0][name], expectedValue, `field ${name} matches item ${guid}`);
  }
}

// Common test cases for sync field changes.
class TestCases {
  async run() {
    do_print("Test 1: inserts, updates, tags, and keywords");
    try {
      await this.testChanges();
    } finally {
      do_print("Reset sync fields after test 1");
      await PlacesTestUtils.markBookmarksAsSynced();
    }

    do_print("Test 2: reparenting");
    try {
      await this.testReparenting();
    } finally {
      do_print("Reset sync fields after test 2");
      await PlacesTestUtils.markBookmarksAsSynced();
    }

    do_print("Test 3: separators");
    try {
      await this.testSeparators();
    } finally {
      do_print("Reset sync fields after test 3");
      await PlacesTestUtils.markBookmarksAsSynced();
    }
  }

  async testChanges() {
    let testUri = NetUtil.newURI("http://test.mozilla.org");

    let guid = await this.insertBookmark(PlacesUtils.bookmarks.unfiledGuid,
                                          testUri,
                                          PlacesUtils.bookmarks.DEFAULT_INDEX,
                                          "bookmark title");
    do_print(`Inserted bookmark ${guid}`);
    await checkSyncFields(guid, { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NEW,
                                   syncChangeCounter: 1 });

    // Pretend Sync just did whatever it does
    await PlacesTestUtils.setBookmarkSyncFields({ guid,
                                                  syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL });
    do_print(`Updated sync status of ${guid}`);
    await checkSyncFields(guid, { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
                                   syncChangeCounter: 1 });

    // update it - it should increment the change counter
    await this.setTitle(guid, "new title");
    do_print(`Changed title of ${guid}`);
    await checkSyncFields(guid, { syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL,
                                   syncChangeCounter: 2 });

    await this.setAnno(guid, "random-anno", "random-value");
    do_print(`Set anno on ${guid}`);
    await checkSyncFields(guid, { syncChangeCounter: 3 });

    // Tagging a bookmark should update its change counter.
    await this.tagURI(testUri, ["test-tag"]);
    do_print(`Tagged bookmark ${guid}`);
    await checkSyncFields(guid, { syncChangeCounter: 4 });

    await this.setKeyword(guid, "keyword");
    do_print(`Set keyword for bookmark ${guid}`);
    await checkSyncFields(guid, { syncChangeCounter: 5 });

    await this.removeKeyword(guid, "keyword");
    do_print(`Removed keyword from bookmark ${guid}`);
    await checkSyncFields(guid, { syncChangeCounter: 6 });
  }

  async testSeparators() {
    let insertSyncedBookmark = uri => {
      return this.insertBookmark(PlacesUtils.bookmarks.unfiledGuid,
                                 NetUtil.newURI(uri),
                                 PlacesUtils.bookmarks.DEFAULT_INDEX,
                                 "A bookmark name");
    };

    await insertSyncedBookmark("http://foo.bar");
    let secondBmk = await insertSyncedBookmark("http://bar.foo");
    let sepGuid = await this.insertSeparator(PlacesUtils.bookmarks.unfiledGuid, PlacesUtils.bookmarks.DEFAULT_INDEX);
    await insertSyncedBookmark("http://barbar.foo");

    do_print("Move a bookmark around the separator");
    await this.moveItem(secondBmk, PlacesUtils.bookmarks.unfiledGuid, 4);
    await checkSyncFields(sepGuid, { syncChangeCounter: 2 });

    do_print("Move a separator around directly");
    await this.moveItem(sepGuid, PlacesUtils.bookmarks.unfiledGuid, 0);
    await checkSyncFields(sepGuid, { syncChangeCounter: 3 });
  }

  async testReparenting() {
    let counterTracker = new CounterTracker();

    let folder1 = await this.createFolder(PlacesUtils.bookmarks.unfiledGuid,
                                           "folder1",
                                           PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_print(`Created the first folder, guid is ${folder1}`);

    // New folder should have a change recorded.
    await counterTracker.track(folder1, "folder 1");

    // Put a new bookmark in the folder.
    let testUri = NetUtil.newURI("http://test2.mozilla.org");
    let child1 = await this.insertBookmark(folder1,
                                            testUri,
                                            PlacesUtils.bookmarks.DEFAULT_INDEX,
                                            "bookmark 1");
    do_print(`Created a new bookmark into ${folder1}, guid is ${child1}`);
    // both the folder and the child should have a change recorded.
    await counterTracker.track(child1, "child 1");
    await counterTracker.check(folder1);

    // A new child in the folder at index 0 - even though the existing child
    // was bumped down the list, it should *not* have a change recorded.
    let child2 = await this.insertBookmark(folder1,
                                            testUri,
                                            0,
                                            "bookmark 2");
    do_print(`Created a second new bookmark into folder ${folder1}, guid is ${child2}`);

    await counterTracker.track(child2, "child 2");
    await counterTracker.check(folder1);

    // Move the items within the same folder - this should result in just a
    // change for the parent, but for neither of the children.
    // child0 is currently at index 0, so move child1 there.
    await this.moveItem(child1, folder1, 0);
    await counterTracker.check(folder1);

    // Another folder to play with.
    let folder2 = await this.createFolder(PlacesUtils.bookmarks.unfiledGuid,
                                           "folder2",
                                           PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_print(`Created a second new folder, guid is ${folder2}`);
    await counterTracker.track(folder2, "folder 2");
    // nothing else has changed.
    await counterTracker.check();

    // Move one of the children to the new folder.
    do_print(`Moving bookmark ${child2} from folder ${folder1} to folder ${folder2}`);
    await this.moveItem(child2, folder2, PlacesUtils.bookmarks.DEFAULT_INDEX);
    // child1 should have no change, everything should have a new change.
    await counterTracker.check(folder1, folder2, child2);

    // Move the new folder to another root.
    await this.moveItem(folder2, PlacesUtils.bookmarks.toolbarGuid,
                         PlacesUtils.bookmarks.DEFAULT_INDEX);
    do_print(`Moving folder ${folder2} to toolbar`);
    await counterTracker.check(folder2, PlacesUtils.bookmarks.toolbarGuid,
                                PlacesUtils.bookmarks.unfiledGuid);

    let child3 = await this.insertBookmark(folder2,
                                            testUri,
                                            0,
                                            "bookmark 3");
    do_print(`Prepended child ${child3} to folder ${folder2}`);
    await counterTracker.check(folder2, child3);

    // Reordering should only track the parent.
    await this.reorder(folder2, [child2, child3]);
    do_print(`Reorder children of ${folder2}`);
    await counterTracker.check(folder2);

    // All fields still have a syncStatus of SYNC_STATUS_NEW - so deleting them
    // should *not* cause any deleted items to be written.
    await this.removeItem(folder1);
    Assert.equal((await PlacesTestUtils.fetchSyncTombstones()).length, 0);

    // Set folder2 and child2 to have a state of SYNC_STATUS_NORMAL and deleting
    // them will cause both GUIDs to be written to moz_bookmarks_deleted.
    await PlacesTestUtils.setBookmarkSyncFields({ guid: folder2,
                                                  syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL });
    await PlacesTestUtils.setBookmarkSyncFields({ guid: child2,
                                                  syncStatus: PlacesUtils.bookmarks.SYNC_STATUS.NORMAL });
    await this.removeItem(folder2);
    let tombstones = await PlacesTestUtils.fetchSyncTombstones();
    let tombstoneGuids = sortBy(tombstones, "guid").map(({ guid }) => guid);
    Assert.equal(tombstoneGuids.length, 2);
    Assert.deepEqual(tombstoneGuids, [folder2, child2].sort(compareAscending));
  }

  // Annos don't have an async API, so we use the sync API for both tests.
  async setAnno(guid, name, value) {
    let id = await PlacesUtils.promiseItemId(guid);
    PlacesUtils.annotations.setItemAnnotation(id, name, value, 0,
                                              PlacesUtils.annotations.EXPIRE_NEVER);
  }
}

// Exercises the legacy, synchronous `nsINavBookmarksService` calls implemented
// in C++.
class SyncTestCases extends TestCases {
  async createFolder(parentGuid, title, index) {
    let parentId = await PlacesUtils.promiseItemId(parentGuid);
    let id = PlacesUtils.bookmarks.createFolder(parentId, title, index);
    return PlacesUtils.promiseItemGuid(id);
  }

  async insertBookmark(parentGuid, uri, index, title) {
    let parentId = await PlacesUtils.promiseItemId(parentGuid);
    let id = PlacesUtils.bookmarks.insertBookmark(parentId, uri, index, title);
    return PlacesUtils.promiseItemGuid(id);
  }

  async insertSeparator(parentGuid, index) {
    let parentId = await PlacesUtils.promiseItemId(parentGuid);
    let id = PlacesUtils.bookmarks.insertSeparator(parentId, index);
    return PlacesUtils.promiseItemGuid(id);
  }

  async moveItem(guid, newParentGuid, index) {
    let id = await PlacesUtils.promiseItemId(guid);
    let newParentId = await PlacesUtils.promiseItemId(newParentGuid);
    PlacesUtils.bookmarks.moveItem(id, newParentId, index);
  }

  async removeItem(guid) {
    let id = await PlacesUtils.promiseItemId(guid);
    PlacesUtils.bookmarks.removeItem(id);
  }

  async setTitle(guid, title) {
    let id = await PlacesUtils.promiseItemId(guid);
    PlacesUtils.bookmarks.setItemTitle(id, title);
  }

  async setKeyword(guid, keyword) {
    let id = await PlacesUtils.promiseItemId(guid);
    PlacesUtils.bookmarks.setKeywordForBookmark(id, keyword);
  }

  async removeKeyword(guid, keyword) {
    let id = await PlacesUtils.promiseItemId(guid);
    if (PlacesUtils.bookmarks.getKeywordForBookmark(id) != keyword) {
      throw new Error(`Keyword ${keyword} not set for bookmark ${guid}`);
    }
    PlacesUtils.bookmarks.setKeywordForBookmark(id, "");
  }

  async tagURI(uri, tags) {
    PlacesUtils.tagging.tagURI(uri, tags);
  }

  async reorder(parentGuid, childGuids) {
    let parentId = await PlacesUtils.promiseItemId(parentGuid);
    for (let index = 0; index < childGuids.length; ++index) {
      let id = await PlacesUtils.promiseItemId(childGuids[index]);
      PlacesUtils.bookmarks.moveItem(id, parentId, index);
    }
  }
}

async function findTagFolder(tag) {
  let db = await PlacesUtils.promiseDBConnection();
  let results = await db.executeCached(`
  SELECT guid
  FROM moz_bookmarks
  WHERE type = :type AND
        parent = :tagsFolderId AND
        title = :tag`,
  { type: PlacesUtils.bookmarks.TYPE_FOLDER,
    tagsFolderId: PlacesUtils.tagsFolderId, tag });
  return results.length ? results[0].getResultByName("guid") : null;
}

// Exercises the new, async calls implemented in `Bookmarks.jsm`.
class AsyncTestCases extends TestCases {
  async createFolder(parentGuid, title, index) {
    let item = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      parentGuid,
      title,
      index,
    });
    return item.guid;
  }

  async insertBookmark(parentGuid, uri, index, title) {
    let item = await PlacesUtils.bookmarks.insert({
      parentGuid,
      url: uri,
      index,
      title,
    });
    return item.guid;
  }

  async insertSeparator(parentGuid, index) {
    let item = await PlacesUtils.bookmarks.insert({
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      parentGuid,
      index
    });
    return item.guid;
  }

  async moveItem(guid, newParentGuid, index) {
    await PlacesUtils.bookmarks.update({
      guid,
      parentGuid: newParentGuid,
      index,
    });
  }

  async removeItem(guid) {
    await PlacesUtils.bookmarks.remove(guid);
  }

  async setTitle(guid, title) {
    await PlacesUtils.bookmarks.update({ guid, title });
  }

  async setKeyword(guid, keyword) {
    let item = await PlacesUtils.bookmarks.fetch(guid);
    if (!item) {
      throw new Error(`Cannot set keyword ${
        keyword} on nonexistent bookmark ${guid}`);
    }
    await PlacesUtils.keywords.insert({ keyword, url: item.url });
  }

  async removeKeyword(guid, keyword) {
    let item = await PlacesUtils.bookmarks.fetch(guid);
    if (!item) {
      throw new Error(`Cannot remove keyword ${
        keyword} from nonexistent bookmark ${guid}`);
    }
    let entry = await PlacesUtils.keywords.fetch({ keyword, url: item.url });
    if (!entry) {
      throw new Error(`Keyword ${keyword} not set on bookmark ${guid}`);
    }
    await PlacesUtils.keywords.remove(entry);
  }

  // There's no async API for tags, but the `PlacesUtils.bookmarks` methods are
  // tag-aware, and should bump the change counters for tagged bookmarks when
  // called directly.
  async tagURI(uri, tags) {
    for (let tag of tags) {
      let tagFolderGuid = await findTagFolder(tag);
      if (!tagFolderGuid) {
        let tagFolder = await PlacesUtils.bookmarks.insert({
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          parentGuid: PlacesUtils.bookmarks.tagsGuid,
          title: tag,
        });
        tagFolderGuid = tagFolder.guid;
      }
      await PlacesUtils.bookmarks.insert({
        url: uri,
        parentGuid: tagFolderGuid,
      });
    }
  }

  async reorder(parentGuid, childGuids) {
    await PlacesUtils.bookmarks.reorder(parentGuid, childGuids);
  }
}

add_task(async function test_sync_api() {
  let tests = new SyncTestCases();
  await tests.run();
});

add_task(async function test_async_api() {
  let tests = new AsyncTestCases();
  await tests.run();
});
