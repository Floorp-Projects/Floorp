/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const bmsvc = PlacesUtils.bookmarks;
const obsvc = PlacesUtils.observers;
const tagssvc = PlacesUtils.tagging;
const PT = PlacesTransactions;
const menuGuid = PlacesUtils.bookmarks.menuGuid;

ChromeUtils.defineESModuleGetters(this, {
  Preferences: "resource://gre/modules/Preferences.sys.mjs",
});

// Create and add bookmarks observer.
var observer = {
  tagRelatedGuids: new Set(),

  reset() {
    this.itemsAdded = new Map();
    this.itemsRemoved = new Map();
    this.itemsMoved = new Map();
    this.itemsKeywordChanged = new Map();
    this.itemsTitleChanged = new Map();
    this.itemsUrlChanged = new Map();
  },

  handlePlacesEvents(events) {
    for (let event of events) {
      switch (event.type) {
        case "bookmark-added":
          // Ignore tag items.
          if (event.isTagging) {
            this.tagRelatedGuids.add(event.guid);
            return;
          }

          this.itemsAdded.set(event.guid, {
            itemId: event.id,
            parentGuid: event.parentGuid,
            index: event.index,
            itemType: event.itemType,
            title: event.title,
            url: event.url,
          });
          break;
        case "bookmark-removed":
          if (this.tagRelatedGuids.has(event.guid)) {
            return;
          }

          this.itemsRemoved.set(event.guid, {
            parentGuid: event.parentGuid,
            index: event.index,
            itemType: event.itemType,
          });
          break;
        case "bookmark-moved":
          this.itemsMoved.set(event.guid, {
            oldParentGuid: event.oldParentGuid,
            oldIndex: event.oldIndex,
            newParentGuid: event.parentGuid,
            newIndex: event.index,
            itemType: event.itemType,
          });
          break;
        case "bookmark-keyword-changed":
          if (this.tagRelatedGuids.has(event.guid)) {
            return;
          }

          this.itemsKeywordChanged.set(event.guid, {
            keyword: event.keyword,
          });
          break;
        case "bookmark-title-changed":
          if (this.tagRelatedGuids.has(event.guid)) {
            return;
          }

          this.itemsTitleChanged.set(event.guid, {
            title: event.title,
            parentGuid: event.parentGuid,
          });
          break;
        case "bookmark-url-changed":
          if (this.tagRelatedGuids.has(event.guid)) {
            return;
          }

          this.itemsUrlChanged.set(event.guid, {
            url: event.url,
          });
          break;
      }
    }
  },
};
observer.reset();

// index at which items should begin
var bmStartIndex = 0;

function run_test() {
  observer.handlePlacesEvents = observer.handlePlacesEvents.bind(observer);
  obsvc.addListener(
    [
      "bookmark-added",
      "bookmark-removed",
      "bookmark-moved",
      "bookmark-keyword-changed",
      "bookmark-title-changed",
      "bookmark-url-changed",
    ],
    observer.handlePlacesEvents
  );
  registerCleanupFunction(function () {
    obsvc.removeListener(
      [
        "bookmark-added",
        "bookmark-removed",
        "bookmark-moved",
        "bookmark-keyword-changed",
        "bookmark-title-changed",
        "bookmark-url-changed",
      ],
      observer.handlePlacesEvents
    );
  });

  run_next_test();
}

function sanityCheckTransactionHistory() {
  Assert.ok(PT.undoPosition <= PT.length);

  let check_entry_throws = f => {
    try {
      f();
      do_throw("PT.entry should throw for invalid input");
    } catch (ex) {}
  };
  check_entry_throws(() => PT.entry(-1));
  check_entry_throws(() => PT.entry({}));
  check_entry_throws(() => PT.entry(PT.length));

  if (PT.undoPosition < PT.length) {
    Assert.equal(PT.topUndoEntry, PT.entry(PT.undoPosition));
  } else {
    Assert.equal(null, PT.topUndoEntry);
  }
  if (PT.undoPosition > 0) {
    Assert.equal(PT.topRedoEntry, PT.entry(PT.undoPosition - 1));
  } else {
    Assert.equal(null, PT.topRedoEntry);
  }
}

function getTransactionsHistoryState() {
  let history = [];
  for (let i = 0; i < PT.length; i++) {
    history.push(PT.entry(i));
  }
  return [history, PT.undoPosition];
}

function ensureUndoState(aExpectedEntries = [], aExpectedUndoPosition = 0) {
  // ensureUndoState is called in various places during this test, so it's
  // a good places to sanity-check the transaction-history APIs in all
  // cases.
  sanityCheckTransactionHistory();

  let [actualEntries, actualUndoPosition] = getTransactionsHistoryState();
  Assert.equal(actualEntries.length, aExpectedEntries.length);
  Assert.equal(actualUndoPosition, aExpectedUndoPosition);

  function checkEqualEntries(aExpectedEntry, aActualEntry) {
    Assert.equal(aExpectedEntry.length, aActualEntry.length);
    aExpectedEntry.forEach((t, i) => Assert.equal(t, aActualEntry[i]));
  }
  aExpectedEntries.forEach((e, i) => checkEqualEntries(e, actualEntries[i]));
}

function ensureItemsAdded(...items) {
  let expectedResultsCount = items.length;

  for (let item of items) {
    if ("children" in item) {
      expectedResultsCount += item.children.length;
    }
    Assert.ok(
      observer.itemsAdded.has(item.guid),
      `Should have the expected guid ${item.guid}`
    );
    let info = observer.itemsAdded.get(item.guid);
    Assert.equal(
      info.parentGuid,
      item.parentGuid,
      "Should have notified the correct parentGuid"
    );
    for (let propName of ["title", "index", "itemType"]) {
      if (propName in item) {
        Assert.equal(info[propName], item[propName]);
      }
    }
    if ("url" in item) {
      Assert.ok(
        Services.io.newURI(info.url).equals(Services.io.newURI(item.url)),
        "Should have the correct url"
      );
    }
  }

  Assert.equal(
    observer.itemsAdded.size,
    expectedResultsCount,
    "Should have added the correct number of items"
  );
}

function ensureItemsRemoved(...items) {
  let expectedResultsCount = items.length;

  for (let item of items) {
    // We accept both guids and full info object here.
    if (typeof item == "string") {
      Assert.ok(
        observer.itemsRemoved.has(item),
        `Should have removed the expected guid ${item}`
      );
    } else {
      if ("children" in item) {
        expectedResultsCount += item.children.length;
      }

      Assert.ok(
        observer.itemsRemoved.has(item.guid),
        `Should have removed expected guid ${item.guid}`
      );
      let info = observer.itemsRemoved.get(item.guid);
      Assert.equal(
        info.parentGuid,
        item.parentGuid,
        "Should have notified the correct parentGuid"
      );
      if ("index" in item) {
        Assert.equal(info.index, item.index);
      }
    }
  }

  Assert.equal(
    observer.itemsRemoved.size,
    expectedResultsCount,
    "Should have removed the correct number of items"
  );
}

function ensureItemsMoved(...items) {
  Assert.equal(
    observer.itemsMoved.size,
    items.length,
    "Should have received the correct number of moved notifications"
  );
  for (let item of items) {
    Assert.ok(
      observer.itemsMoved.has(item.guid),
      `Observer should have a move for ${item.guid}`
    );
    let info = observer.itemsMoved.get(item.guid);
    Assert.equal(
      info.oldParentGuid,
      item.oldParentGuid,
      "Should have the correct old parent guid"
    );
    Assert.equal(
      info.oldIndex,
      item.oldIndex,
      "Should have the correct old index"
    );
    Assert.equal(
      info.newParentGuid,
      item.newParentGuid,
      "Should have the correct new parent guid"
    );
    Assert.equal(
      info.newIndex,
      item.newIndex,
      "Should have the correct new index"
    );
  }
}

function ensureItemsKeywordChanged(...items) {
  for (const item of items) {
    Assert.ok(
      observer.itemsKeywordChanged.has(item.guid),
      `Observer should have a keyword changed for ${item.guid}`
    );
    const info = observer.itemsKeywordChanged.get(item.guid);
    Assert.equal(info.keyword, item.keyword, "Should have the correct keyword");
  }
}

function ensureItemsTitleChanged(...items) {
  Assert.equal(
    observer.itemsTitleChanged.size,
    items.length,
    "Should have received the correct number of bookmark-title-changed notifications"
  );
  for (const item of items) {
    Assert.ok(
      observer.itemsTitleChanged.has(item.guid),
      `Observer should have a title changed for ${item.guid}`
    );
    const info = observer.itemsTitleChanged.get(item.guid);
    Assert.equal(info.title, item.title, "Should have the correct title");
    Assert.equal(
      info.parentGuid,
      item.parentGuid,
      "Should have the correct parent guid"
    );
  }
}

function ensureItemsUrlChanged(...items) {
  Assert.equal(
    observer.itemsUrlChanged.size,
    items.length,
    "Should have received the correct number of bookmark-url-changed notifications"
  );
  for (const item of items) {
    Assert.ok(
      observer.itemsUrlChanged.has(item.guid),
      `Observer should have a title changed for ${item.guid}`
    );
    const info = observer.itemsUrlChanged.get(item.guid);
    Assert.equal(info.url, item.url, "Should have the correct url");
  }
}

function ensureTagsForURI(aURI, aTags) {
  let tagsSet = tagssvc.getTagsForURI(Services.io.newURI(aURI));
  Assert.equal(tagsSet.length, aTags.length);
  Assert.ok(aTags.every(t => tagsSet.includes(t)));
}

function createTestFolderInfo(
  title = "Test Folder",
  parentGuid = menuGuid,
  children = undefined
) {
  let info = { parentGuid, title };
  if (children) {
    info.children = children;
  }
  return info;
}

function removeAllDatesInTree(tree) {
  if ("lastModified" in tree) {
    delete tree.lastModified;
  }
  if ("dateAdded" in tree) {
    delete tree.dateAdded;
  }

  if (!tree.children) {
    return;
  }

  for (let child of tree.children) {
    removeAllDatesInTree(child);
  }
}

// Checks if two bookmark trees (as returned by promiseBookmarksTree) are the
// same.
// false value for aCheckParentAndPosition is ignored if aIsRestoredItem is set.
async function ensureEqualBookmarksTrees(
  aOriginal,
  aNew,
  aIsRestoredItem = true,
  aCheckParentAndPosition = false,
  aIgnoreAllDates = false
) {
  // Note "id" is not-enumerable, and is therefore skipped by Object.keys (both
  // ours and the one at deepEqual). This is fine for us because ids are not
  // restored by Redo.
  if (aIsRestoredItem) {
    if (aIgnoreAllDates) {
      removeAllDatesInTree(aOriginal);
      removeAllDatesInTree(aNew);
    } else if (!aOriginal.lastModified) {
      // Ignore lastModified for newly created items, for performance reasons.
      aNew.lastModified = aOriginal.lastModified;
    }
    Assert.deepEqual(aOriginal, aNew);
    return;
  }

  for (let property of Object.keys(aOriginal)) {
    if (property == "children") {
      Assert.equal(aOriginal.children.length, aNew.children.length);
      for (let i = 0; i < aOriginal.children.length; i++) {
        await ensureEqualBookmarksTrees(
          aOriginal.children[i],
          aNew.children[i],
          false,
          true,
          aIgnoreAllDates
        );
      }
    } else if (property == "guid") {
      // guid shouldn't be copied if the item was not restored.
      Assert.notEqual(aOriginal.guid, aNew.guid);
    } else if (property == "dateAdded") {
      // dateAdded shouldn't be copied if the item was not restored.
      Assert.ok(is_time_ordered(aOriginal.dateAdded, aNew.dateAdded));
    } else if (property == "lastModified") {
      // same same, except for the never-changed case
      if (!aOriginal.lastModified) {
        Assert.ok(!aNew.lastModified);
      } else {
        Assert.ok(is_time_ordered(aOriginal.lastModified, aNew.lastModified));
      }
    } else if (
      aCheckParentAndPosition ||
      (property != "parentGuid" && property != "index")
    ) {
      Assert.deepEqual(aOriginal[property], aNew[property]);
    }
  }
}

async function ensureBookmarksTreeRestoredCorrectly(
  ...aOriginalBookmarksTrees
) {
  for (let originalTree of aOriginalBookmarksTrees) {
    let restoredTree = await PlacesUtils.promiseBookmarksTree(
      originalTree.guid
    );
    await ensureEqualBookmarksTrees(originalTree, restoredTree);
  }
}

async function ensureBookmarksTreeRestoredCorrectlyExceptDates(
  ...aOriginalBookmarksTrees
) {
  for (let originalTree of aOriginalBookmarksTrees) {
    let restoredTree = await PlacesUtils.promiseBookmarksTree(
      originalTree.guid
    );
    await ensureEqualBookmarksTrees(
      originalTree,
      restoredTree,
      true,
      false,
      true
    );
  }
}

async function ensureNonExistent(...aGuids) {
  for (let guid of aGuids) {
    Assert.strictEqual(await PlacesUtils.promiseBookmarksTree(guid), null);
  }
}

add_task(async function test_recycled_transactions() {
  async function ensureTransactThrowsFor(aTransaction) {
    let [txns, undoPosition] = getTransactionsHistoryState();
    try {
      await aTransaction.transact();
      do_throw("Shouldn't be able to use the same transaction twice");
    } catch (ex) {}
    ensureUndoState(txns, undoPosition);
  }

  let txn_a = PT.NewFolder(createTestFolderInfo());
  await txn_a.transact();
  ensureUndoState([[txn_a]], 0);
  await ensureTransactThrowsFor(txn_a);

  await PT.undo();
  ensureUndoState([[txn_a]], 1);
  ensureTransactThrowsFor(txn_a);

  await PT.clearTransactionsHistory();
  ensureUndoState();
  ensureTransactThrowsFor(txn_a);

  let txn_b = PT.NewFolder(createTestFolderInfo());
  let results = await PT.batch([
    txn_a, // Shouldn't be able to use the same transaction twice.
    txn_b,
  ]);
  Assert.strictEqual(
    results[0],
    undefined,
    "First transaction should fail as it can't be reused"
  );
  ensureUndoState([[txn_b]], 0);

  await PT.undo();
  ensureUndoState([[txn_b]], 1);
  ensureTransactThrowsFor(txn_a);
  ensureTransactThrowsFor(txn_b);

  await PT.clearTransactionsHistory();
  ensureUndoState();
  observer.reset();
});

add_task(async function test_new_folder_with_children() {
  let folder_info = createTestFolderInfo(
    "Test folder",
    PlacesUtils.bookmarks.menuGuid,
    [
      {
        url: "http://test_create_item.com",
        title: "Test creating an item",
      },
    ]
  );
  ensureUndoState();
  let txn = PT.NewFolder(folder_info);
  folder_info.guid = await txn.transact();
  let originalInfo = await PlacesUtils.promiseBookmarksTree(folder_info.guid);
  let ensureDo = async function (aRedo = false) {
    ensureUndoState([[txn]], 0);
    ensureItemsAdded(folder_info);
    if (aRedo) {
      // Ignore lastModified in the comparison, for performance reasons.
      originalInfo.lastModified = null;
      await ensureBookmarksTreeRestoredCorrectlyExceptDates(originalInfo);
    }
    observer.reset();
  };

  let ensureUndo = () => {
    ensureUndoState([[txn]], 1);
    ensureItemsRemoved({
      guid: folder_info.guid,
      parentGuid: folder_info.parentGuid,
      index: bmStartIndex,
      children: [
        {
          title: "Test creating an item",
          url: "http://test_create_item.com",
        },
      ],
    });
    observer.reset();
  };

  await ensureDo();
  await PT.undo();
  await ensureUndo();
  await PT.redo();
  await ensureDo(true);
  await PT.undo();
  ensureUndo();
  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_new_bookmark() {
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test_create_item.com",
    index: bmStartIndex,
    title: "Test creating an item",
  };

  ensureUndoState();
  let txn = PT.NewBookmark(bm_info);
  bm_info.guid = await txn.transact();

  let originalInfo = await PlacesUtils.promiseBookmarksTree(bm_info.guid);
  let ensureDo = async function (aRedo = false) {
    ensureUndoState([[txn]], 0);
    await ensureItemsAdded(bm_info);
    if (aRedo) {
      await ensureBookmarksTreeRestoredCorrectly(originalInfo);
    }
    observer.reset();
  };
  let ensureUndo = () => {
    ensureUndoState([[txn]], 1);
    ensureItemsRemoved({
      guid: bm_info.guid,
      parentGuid: bm_info.parentGuid,
      index: bmStartIndex,
    });
    observer.reset();
  };

  await ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo(true);
  await ensureDo();
  await PT.undo();
  ensureUndo();

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_merge_create_folder_and_item() {
  let folder_info = createTestFolderInfo();
  let bm_info = {
    url: "http://test_create_item_to_folder.com",
    title: "Test Bookmark",
    index: bmStartIndex,
  };

  let folderTxn = PT.NewFolder(folder_info);
  let bmTxn;
  let results = await PT.batch([
    folderTxn,
    previousResults => {
      bm_info.parentGuid = previousResults[0];
      return (bmTxn = PT.NewBookmark(bm_info));
    },
  ]);
  folder_info.guid = results[0];
  bm_info.guid = results[1];

  let ensureDo = async function () {
    ensureUndoState([[bmTxn, folderTxn]], 0);
    await ensureItemsAdded(folder_info, bm_info);
    observer.reset();
  };

  let ensureUndo = () => {
    ensureUndoState([[bmTxn, folderTxn]], 1);
    ensureItemsRemoved(folder_info, bm_info);
    observer.reset();
  };

  await ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo();
  await ensureDo();
  await PT.undo();
  ensureUndo();

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_move_items_to_folder() {
  let folder_a_info = createTestFolderInfo("Folder A");
  let bm_a_info = { url: "http://test_move_items.com", title: "Bookmark A" };
  let bm_b_info = { url: "http://test_move_items.com", title: "Bookmark B" };

  // Test moving items within the same folder.
  let folder_a_txn = PT.NewFolder(folder_a_info);
  let bm_a_txn, bm_b_txn;
  let results = await PT.batch([
    folder_a_txn,
    previousResults => {
      bm_a_info.parentGuid = previousResults[0];
      return (bm_a_txn = PT.NewBookmark(bm_a_info));
    },
    previousResults => {
      bm_b_info.parentGuid = previousResults[0];
      return (bm_b_txn = PT.NewBookmark(bm_b_info));
    },
  ]);
  console.log("results: " + results);
  folder_a_info.guid = results[0];
  bm_a_info.guid = results[1];
  bm_b_info.guid = results[2];

  ensureUndoState([[bm_b_txn, bm_a_txn, folder_a_txn]], 0);

  let moveTxn = PT.Move({
    guid: bm_a_info.guid,
    newParentGuid: folder_a_info.guid,
  });
  await moveTxn.transact();

  let ensureDo = () => {
    ensureUndoState([[moveTxn], [bm_b_txn, bm_a_txn, folder_a_txn]], 0);
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_a_info.guid,
      newParentGuid: folder_a_info.guid,
      oldIndex: 0,
      newIndex: 1,
    });
    observer.reset();
  };
  let ensureUndo = () => {
    ensureUndoState([[moveTxn], [bm_b_txn, bm_a_txn, folder_a_txn]], 1);
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_a_info.guid,
      newParentGuid: folder_a_info.guid,
      oldIndex: 1,
      newIndex: 0,
    });
    observer.reset();
  };

  ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo();
  ensureDo();
  await PT.undo();
  ensureUndo();

  await PT.clearTransactionsHistory(false, true);
  ensureUndoState([[bm_b_txn, bm_a_txn, folder_a_txn]], 0);

  // Test moving items between folders.
  let folder_b_info = createTestFolderInfo("Folder B");
  let folder_b_txn = PT.NewFolder(folder_b_info);
  folder_b_info.guid = await folder_b_txn.transact();
  ensureUndoState([[folder_b_txn], [bm_b_txn, bm_a_txn, folder_a_txn]], 0);

  moveTxn = PT.Move({
    guid: bm_a_info.guid,
    newParentGuid: folder_b_info.guid,
    newIndex: bmsvc.DEFAULT_INDEX,
  });
  await moveTxn.transact();

  ensureDo = () => {
    ensureUndoState(
      [[moveTxn], [folder_b_txn], [bm_b_txn, bm_a_txn, folder_a_txn]],
      0
    );
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_a_info.guid,
      newParentGuid: folder_b_info.guid,
      oldIndex: 0,
      newIndex: 0,
    });
    observer.reset();
  };
  ensureUndo = () => {
    ensureUndoState(
      [[moveTxn], [folder_b_txn], [bm_b_txn, bm_a_txn, folder_a_txn]],
      1
    );
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_b_info.guid,
      newParentGuid: folder_a_info.guid,
      oldIndex: 0,
      newIndex: 0,
    });
    observer.reset();
  };

  ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo();
  ensureDo();
  await PT.undo();
  ensureUndo();

  // Clean up
  await PT.undo(); // folder_b_txn
  await PT.undo(); // folder_a_txn + the bookmarks;
  Assert.equal(observer.itemsRemoved.size, 4);
  ensureUndoState(
    [[moveTxn], [folder_b_txn], [bm_b_txn, bm_a_txn, folder_a_txn]],
    3
  );
  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_move_multiple_items_to_folder() {
  let folder_a_info = createTestFolderInfo("Folder A");
  let bm_a_info = { url: "http://test_move_items.com", title: "Bookmark A" };
  let bm_b_info = { url: "http://test_move_items.com", title: "Bookmark B" };
  let bm_c_info = { url: "http://test_move_items.com", title: "Bookmark C" };

  // Test moving items within the same folder.

  let folder_a_txn = PT.NewFolder(folder_a_info);
  let bm_a_txn, bm_b_txn, bm_c_txn;
  let results = await PT.batch([
    folder_a_txn,
    previousResults => {
      bm_a_info.parentGuid = previousResults[0];
      return (bm_a_txn = PT.NewBookmark(bm_a_info));
    },
    previousResults => {
      bm_b_info.parentGuid = previousResults[0];
      return (bm_b_txn = PT.NewBookmark(bm_b_info));
    },
    previousResults => {
      bm_c_info.parentGuid = previousResults[0];
      return (bm_c_txn = PT.NewBookmark(bm_c_info));
    },
  ]);

  folder_a_info.guid = results[0];
  bm_a_info.guid = results[1];
  bm_b_info.guid = results[2];
  bm_c_info.guid = results[3];

  ensureUndoState([[bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]], 0);

  let moveTxn = PT.Move({
    guids: [bm_a_info.guid, bm_b_info.guid],
    newParentGuid: folder_a_info.guid,
  });
  await moveTxn.transact();

  let ensureDo = () => {
    ensureUndoState(
      [[moveTxn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
      0
    );
    ensureItemsMoved(
      {
        guid: bm_a_info.guid,
        oldParentGuid: folder_a_info.guid,
        newParentGuid: folder_a_info.guid,
        oldIndex: 0,
        newIndex: 2,
      },
      {
        guid: bm_b_info.guid,
        oldParentGuid: folder_a_info.guid,
        newParentGuid: folder_a_info.guid,
        oldIndex: 1,
        newIndex: 2,
      }
    );
    observer.reset();
  };
  let ensureUndo = () => {
    ensureUndoState(
      [[moveTxn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
      1
    );
    ensureItemsMoved(
      {
        guid: bm_a_info.guid,
        oldParentGuid: folder_a_info.guid,
        newParentGuid: folder_a_info.guid,
        oldIndex: 1,
        newIndex: 0,
      },
      {
        guid: bm_b_info.guid,
        oldParentGuid: folder_a_info.guid,
        newParentGuid: folder_a_info.guid,
        oldIndex: 2,
        newIndex: 1,
      }
    );
    observer.reset();
  };

  ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo();
  ensureDo();
  await PT.undo();
  ensureUndo();

  await PT.clearTransactionsHistory(false, true);
  ensureUndoState([[bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]], 0);

  // Test moving items between folders.
  let folder_b_info = createTestFolderInfo("Folder B");
  let folder_b_txn = PT.NewFolder(folder_b_info);
  folder_b_info.guid = await folder_b_txn.transact();
  ensureUndoState(
    [[folder_b_txn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
    0
  );

  moveTxn = PT.Move({
    guid: bm_a_info.guid,
    newParentGuid: folder_b_info.guid,
    newIndex: bmsvc.DEFAULT_INDEX,
  });
  await moveTxn.transact();

  ensureDo = () => {
    ensureUndoState(
      [[moveTxn], [folder_b_txn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
      0
    );
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_a_info.guid,
      newParentGuid: folder_b_info.guid,
      oldIndex: 0,
      newIndex: 0,
    });
    observer.reset();
  };
  ensureUndo = () => {
    ensureUndoState(
      [[moveTxn], [folder_b_txn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
      1
    );
    ensureItemsMoved({
      guid: bm_a_info.guid,
      oldParentGuid: folder_b_info.guid,
      newParentGuid: folder_a_info.guid,
      oldIndex: 0,
      newIndex: 0,
    });
    observer.reset();
  };

  ensureDo();
  await PT.undo();
  ensureUndo();
  await PT.redo();
  ensureDo();
  await PT.undo();
  ensureUndo();

  // Clean up
  await PT.undo(); // folder_b_txn
  await PT.undo(); // folder_a_txn + the bookmarks;
  Assert.equal(observer.itemsRemoved.size, 5);
  ensureUndoState(
    [[moveTxn], [folder_b_txn], [bm_c_txn, bm_b_txn, bm_a_txn, folder_a_txn]],
    3
  );
  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_remove_folder() {
  let folder_level_1_info = createTestFolderInfo("Folder Level 1");
  let folder_level_2_info = { title: "Folder Level 2" };

  let folder_level_1_txn = PT.NewFolder(folder_level_1_info);
  let folder_level_2_txn;
  let results = await PT.batch([
    folder_level_1_txn,
    previousResults => {
      folder_level_2_info.parentGuid = previousResults[0];
      return (folder_level_2_txn = PT.NewFolder(folder_level_2_info));
    },
  ]);
  folder_level_1_info.guid = results[0];
  folder_level_2_info.guid = results[1];

  let original_folder_level_1_tree = await PlacesUtils.promiseBookmarksTree(
    folder_level_1_info.guid
  );
  let original_folder_level_2_tree = Object.assign(
    { parentGuid: original_folder_level_1_tree.guid },
    original_folder_level_1_tree.children[0]
  );

  ensureUndoState([[folder_level_2_txn, folder_level_1_txn]]);
  await ensureItemsAdded(folder_level_1_info, folder_level_2_info);
  observer.reset();

  let remove_folder_2_txn = PT.Remove(folder_level_2_info);
  await remove_folder_2_txn.transact();

  ensureUndoState([
    [remove_folder_2_txn],
    [folder_level_2_txn, folder_level_1_txn],
  ]);
  await ensureItemsRemoved(folder_level_2_info);

  // Undo Remove "Folder Level 2"
  await PT.undo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    1
  );
  await ensureItemsAdded(folder_level_2_info);
  await ensureBookmarksTreeRestoredCorrectly(original_folder_level_2_tree);
  observer.reset();

  // Redo Remove "Folder Level 2"
  await PT.redo();
  ensureUndoState([
    [remove_folder_2_txn],
    [folder_level_2_txn, folder_level_1_txn],
  ]);
  await ensureItemsRemoved(folder_level_2_info);
  observer.reset();

  // Undo it again
  await PT.undo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    1
  );
  await ensureItemsAdded(folder_level_2_info);
  await ensureBookmarksTreeRestoredCorrectly(original_folder_level_2_tree);
  observer.reset();

  // Undo the creation of both folders
  await PT.undo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    2
  );
  await ensureItemsRemoved(folder_level_2_info, folder_level_1_info);
  observer.reset();

  // Redo the creation of both folders
  await PT.redo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    1
  );
  await ensureItemsAdded(folder_level_1_info, folder_level_2_info);
  await ensureBookmarksTreeRestoredCorrectlyExceptDates(
    original_folder_level_1_tree
  );
  observer.reset();

  // Redo Remove "Folder Level 2"
  await PT.redo();
  ensureUndoState([
    [remove_folder_2_txn],
    [folder_level_2_txn, folder_level_1_txn],
  ]);
  await ensureItemsRemoved(folder_level_2_info);
  observer.reset();

  // Undo everything one last time
  await PT.undo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    1
  );
  await ensureItemsAdded(folder_level_2_info);
  observer.reset();

  await PT.undo();
  ensureUndoState(
    [[remove_folder_2_txn], [folder_level_2_txn, folder_level_1_txn]],
    2
  );
  await ensureItemsRemoved(folder_level_2_info, folder_level_2_info);
  observer.reset();

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_add_and_remove_bookmarks_with_additional_info() {
  const testURI = "http://add.remove.tag";
  const TAG_1 = "TestTag1";
  const TAG_2 = "TestTag2";

  let folder_info = createTestFolderInfo();
  folder_info.guid = await PT.NewFolder(folder_info).transact();
  let ensureTags = ensureTagsForURI.bind(null, testURI);

  // Check that the NewBookmark transaction preserves tags.
  observer.reset();
  let b1_info = { parentGuid: folder_info.guid, url: testURI, tags: [TAG_1] };
  b1_info.guid = await PT.NewBookmark(b1_info).transact();
  let b1_originalInfo = await PlacesUtils.promiseBookmarksTree(b1_info.guid);
  ensureTags([TAG_1]);
  await PT.undo();
  ensureTags([]);

  observer.reset();
  await PT.redo();
  await ensureBookmarksTreeRestoredCorrectly(b1_originalInfo);
  ensureTags([TAG_1]);

  // Check if the Remove transaction removes and restores tags of children
  // correctly.
  await PT.Remove(folder_info.guid).transact();
  ensureTags([]);

  observer.reset();
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(b1_originalInfo);
  ensureTags([TAG_1]);

  await PT.redo();
  ensureTags([]);

  observer.reset();
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(b1_originalInfo);
  ensureTags([TAG_1]);

  // * Check that no-op tagging (the uri is already tagged with TAG_1) is
  //   also a no-op on undo.
  observer.reset();
  let b2_info = {
    parentGuid: folder_info.guid,
    url: testURI,
    tags: [TAG_1, TAG_2],
  };
  b2_info.guid = await PT.NewBookmark(b2_info).transact();
  ensureTags([TAG_1, TAG_2]);

  observer.reset();
  await PT.undo();
  await ensureItemsRemoved(b2_info);
  ensureTags([TAG_1]);

  // Check if Remove correctly restores tags.
  observer.reset();
  await PT.redo();
  ensureTags([TAG_1, TAG_2]);

  // Test Remove for multiple items.
  observer.reset();
  await PT.Remove(b1_info.guid).transact();
  await PT.Remove(b2_info.guid).transact();
  await PT.Remove(folder_info.guid).transact();
  await ensureItemsRemoved(b1_info, b2_info, folder_info);
  ensureTags([]);

  observer.reset();
  await PT.undo();
  await ensureItemsAdded(folder_info);
  ensureTags([]);

  observer.reset();
  await PT.undo();
  ensureTags([TAG_1, TAG_2]);

  observer.reset();
  await PT.undo();
  await ensureItemsAdded(b1_info);
  ensureTags([TAG_1, TAG_2]);

  // The redo calls below cleanup everything we did.
  observer.reset();
  await PT.redo();
  await ensureItemsRemoved(b1_info);
  ensureTags([TAG_1, TAG_2]);

  observer.reset();
  await PT.redo();
  // The tag containers are removed in async and take some time
  let oldCountTag1 = 0;
  let oldCountTag2 = 0;
  let allTags = await bmsvc.fetchTags();
  for (let i of allTags) {
    if (i.name == TAG_1) {
      oldCountTag1 = i.count;
    }
    if (i.name == TAG_2) {
      oldCountTag2 = i.count;
    }
  }
  await TestUtils.waitForCondition(async () => {
    allTags = await bmsvc.fetchTags();
    let newCountTag1 = 0;
    let newCountTag2 = 0;
    for (let i of allTags) {
      if (i.name == TAG_1) {
        newCountTag1 = i.count;
      }
      if (i.name == TAG_2) {
        newCountTag2 = i.count;
      }
    }
    return newCountTag1 == oldCountTag1 - 1 && newCountTag2 == oldCountTag2 - 1;
  });
  await ensureItemsRemoved(b2_info);

  ensureTags([]);

  observer.reset();
  await PT.redo();
  await ensureItemsRemoved(folder_info);
  ensureTags([]);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_creating_and_removing_a_separator() {
  let folder_info = createTestFolderInfo();
  let separator_info = {};
  let undoEntries = [];

  observer.reset();
  let folder_txn = PT.NewFolder(folder_info);
  let separator_txn;
  let results = await PT.batch([
    folder_txn,
    previousResults => {
      separator_info.parentGuid = previousResults[0];
      return (separator_txn = PT.NewSeparator(separator_info));
    },
  ]);
  folder_info.guid = results[0];
  separator_info.guid = results[1];

  undoEntries.unshift([separator_txn, folder_txn]);
  ensureUndoState(undoEntries, 0);
  ensureItemsAdded(folder_info, separator_info);

  observer.reset();
  await PT.undo();
  ensureUndoState(undoEntries, 1);
  ensureItemsRemoved(folder_info, separator_info);

  observer.reset();
  await PT.redo();
  ensureUndoState(undoEntries, 0);
  ensureItemsAdded(folder_info, separator_info);

  observer.reset();
  let remove_sep_txn = PT.Remove(separator_info);
  await remove_sep_txn.transact();
  undoEntries.unshift([remove_sep_txn]);
  ensureUndoState(undoEntries, 0);
  ensureItemsRemoved(separator_info);

  observer.reset();
  await PT.undo();
  ensureUndoState(undoEntries, 1);
  ensureItemsAdded(separator_info);

  observer.reset();
  await PT.undo();
  ensureUndoState(undoEntries, 2);
  ensureItemsRemoved(folder_info, separator_info);

  observer.reset();
  await PT.redo();
  ensureUndoState(undoEntries, 1);
  ensureItemsAdded(folder_info, separator_info);

  // Clear redo entries and check that |redo| does nothing
  observer.reset();
  await PT.clearTransactionsHistory(false, true);
  undoEntries.shift();
  ensureUndoState(undoEntries, 0);
  await PT.redo();
  ensureItemsAdded();
  ensureItemsRemoved();

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureUndoState(undoEntries, 1);
  ensureItemsRemoved(folder_info, separator_info);
  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_edit_title() {
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test_create_item.com",
    title: "Original Title",
  };

  function ensureTitleChange(aCurrentTitle) {
    ensureItemsTitleChanged({
      guid: bm_info.guid,
      title: aCurrentTitle,
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditTitle({ guid: bm_info.guid, title: "New Title" }).transact();
  ensureTitleChange("New Title");

  observer.reset();
  await PT.undo();
  ensureTitleChange("Original Title");

  observer.reset();
  await PT.redo();
  ensureTitleChange("New Title");

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureTitleChange("Original Title");
  await PT.undo();
  ensureItemsRemoved(bm_info);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_edit_url() {
  let oldURI = "http://old.test_editing_item_uri.com/";
  let newURI = "http://new.test_editing_item_uri.com/";
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: oldURI,
    tags: ["TestTag"],
  };
  function ensureURIAndTags(
    aPreChangeURI,
    aPostChangeURI,
    aOLdURITagsPreserved
  ) {
    ensureItemsUrlChanged({
      guid: bm_info.guid,
      url: aPostChangeURI,
    });
    ensureTagsForURI(aPostChangeURI, bm_info.tags);
    ensureTagsForURI(aPreChangeURI, aOLdURITagsPreserved ? bm_info.tags : []);
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  ensureTagsForURI(oldURI, bm_info.tags);

  // When there's a single bookmark for the same url, tags should be moved.
  observer.reset();
  await PT.EditUrl({ guid: bm_info.guid, url: newURI }).transact();
  ensureURIAndTags(oldURI, newURI, false);

  observer.reset();
  await PT.undo();
  ensureURIAndTags(newURI, oldURI, false);

  observer.reset();
  await PT.redo();
  ensureURIAndTags(oldURI, newURI, false);

  observer.reset();
  await PT.undo();
  ensureURIAndTags(newURI, oldURI, false);

  // When there're multiple bookmarks for the same url, tags should be copied.
  let bm2_info = Object.create(bm_info);
  bm2_info.guid = await PT.NewBookmark(bm2_info).transact();
  let bm3_info = Object.create(bm_info);
  bm3_info.url = newURI;
  bm3_info.guid = await PT.NewBookmark(bm3_info).transact();

  observer.reset();
  await PT.EditUrl({ guid: bm_info.guid, url: newURI }).transact();
  ensureURIAndTags(oldURI, newURI, true);

  observer.reset();
  await PT.undo();
  ensureURIAndTags(newURI, oldURI, true);

  observer.reset();
  await PT.redo();
  ensureURIAndTags(oldURI, newURI, true);

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureURIAndTags(newURI, oldURI, true);
  await PT.undo();
  await PT.undo();
  await PT.undo();
  ensureItemsRemoved(bm3_info, bm2_info, bm_info);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_edit_keyword() {
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test.edit.keyword/",
  };
  const KEYWORD = "test_keyword";
  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  function ensureKeywordChange(aCurrentKeyword = "") {
    ensureItemsKeywordChanged({
      guid: bm_info.guid,
      keyword: aCurrentKeyword,
    });
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditKeyword({
    guid: bm_info.guid,
    keyword: KEYWORD,
    postData: "postData",
  }).transact();
  ensureKeywordChange(KEYWORD);
  let entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData");

  observer.reset();
  await PT.undo();
  ensureKeywordChange();
  entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry, null);

  observer.reset();
  await PT.redo();
  ensureKeywordChange(KEYWORD);
  entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData");

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureKeywordChange();
  await PT.undo();
  ensureItemsRemoved(bm_info);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_edit_keyword_null_postData() {
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test.edit.keyword/",
  };
  const KEYWORD = "test_keyword";
  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  function ensureKeywordChange(aCurrentKeyword = "") {
    ensureItemsKeywordChanged({
      guid: bm_info.guid,
      keyword: aCurrentKeyword,
    });
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditKeyword({
    guid: bm_info.guid,
    keyword: KEYWORD,
    postData: null,
  }).transact();
  ensureKeywordChange(KEYWORD);
  let entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, null);

  observer.reset();
  await PT.undo();
  ensureKeywordChange();
  entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry, null);

  observer.reset();
  await PT.redo();
  ensureKeywordChange(KEYWORD);
  entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, null);

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureKeywordChange();
  await PT.undo();
  ensureItemsRemoved(bm_info);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_edit_specific_keyword() {
  let bm_info = {
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://test.edit.keyword/",
  };
  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  function ensureKeywordChange(aCurrentKeyword = "", aPreviousKeyword = "") {
    ensureItemsKeywordChanged({
      guid: bm_info.guid,
      keyword: aCurrentKeyword,
    });
  }

  await PlacesUtils.keywords.insert({
    keyword: "kw1",
    url: bm_info.url,
    postData: "postData1",
  });
  await PlacesUtils.keywords.insert({
    keyword: "kw2",
    url: bm_info.url,
    postData: "postData2",
  });
  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditKeyword({
    guid: bm_info.guid,
    keyword: "keyword",
    oldKeyword: "kw2",
  }).transact();
  ensureKeywordChange("keyword", "kw2");
  let entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData2");
  entry = await PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry, null);

  observer.reset();
  await PT.undo();
  ensureKeywordChange("kw2", "keyword");
  entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData2");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry, null);

  observer.reset();
  await PT.redo();
  ensureKeywordChange("keyword", "kw2");
  entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry.url.href, bm_info.url);
  Assert.equal(entry.postData, "postData2");
  entry = await PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry, null);

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureKeywordChange("kw2");
  await PT.undo();
  ensureItemsRemoved(bm_info);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_tag_uri() {
  // This also tests passing uri specs.
  let bm_info_a = {
    url: "http://bookmarked.uri",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  };
  let bm_info_b = {
    url: "http://bookmarked2.uri",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  };
  let unbookmarked_uri = "http://un.bookmarked.uri";

  let results = await PT.batch([
    PT.NewBookmark(bm_info_a),
    PT.NewBookmark(bm_info_b),
  ]);
  bm_info_a.guid = results[0];
  bm_info_b.guid = results[1];

  async function doTest(aInfo) {
    let urls = "url" in aInfo ? [aInfo.url] : aInfo.urls;
    let tags = "tag" in aInfo ? [aInfo.tag] : aInfo.tags;

    let tagWillAlsoBookmark = new Set();
    for (let url of urls) {
      if (!(await bmsvc.fetch({ url }))) {
        tagWillAlsoBookmark.add(url);
      }
    }

    async function ensureTagsSet() {
      for (let url of urls) {
        ensureTagsForURI(url, tags);
        Assert.ok(await bmsvc.fetch({ url }));
      }
    }
    async function ensureTagsUnset() {
      for (let url of urls) {
        ensureTagsForURI(url, []);
        if (tagWillAlsoBookmark.has(url)) {
          Assert.ok(!(await bmsvc.fetch({ url })));
        } else {
          Assert.ok(await bmsvc.fetch({ url }));
        }
      }
    }

    await PT.Tag(aInfo).transact();
    await ensureTagsSet();
    await PT.undo();
    await ensureTagsUnset();
    await PT.redo();
    await ensureTagsSet();
    await PT.undo();
    await ensureTagsUnset();
  }

  await doTest({ url: bm_info_a.url, tags: ["MyTag"] });
  await doTest({ urls: [bm_info_a.url], tag: "MyTag" });
  await doTest({ urls: [bm_info_a.url, bm_info_b.url], tags: ["A, B"] });
  await doTest({ urls: [bm_info_a.url, unbookmarked_uri], tag: "C" });
  // Duplicate URLs listed.
  await doTest({
    urls: [bm_info_a.url, bm_info_b.url, bm_info_a.url],
    tag: "D",
  });

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureItemsRemoved(bm_info_a, bm_info_b);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_untag_uri() {
  let bm_info_a = {
    url: "http://bookmarked.uri",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    tags: ["A", "B"],
  };
  let bm_info_b = {
    url: "http://bookmarked2.uri",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    tag: "B",
  };

  let results = await PT.batch([
    PT.NewBookmark(bm_info_a),
    PT.NewBookmark(bm_info_b),
  ]);
  ensureTagsForURI(bm_info_a.url, bm_info_a.tags);
  ensureTagsForURI(bm_info_b.url, [bm_info_b.tag]);
  bm_info_a.guid = results[0];

  bm_info_b.guid = results[1];

  async function doTest(aInfo) {
    let urls, tagsRemoved;
    if (typeof aInfo == "string") {
      urls = [aInfo];
      tagsRemoved = [];
    } else if (Array.isArray(aInfo)) {
      urls = aInfo;
      tagsRemoved = [];
    } else {
      urls = "url" in aInfo ? [aInfo.url] : aInfo.urls;
      tagsRemoved = "tag" in aInfo ? [aInfo.tag] : aInfo.tags;
    }

    let preRemovalTags = new Map();
    for (let url of urls) {
      preRemovalTags.set(url, tagssvc.getTagsForURI(Services.io.newURI(url)));
    }

    function ensureTagsSet() {
      for (let url of urls) {
        ensureTagsForURI(url, preRemovalTags.get(url));
      }
    }
    function ensureTagsUnset() {
      for (let url of urls) {
        let expectedTags = !tagsRemoved.length
          ? []
          : preRemovalTags.get(url).filter(tag => !tagsRemoved.includes(tag));
        ensureTagsForURI(url, expectedTags);
      }
    }

    await PT.Untag(aInfo).transact();
    await ensureTagsUnset();
    await PT.undo();
    await ensureTagsSet();
    await PT.redo();
    await ensureTagsUnset();
    await PT.undo();
    await ensureTagsSet();
  }

  await doTest(bm_info_a);
  await doTest(bm_info_b);
  await doTest(bm_info_a.url);
  await doTest(bm_info_b.url);
  await doTest([bm_info_a.url, bm_info_b.url]);
  await doTest({ urls: [bm_info_a.url, bm_info_b.url], tags: ["A", "B"] });
  await doTest({ urls: [bm_info_a.url, bm_info_b.url], tag: "B" });
  await doTest({ urls: [bm_info_a.url, bm_info_b.url], tag: "C" });
  await doTest({ urls: [bm_info_a.url, bm_info_b.url], tags: ["C"] });

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureItemsRemoved(bm_info_a, bm_info_b);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_sort_folder_by_name() {
  let folder_info = createTestFolderInfo();

  let url = "http://sort.by.name/";
  let preSep = ["3", "2", "1"].map(i => ({ title: i, url }));
  let sep = {};
  let postSep = ["c", "b", "a"].map(l => ({ title: l, url }));
  let originalOrder = [...preSep, sep, ...postSep];
  let sortedOrder = [
    ...preSep.slice(0).reverse(),
    sep,
    ...postSep.slice(0).reverse(),
  ];

  let folder_txn = await PT.NewFolder(folder_info);
  let transactions = [folder_txn];
  for (let info of originalOrder) {
    transactions.push(previousResults => {
      info.parentGuid = previousResults[0];
      return info == sep ? PT.NewSeparator(info) : PT.NewBookmark(info);
    });
  }
  let results = await PT.batch(transactions);
  folder_info.guid = results[0];
  for (let i = 0; i < originalOrder.length; ++i) {
    originalOrder[i].guid = results[i + 1];
  }

  let folderContainer = PlacesUtils.getFolderContents(folder_info.guid).root;
  function ensureOrder(aOrder) {
    for (let i = 0; i < folderContainer.childCount; i++) {
      Assert.equal(folderContainer.getChild(i).bookmarkGuid, aOrder[i].guid);
    }
  }

  ensureOrder(originalOrder);
  await PT.SortByName(folder_info.guid).transact();
  ensureOrder(sortedOrder);
  await PT.undo();
  ensureOrder(originalOrder);
  await PT.redo();
  ensureOrder(sortedOrder);

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureOrder(originalOrder);
  await PT.undo();
  ensureItemsRemoved(...originalOrder, folder_info);
});

add_task(async function test_copy() {
  async function duplicate_and_test(aOriginalGuid) {
    let txn = PT.Copy({
      guid: aOriginalGuid,
      newParentGuid: PlacesUtils.bookmarks.unfiledGuid,
    });
    let duplicateGuid = await txn.transact();
    let originalInfo = await PlacesUtils.promiseBookmarksTree(aOriginalGuid);
    let duplicateInfo = await PlacesUtils.promiseBookmarksTree(duplicateGuid);
    await ensureEqualBookmarksTrees(originalInfo, duplicateInfo, false);

    async function redo() {
      await PT.redo();
      await ensureBookmarksTreeRestoredCorrectlyExceptDates(originalInfo);
      await PT.redo();
      await ensureBookmarksTreeRestoredCorrectlyExceptDates(duplicateInfo);
    }
    async function undo() {
      await PT.undo();
      // also undo the original item addition.
      await PT.undo();
      await ensureNonExistent(aOriginalGuid, duplicateGuid);
    }

    await undo();
    await redo();
    await undo();
    await redo();

    // Cleanup. This also remove the original item.
    await PT.undo();
    observer.reset();
    await PT.clearTransactionsHistory();
  }

  let timerPrecision = Preferences.get("privacy.reduceTimerPrecision");
  Preferences.set("privacy.reduceTimerPrecision", false);

  registerCleanupFunction(function () {
    Preferences.set("privacy.reduceTimerPrecision", timerPrecision);
  });

  // Test duplicating leafs (bookmark, separator, empty folder)
  PT.NewBookmark({
    url: "http://test.item.duplicate",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    annos: [{ name: "Anno", value: "AnnoValue" }],
  });
  let sepTxn = PT.NewSeparator({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    index: 1,
  });

  let emptyFolderTxn = PT.NewFolder(createTestFolderInfo());
  for (let txn of [sepTxn, emptyFolderTxn]) {
    let guid = await txn.transact();
    await duplicate_and_test(guid);
  }

  // Test duplicating a folder having some contents.
  let results = await PT.batch([
    PT.NewFolder(createTestFolderInfo()),
    previousResults =>
      PT.NewFolder({
        parentGuid: previousResults[0],
        title: "Nested Folder",
      }),
    // Insert a bookmark under the nested folder.
    previousResults =>
      PT.NewBookmark({
        url: "http://nested.nested.bookmark",
        parentGuid: previousResults[1],
      }),
    // Insert a separator below the nested folder
    previousResults => PT.NewSeparator({ parentGuid: previousResults[0] }),
    // And another bookmark.
    previousResults =>
      PT.NewBookmark({
        url: "http://nested.bookmark",
        parentGuid: previousResults[0],
      }),
  ]);
  let filledFolderGuid = results[0];

  await duplicate_and_test(filledFolderGuid);

  // Cleanup
  await PT.clearTransactionsHistory();
});

add_task(async function test_array_input_for_batch() {
  let folderTxn = PT.NewFolder(createTestFolderInfo());
  let folderGuid = await folderTxn.transact();

  let sep1_txn = PT.NewSeparator({ parentGuid: folderGuid });
  let sep2_txn = PT.NewSeparator({ parentGuid: folderGuid });
  await PT.batch([sep1_txn, sep2_txn]);
  ensureUndoState([[sep2_txn, sep1_txn], [folderTxn]], 0);

  let ensureChildCount = async function (count) {
    let tree = await PlacesUtils.promiseBookmarksTree(folderGuid);
    if (count == 0) {
      Assert.ok(!("children" in tree));
    } else {
      Assert.equal(tree.children.length, count);
    }
  };

  await ensureChildCount(2);
  await PT.undo();
  await ensureChildCount(0);
  await PT.redo();
  await ensureChildCount(2);
  await PT.undo();
  await ensureChildCount(0);

  await PT.undo();
  Assert.equal(await PlacesUtils.promiseBookmarksTree(folderGuid), null);

  // Cleanup
  await PT.clearTransactionsHistory();
});

add_task(async function test_invalid_uri_spec_throws() {
  Assert.throws(
    () =>
      PT.NewBookmark({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: "invalid uri spec",
        title: "test bookmark",
      }),
    /invalid uri spec is not a valid URL/
  );
  Assert.throws(
    () => PT.Tag({ tag: "TheTag", urls: ["invalid uri spec"] }),
    /TypeError: URL constructor: invalid uri spec is not a valid URL/
  );
  Assert.throws(
    () => PT.Tag({ tag: "TheTag", urls: ["about:blank", "invalid uri spec"] }),
    /TypeError: URL constructor: invalid uri spec is not a valid URL/
  );
});

add_task(async function test_remove_multiple() {
  let results = await PT.batch([
    PT.NewFolder({
      title: "Test Folder",
      parentGuid: menuGuid,
    }),
    previousResults =>
      PT.NewFolder({
        title: "Nested Test Folder",
        parentGuid: previousResults[0],
      }),
    previousResults => PT.NewSeparator(previousResults[1]),
    PT.NewBookmark({
      url: "http://test.bookmark.removed",
      parentGuid: menuGuid,
    }),
  ]);
  let guids = [results[0], results[3]];

  let originalInfos = [];
  for (let guid of guids) {
    originalInfos.push(await PlacesUtils.promiseBookmarksTree(guid));
  }

  await PT.Remove(guids).transact();
  await ensureNonExistent(...guids);
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(...originalInfos);
  await PT.redo();
  await ensureNonExistent(...guids);
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(...originalInfos);

  // Undo the New* transactions batch.
  await PT.undo();
  await ensureNonExistent(...guids);

  // Redo it.
  await PT.redo();
  await ensureBookmarksTreeRestoredCorrectlyExceptDates(...originalInfos);

  // Redo remove.
  await PT.redo();
  await ensureNonExistent(...guids);

  // Cleanup
  await PT.clearTransactionsHistory();
  observer.reset();
});

add_task(async function test_renameTag() {
  let url = "http://test.edit.keyword/";
  await PT.Tag({ url, tags: ["t1", "t2"] }).transact();
  ensureTagsForURI(url, ["t1", "t2"]);

  // Create bookmark queries that point to the modified tag.
  let bm1 = await PlacesUtils.bookmarks.insert({
    url: "place:tag=t2",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  let bm2 = await PlacesUtils.bookmarks.insert({
    url: "place:tag=t2&sort=1",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  // This points to 2 tags, and as such won't be touched.
  let bm3 = await PlacesUtils.bookmarks.insert({
    url: "place:tag=t2&tag=t1",
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });

  await PT.RenameTag({ oldTag: "t2", tag: "t3" }).transact();
  ensureTagsForURI(url, ["t1", "t3"]);
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm1.guid)).url.href,
    "place:tag=t3",
    "The fitst bookmark has been updated"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm2.guid)).url.href,
    "place:tag=t3&sort=1",
    "The second bookmark has been updated"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm3.guid)).url.href,
    "place:tag=t3&tag=t1",
    "The third bookmark has been updated"
  );

  await PT.undo();
  ensureTagsForURI(url, ["t1", "t2"]);
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm1.guid)).url.href,
    "place:tag=t2",
    "The fitst bookmark has been restored"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm2.guid)).url.href,
    "place:tag=t2&sort=1",
    "The second bookmark has been restored"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm3.guid)).url.href,
    "place:tag=t2&tag=t1",
    "The third bookmark has been restored"
  );

  await PT.redo();
  ensureTagsForURI(url, ["t1", "t3"]);
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm1.guid)).url.href,
    "place:tag=t3",
    "The fitst bookmark has been updated"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm2.guid)).url.href,
    "place:tag=t3&sort=1",
    "The second bookmark has been updated"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm3.guid)).url.href,
    "place:tag=t3&tag=t1",
    "The third bookmark has been updated"
  );

  await PT.undo();
  ensureTagsForURI(url, ["t1", "t2"]);
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm1.guid)).url.href,
    "place:tag=t2",
    "The fitst bookmark has been restored"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm2.guid)).url.href,
    "place:tag=t2&sort=1",
    "The second bookmark has been restored"
  );
  Assert.equal(
    (await PlacesUtils.bookmarks.fetch(bm3.guid)).url.href,
    "place:tag=t2&tag=t1",
    "The third bookmark has been restored"
  );

  await PT.undo();
  ensureTagsForURI(url, []);

  await PT.clearTransactionsHistory();
  ensureUndoState();
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_remove_invalid_url() {
  let folderGuid = await PT.NewFolder({
    title: "Test Folder",
    parentGuid: menuGuid,
  }).transact();

  let guid = "invalid_____";
  let folderedGuid = "invalid____2";
  let url = "invalid-uri";
  await PlacesUtils.withConnectionWrapper("test_bookmarks_remove", async db => {
    await db.execute(
      `
      INSERT INTO moz_places(url, url_hash, title, rev_host, guid)
      VALUES (:url, hash(:url), 'Invalid URI', '.', GENERATE_GUID())
    `,
      { url }
    );
    await db.execute(
      `INSERT INTO moz_bookmarks (type, fk, parent, position, guid)
        VALUES (:type,
                (SELECT id FROM moz_places WHERE url = :url),
                (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
                (SELECT MAX(position) + 1 FROM moz_bookmarks WHERE parent = (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid)),
                :guid)
      `,
      {
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url,
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        guid,
      }
    );
    await db.execute(
      `INSERT INTO moz_bookmarks (type, fk, parent, position, guid)
        VALUES (:type,
                (SELECT id FROM moz_places WHERE url = :url),
                (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid),
                (SELECT MAX(position) + 1 FROM moz_bookmarks WHERE parent = (SELECT id FROM moz_bookmarks WHERE guid = :parentGuid)),
                :guid)
      `,
      {
        type: PlacesUtils.bookmarks.TYPE_BOOKMARK,
        url,
        parentGuid: folderGuid,
        guid: folderedGuid,
      }
    );
  });

  let guids = [folderGuid, guid];
  await PT.Remove(guids).transact();
  await ensureNonExistent(...guids, folderedGuid);
  // Shouldn't throw, should restore the folder but not the bookmarks.
  await PT.undo();
  await ensureNonExistent(guid, folderedGuid);
  Assert.ok(
    await PlacesUtils.bookmarks.fetch(folderGuid),
    "The folder should have been re-created"
  );
  await PT.redo();
  await ensureNonExistent(guids, folderedGuid);
  // Cleanup
  await PT.clearTransactionsHistory();
  observer.reset();
});
