/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const bmsvc    = PlacesUtils.bookmarks;
const tagssvc  = PlacesUtils.tagging;
const annosvc  = PlacesUtils.annotations;
const PT       = PlacesTransactions;
const rootGuid = PlacesUtils.bookmarks.rootGuid;

Components.utils.importGlobalProperties(["URL"]);

// Create and add bookmarks observer.
var observer = {
  __proto__: NavBookmarkObserver.prototype,

  tagRelatedGuids: new Set(),

  reset() {
    this.itemsAdded = new Map();
    this.itemsRemoved = new Map();
    this.itemsChanged = new Map();
    this.itemsMoved = new Map();
    this.beginUpdateBatch = false;
    this.endUpdateBatch = false;
  },

  onBeginUpdateBatch() {
    this.beginUpdateBatch = true;
  },

  onEndUpdateBatch() {
    this.endUpdateBatch = true;
  },

  onItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded,
           aGuid, aParentGuid) {
    // Ignore tag items.
    if (aParentId == PlacesUtils.tagsFolderId ||
        (aParentId != PlacesUtils.placesRootId &&
         bmsvc.getFolderIdForItem(aParentId) == PlacesUtils.tagsFolderId)) {
      this.tagRelatedGuids.add(aGuid);
      return;
    }

    this.itemsAdded.set(aGuid, { itemId:         aItemId,
                                 parentGuid:     aParentGuid,
                                 index:          aIndex,
                                 itemType:       aItemType,
                                 title:          aTitle,
                                 url:            aURI });
  },

  onItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGuid, aParentGuid) {
    if (this.tagRelatedGuids.has(aGuid))
      return;

    this.itemsRemoved.set(aGuid, { parentGuid: aParentGuid,
                                   index:      aIndex,
                                   itemType:   aItemType });
  },

  onItemChanged(aItemId, aProperty, aIsAnnoProperty, aNewValue, aLastModified,
           aItemType, aParentId, aGuid, aParentGuid) {
    if (this.tagRelatedGuids.has(aGuid))
      return;

    let changesForGuid = this.itemsChanged.get(aGuid);
    if (changesForGuid === undefined) {
      changesForGuid = new Map();
      this.itemsChanged.set(aGuid, changesForGuid);
    }

    let newValue = aNewValue;
    if (aIsAnnoProperty) {
      if (annosvc.itemHasAnnotation(aItemId, aProperty))
        newValue = annosvc.getItemAnnotation(aItemId, aProperty);
      else
        newValue = null;
    }
    let change = { isAnnoProperty: aIsAnnoProperty,
                   newValue,
                   lastModified: aLastModified,
                   itemType: aItemType };
    changesForGuid.set(aProperty, change);
  },

  onItemVisited: () => {},

  onItemMoved(aItemId, aOldParent, aOldIndex, aNewParent, aNewIndex, aItemType,
           aGuid, aOldParentGuid, aNewParentGuid) {
    this.itemsMoved.set(aGuid, { oldParentGuid: aOldParentGuid,
                                 oldIndex:      aOldIndex,
                                 newParentGuid: aNewParentGuid,
                                 newIndex:      aNewIndex,
                                 itemType:      aItemType });
  }
};
observer.reset();

// index at which items should begin
var bmStartIndex = 0;

function run_test() {
  bmsvc.addObserver(observer);
  do_register_cleanup(function() {
    bmsvc.removeObserver(observer);
  });

  run_next_test();
}

function sanityCheckTransactionHistory() {
  do_check_true(PT.undoPosition <= PT.length);

  let check_entry_throws = f => {
    try {
      f();
      do_throw("PT.entry should throw for invalid input");
    } catch (ex) {}
  };
  check_entry_throws( () => PT.entry(-1) );
  check_entry_throws( () => PT.entry({}) );
  check_entry_throws( () => PT.entry(PT.length) );

  if (PT.undoPosition < PT.length)
    do_check_eq(PT.topUndoEntry, PT.entry(PT.undoPosition));
  else
    do_check_null(PT.topUndoEntry);
  if (PT.undoPosition > 0)
    do_check_eq(PT.topRedoEntry, PT.entry(PT.undoPosition - 1));
  else
    do_check_null(PT.topRedoEntry);
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
  do_check_eq(actualEntries.length, aExpectedEntries.length);
  do_check_eq(actualUndoPosition, aExpectedUndoPosition);

  function checkEqualEntries(aExpectedEntry, aActualEntry) {
    do_check_eq(aExpectedEntry.length, aActualEntry.length);
    aExpectedEntry.forEach( (t, i) => do_check_eq(t, aActualEntry[i]) );
  }
  aExpectedEntries.forEach( (e, i) => checkEqualEntries(e, actualEntries[i]) );
}

function ensureItemsAdded(...items) {
  Assert.equal(observer.itemsAdded.size, items.length);
  for (let item of items) {
    Assert.ok(observer.itemsAdded.has(item.guid));
    let info = observer.itemsAdded.get(item.guid);
    Assert.equal(info.parentGuid, item.parentGuid);
    for (let propName of ["title", "index", "itemType"]) {
      if (propName in item)
        Assert.equal(info[propName], item[propName]);
    }
    if ("url" in item)
      Assert.ok(info.url.equals(item.url));
  }
}

function ensureItemsRemoved(...items) {
  Assert.equal(observer.itemsRemoved.size, items.length);
  for (let item of items) {
    // We accept both guids and full info object here.
    if (typeof(item) == "string") {
      Assert.ok(observer.itemsRemoved.has(item));
    } else {
      Assert.ok(observer.itemsRemoved.has(item.guid));
      let info = observer.itemsRemoved.get(item.guid);
      Assert.equal(info.parentGuid, item.parentGuid);
      if ("index" in item)
        Assert.equal(info.index, item.index);
    }
  }
}

function ensureItemsChanged(...items) {
  for (let item of items) {
    do_check_true(observer.itemsChanged.has(item.guid));
    let changes = observer.itemsChanged.get(item.guid);
    do_check_true(changes.has(item.property));
    let info = changes.get(item.property);
    if (!("isAnnoProperty" in item)) {
      do_check_false(info.isAnnoProperty);
    } else {
      do_check_eq(info.isAnnoProperty, Boolean(item.isAnnoProperty));
    }
    do_check_eq(info.newValue, item.newValue);
    if ("url" in item)
      do_check_true(item.url.equals(info.url));
  }
}

function ensureAnnotationsSet(aGuid, aAnnos) {
  do_check_true(observer.itemsChanged.has(aGuid));
  let changes = observer.itemsChanged.get(aGuid);
  for (let anno of aAnnos) {
    do_check_true(changes.has(anno.name));
    let changeInfo = changes.get(anno.name);
    do_check_true(changeInfo.isAnnoProperty);
    do_check_eq(changeInfo.newValue, anno.value);
  }
}

function ensureItemsMoved(...items) {
  do_check_true(observer.itemsMoved.size, items.length);
  for (let item of items) {
    do_check_true(observer.itemsMoved.has(item.guid));
    let info = observer.itemsMoved.get(item.guid);
    do_check_eq(info.oldParentGuid, item.oldParentGuid);
    do_check_eq(info.oldIndex, item.oldIndex);
    do_check_eq(info.newParentGuid, item.newParentGuid);
    do_check_eq(info.newIndex, item.newIndex);
  }
}

function ensureTimestampsUpdated(aGuid, aCheckDateAdded = false) {
  do_check_true(observer.itemsChanged.has(aGuid));
  let changes = observer.itemsChanged.get(aGuid);
  if (aCheckDateAdded)
    do_check_true(changes.has("dateAdded"))
  do_check_true(changes.has("lastModified"));
}

function ensureTagsForURI(aURI, aTags) {
  let tagsSet = tagssvc.getTagsForURI(aURI);
  do_check_eq(tagsSet.length, aTags.length);
  do_check_true(aTags.every( t => tagsSet.includes(t)));
}

function createTestFolderInfo(aTitle = "Test Folder") {
  return { parentGuid: rootGuid, title: aTitle };
}

function isLivemarkTree(aTree) {
  return !!aTree.annos &&
         aTree.annos.some( a => a.name == PlacesUtils.LMANNO_FEEDURI );
}

async function ensureLivemarkCreatedByAddLivemark(aLivemarkGuid) {
  // This throws otherwise.
  await PlacesUtils.livemarks.getLivemark({ guid: aLivemarkGuid });
}

// Checks if two bookmark trees (as returned by promiseBookmarksTree) are the
// same.
// false value for aCheckParentAndPosition is ignored if aIsRestoredItem is set.
async function ensureEqualBookmarksTrees(aOriginal,
                                    aNew,
                                    aIsRestoredItem = true,
                                    aCheckParentAndPosition = false) {
  // Note "id" is not-enumerable, and is therefore skipped by Object.keys (both
  // ours and the one at deepEqual). This is fine for us because ids are not
  // restored by Redo.
  if (aIsRestoredItem) {
    Assert.deepEqual(aOriginal, aNew);
    if (isLivemarkTree(aNew))
      await ensureLivemarkCreatedByAddLivemark(aNew.guid);
    return;
  }

  for (let property of Object.keys(aOriginal)) {
    if (property == "children") {
      Assert.equal(aOriginal.children.length, aNew.children.length);
      for (let i = 0; i < aOriginal.children.length; i++) {
        await ensureEqualBookmarksTrees(aOriginal.children[i],
                                        aNew.children[i],
                                        false,
                                        true);
      }
    } else if (property == "guid") {
      // guid shouldn't be copied if the item was not restored.
      Assert.notEqual(aOriginal.guid, aNew.guid);
    } else if (property == "dateAdded") {
      // dateAdded shouldn't be copied if the item was not restored.
      Assert.ok(is_time_ordered(aOriginal.dateAdded, aNew.dateAdded));
    } else if (property == "lastModified") {
      // same same, except for the never-changed case
      if (!aOriginal.lastModified)
        Assert.ok(!aNew.lastModified);
      else
        Assert.ok(is_time_ordered(aOriginal.lastModified, aNew.lastModified));
    } else if (aCheckParentAndPosition ||
             (property != "parentGuid" && property != "index")) {
      Assert.deepEqual(aOriginal[property], aNew[property]);
    }
  }

  if (isLivemarkTree(aNew))
    await ensureLivemarkCreatedByAddLivemark(aNew.guid);
}

async function ensureBookmarksTreeRestoredCorrectly(...aOriginalBookmarksTrees) {
  for (let originalTree of aOriginalBookmarksTrees) {
    let restoredTree =
      await PlacesUtils.promiseBookmarksTree(originalTree.guid);
    await ensureEqualBookmarksTrees(originalTree, restoredTree);
  }
}

async function ensureNonExistent(...aGuids) {
  for (let guid of aGuids) {
    Assert.strictEqual((await PlacesUtils.promiseBookmarksTree(guid)), null);
  }
}

add_task(async function test_recycled_transactions() {
  async function ensureTransactThrowsFor(aTransaction) {
    let [txns, undoPosition] = getTransactionsHistoryState();
    try {
      await aTransaction.transact();
      do_throw("Shouldn't be able to use the same transaction twice");
    } catch (ex) { }
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
  await PT.batch(async function() {
    try {
      await txn_a.transact();
      do_throw("Shouldn't be able to use the same transaction twice");
    } catch (ex) { }
    ensureUndoState();
    await txn_b.transact();
  });
  ensureUndoState([[txn_b]], 0);

  await PT.undo();
  ensureUndoState([[txn_b]], 1);
  ensureTransactThrowsFor(txn_a);
  ensureTransactThrowsFor(txn_b);

  await PT.clearTransactionsHistory();
  ensureUndoState();
  observer.reset();
});

add_task(async function test_new_folder_with_annotation() {
  const ANNO = { name: "TestAnno", value: "TestValue" };
  let folder_info = createTestFolderInfo();
  folder_info.index = bmStartIndex;
  folder_info.annotations = [ANNO];
  ensureUndoState();
  let txn = PT.NewFolder(folder_info);
  folder_info.guid = await txn.transact();
  let ensureDo = async function(aRedo = false) {
    ensureUndoState([[txn]], 0);
    await ensureItemsAdded(folder_info);
    ensureAnnotationsSet(folder_info.guid, [ANNO]);
    if (aRedo)
      ensureTimestampsUpdated(folder_info.guid, true);
    observer.reset();
  };

  let ensureUndo = () => {
    ensureUndoState([[txn]], 1);
    ensureItemsRemoved({ guid:       folder_info.guid,
                         parentGuid: folder_info.parentGuid,
                         index:      bmStartIndex });
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
  let bm_info = { parentGuid: rootGuid,
                  url:        NetUtil.newURI("http://test_create_item.com"),
                  index:      bmStartIndex,
                  title:      "Test creating an item" };

  ensureUndoState();
  let txn = PT.NewBookmark(bm_info);
  bm_info.guid = await txn.transact();

  let ensureDo = async function(aRedo = false) {
    ensureUndoState([[txn]], 0);
    await ensureItemsAdded(bm_info);
    if (aRedo)
      ensureTimestampsUpdated(bm_info.guid, true);
    observer.reset();
  };
  let ensureUndo = () => {
    ensureUndoState([[txn]], 1);
    ensureItemsRemoved({ guid:       bm_info.guid,
                         parentGuid: bm_info.parentGuid,
                         index:      bmStartIndex });
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
  let bm_info = { url: NetUtil.newURI("http://test_create_item_to_folder.com"),
                  title: "Test Bookmark",
                  index: bmStartIndex };

  let [folderTxnResult, bkmTxnResult] = await PT.batch(async function() {
    let folderTxn = PT.NewFolder(folder_info);
    folder_info.guid = bm_info.parentGuid = await folderTxn.transact();
    let bkmTxn = PT.NewBookmark(bm_info);
    bm_info.guid = await bkmTxn.transact();
    return [folderTxn, bkmTxn];
  });

  let ensureDo = async function() {
    ensureUndoState([[bkmTxnResult, folderTxnResult]], 0);
    await ensureItemsAdded(folder_info, bm_info);
    observer.reset();
  };

  let ensureUndo = () => {
    ensureUndoState([[bkmTxnResult, folderTxnResult]], 1);
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
  let bkm_a_info = { url: new URL("http://test_move_items.com"),
                     title: "Bookmark A" };
  let bkm_b_info = { url: NetUtil.newURI("http://test_move_items.com"),
                     title: "Bookmark B" };

  // Test moving items within the same folder.
  let [folder_a_txn_result, bkm_a_txn_result, bkm_b_txn_result] = await PT.batch(async function() {
    let folder_a_txn = PT.NewFolder(folder_a_info);

    folder_a_info.guid = bkm_a_info.parentGuid = bkm_b_info.parentGuid =
      await folder_a_txn.transact();
    let bkm_a_txn = PT.NewBookmark(bkm_a_info);
    bkm_a_info.guid = await bkm_a_txn.transact();
    let bkm_b_txn = PT.NewBookmark(bkm_b_info);
    bkm_b_info.guid = await bkm_b_txn.transact();
    return [folder_a_txn, bkm_a_txn, bkm_b_txn];
  });

  ensureUndoState([[bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result]], 0);

  let moveTxn = PT.Move({ guid:          bkm_a_info.guid,
                          newParentGuid: folder_a_info.guid });
  await moveTxn.transact();

  let ensureDo = () => {
    ensureUndoState([[moveTxn], [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result]], 0);
    ensureItemsMoved({ guid:          bkm_a_info.guid,
                       oldParentGuid: folder_a_info.guid,
                       newParentGuid: folder_a_info.guid,
                       oldIndex:      0,
                       newIndex:      1 });
    observer.reset();
  };
  let ensureUndo = () => {
    ensureUndoState([[moveTxn], [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result]], 1);
    ensureItemsMoved({ guid:          bkm_a_info.guid,
                       oldParentGuid: folder_a_info.guid,
                       newParentGuid: folder_a_info.guid,
                       oldIndex:      1,
                       newIndex:      0 });
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
  ensureUndoState([[bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result]], 0);

  // Test moving items between folders.
  let folder_b_info = createTestFolderInfo("Folder B");
  let folder_b_txn = PT.NewFolder(folder_b_info);
  folder_b_info.guid = await folder_b_txn.transact();
  ensureUndoState([ [folder_b_txn],
                    [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result] ], 0);

  moveTxn = PT.Move({ guid:          bkm_a_info.guid,
                      newParentGuid: folder_b_info.guid,
                      newIndex:      bmsvc.DEFAULT_INDEX });
  await moveTxn.transact();

  ensureDo = () => {
    ensureUndoState([ [moveTxn],
                      [folder_b_txn],
                      [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result] ], 0);
    ensureItemsMoved({ guid:          bkm_a_info.guid,
                       oldParentGuid: folder_a_info.guid,
                       newParentGuid: folder_b_info.guid,
                       oldIndex:      0,
                       newIndex:      0 });
    observer.reset();
  };
  ensureUndo = () => {
    ensureUndoState([ [moveTxn],
                      [folder_b_txn],
                      [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result] ], 1);
    ensureItemsMoved({ guid:          bkm_a_info.guid,
                       oldParentGuid: folder_b_info.guid,
                       newParentGuid: folder_a_info.guid,
                       oldIndex:      0,
                       newIndex:      0 });
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
  await PT.undo();  // folder_b_txn
  await PT.undo();  // folder_a_txn + the bookmarks;
  do_check_eq(observer.itemsRemoved.size, 4);
  ensureUndoState([ [moveTxn],
                    [folder_b_txn],
                    [bkm_b_txn_result, bkm_a_txn_result, folder_a_txn_result] ], 3);
  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_remove_folder() {
  let folder_level_1_info = createTestFolderInfo("Folder Level 1");
  let folder_level_2_info = { title: "Folder Level 2" };
  let [folder_level_1_txn_result,
       folder_level_2_txn_result] = await PT.batch(async function() {
    let folder_level_1_txn  = PT.NewFolder(folder_level_1_info);
    folder_level_1_info.guid = await folder_level_1_txn.transact();
    folder_level_2_info.parentGuid = folder_level_1_info.guid;
    let folder_level_2_txn = PT.NewFolder(folder_level_2_info);
    folder_level_2_info.guid = await folder_level_2_txn.transact();
    return [folder_level_1_txn, folder_level_2_txn];
  });

  ensureUndoState([[folder_level_2_txn_result, folder_level_1_txn_result]]);
  await ensureItemsAdded(folder_level_1_info, folder_level_2_info);
  observer.reset();

  let remove_folder_2_txn = PT.Remove(folder_level_2_info);
  await remove_folder_2_txn.transact();

  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ]);
  await ensureItemsRemoved(folder_level_2_info);

  // Undo Remove "Folder Level 2"
  await PT.undo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 1);
  await ensureItemsAdded(folder_level_2_info);
  ensureTimestampsUpdated(folder_level_2_info.guid, true);
  observer.reset();

  // Redo Remove "Folder Level 2"
  await PT.redo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ]);
  await ensureItemsRemoved(folder_level_2_info);
  observer.reset();

  // Undo it again
  await PT.undo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 1);
  await ensureItemsAdded(folder_level_2_info);
  ensureTimestampsUpdated(folder_level_2_info.guid, true);
  observer.reset();

  // Undo the creation of both folders
  await PT.undo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 2);
  await ensureItemsRemoved(folder_level_2_info, folder_level_1_info);
  observer.reset();

  // Redo the creation of both folders
  await PT.redo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 1);
  await ensureItemsAdded(folder_level_1_info, folder_level_2_info);
  ensureTimestampsUpdated(folder_level_1_info.guid, true);
  ensureTimestampsUpdated(folder_level_2_info.guid, true);
  observer.reset();

  // Redo Remove "Folder Level 2"
  await PT.redo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ]);
  await ensureItemsRemoved(folder_level_2_info);
  observer.reset();

  // Undo everything one last time
  await PT.undo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 1);
  await ensureItemsAdded(folder_level_2_info);
  observer.reset();

  await PT.undo();
  ensureUndoState([ [remove_folder_2_txn],
                    [folder_level_2_txn_result, folder_level_1_txn_result] ], 2);
  await ensureItemsRemoved(folder_level_2_info, folder_level_2_info);
  observer.reset();

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_add_and_remove_bookmarks_with_additional_info() {
  const testURI = NetUtil.newURI("http://add.remove.tag");
  const TAG_1 = "TestTag1";
  const TAG_2 = "TestTag2";
  const KEYWORD = "test_keyword";
  const POST_DATA = "post_data";
  const ANNO = { name: "TestAnno", value: "TestAnnoValue" };

  let folder_info = createTestFolderInfo();
  folder_info.guid = await PT.NewFolder(folder_info).transact();
  let ensureTags = ensureTagsForURI.bind(null, testURI);

  // Check that the NewBookmark transaction preserves tags.
  observer.reset();
  let b1_info = { parentGuid: folder_info.guid, url: testURI, tags: [TAG_1] };
  b1_info.guid = await PT.NewBookmark(b1_info).transact();
  ensureTags([TAG_1]);
  await PT.undo();
  ensureTags([]);

  observer.reset();
  await PT.redo();
  ensureTimestampsUpdated(b1_info.guid, true);
  ensureTags([TAG_1]);

  // Check if the Remove transaction removes and restores tags of children
  // correctly.
  await PT.Remove(folder_info.guid).transact();
  ensureTags([]);

  observer.reset();
  await PT.undo();
  ensureTimestampsUpdated(b1_info.guid, true);
  ensureTags([TAG_1]);

  await PT.redo();
  ensureTags([]);

  observer.reset();
  await PT.undo();
  ensureTimestampsUpdated(b1_info.guid, true);
  ensureTags([TAG_1]);

  // * Check that no-op tagging (the uri is already tagged with TAG_1) is
  //   also a no-op on undo.
  // * Test the "keyword" property of the NewBookmark transaction.
  observer.reset();
  let b2_info = { parentGuid:  folder_info.guid,
                  url:         testURI, tags: [TAG_1, TAG_2],
                  keyword:     KEYWORD,
                  postData:    POST_DATA,
                  annotations: [ANNO] };
  b2_info.guid = await PT.NewBookmark(b2_info).transact();
  let b2_post_creation_changes = [
   { guid: b2_info.guid,
     isAnnoProperty: true,
     property: ANNO.name,
     newValue: ANNO.value },
   { guid: b2_info.guid,
     property: "keyword",
     newValue: KEYWORD } ];
  ensureItemsChanged(...b2_post_creation_changes);
  ensureTags([TAG_1, TAG_2]);

  observer.reset();
  await PT.undo();
  await ensureItemsRemoved(b2_info);
  ensureTags([TAG_1]);

  // Check if Remove correctly restores keywords, tags and annotations.
  // Since both bookmarks share the same uri, they also share the keyword that
  // is not removed along with one of the bookmarks.
  observer.reset();
  await PT.redo();
  ensureItemsChanged({ guid: b2_info.guid,
                       isAnnoProperty: true,
                       property: ANNO.name,
                       newValue: ANNO.value });
  ensureTags([TAG_1, TAG_2]);

  // Test Remove for multiple items.
  observer.reset();
  await PT.Remove(b1_info.guid).transact();
  await PT.Remove(b2_info.guid).transact();
  await PT.Remove(folder_info.guid).transact();
  await ensureItemsRemoved(b1_info, b2_info, folder_info);
  ensureTags([]);
  // There is no keyword removal notification cause all bookmarks are removed
  // before the keyword itself, so there's no one to notify.
  let entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry, null, "keyword has been removed");

  observer.reset();
  await PT.undo();
  await ensureItemsAdded(folder_info);
  ensureTags([]);

  observer.reset();
  await PT.undo();
  ensureItemsChanged(...b2_post_creation_changes);
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
  let create_txns = await PT.batch(async function() {
    let folder_txn = PT.NewFolder(folder_info);
    folder_info.guid = separator_info.parentGuid = await folder_txn.transact();
    let separator_txn = PT.NewSeparator(separator_info);
    separator_info.guid = await separator_txn.transact();
    return [separator_txn, folder_txn];
  });
  undoEntries.unshift(create_txns);
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

add_task(async function test_add_and_remove_livemark() {
  let createLivemarkTxn = PT.NewLivemark(
    { feedUrl: NetUtil.newURI("http://test.remove.livemark"),
      parentGuid: rootGuid,
      title: "Test Remove Livemark" });
  let guid = await createLivemarkTxn.transact();
  let originalInfo = await PlacesUtils.promiseBookmarksTree(guid);
  Assert.ok(originalInfo);
  await ensureLivemarkCreatedByAddLivemark(guid);

  let removeTxn = PT.Remove(guid);
  await removeTxn.transact();
  await ensureNonExistent(guid);
  async function undo() {
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 0);
    await PT.undo();
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 1);
    await ensureBookmarksTreeRestoredCorrectly(originalInfo);
    await PT.undo();
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 2);
    await ensureNonExistent(guid);
  }
  async function redo() {
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 2);
    await PT.redo();
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 1);
    await ensureBookmarksTreeRestoredCorrectly(originalInfo);
    await PT.redo();
    ensureUndoState([[removeTxn], [createLivemarkTxn]], 0);
    await ensureNonExistent(guid);
  }

  await undo();
  await redo();
  await undo();
  await redo();

  // Cleanup
  await undo();
  observer.reset();
  await PT.clearTransactionsHistory();
});

add_task(async function test_edit_title() {
  let bm_info = { parentGuid: rootGuid,
                  url:        NetUtil.newURI("http://test_create_item.com"),
                  title:      "Original Title" };

  function ensureTitleChange(aCurrentTitle) {
    ensureItemsChanged({ guid: bm_info.guid,
                         property: "title",
                         newValue: aCurrentTitle});
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
  let oldURI = NetUtil.newURI("http://old.test_editing_item_uri.com/");
  let newURI = NetUtil.newURI("http://new.test_editing_item_uri.com/");
  let bm_info = { parentGuid: rootGuid, url: oldURI, tags: ["TestTag"] };
  function ensureURIAndTags(aPreChangeURI, aPostChangeURI, aOLdURITagsPreserved) {
    ensureItemsChanged({ guid: bm_info.guid,
                         property: "uri",
                         newValue: aPostChangeURI.spec });
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
  let bm_info = { parentGuid: rootGuid,
                  url: NetUtil.newURI("http://test.edit.keyword") };
  const KEYWORD = "test_keyword";
  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  function ensureKeywordChange(aCurrentKeyword = "") {
    ensureItemsChanged({ guid: bm_info.guid,
                         property: "keyword",
                         newValue: aCurrentKeyword });
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditKeyword({ guid: bm_info.guid, keyword: KEYWORD, postData: "postData" }).transact();
  ensureKeywordChange(KEYWORD);
  let entry = await PlacesUtils.keywords.fetch(KEYWORD);
  Assert.equal(entry.url.href, bm_info.url.spec);
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
  Assert.equal(entry.url.href, bm_info.url.spec);
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

add_task(async function test_edit_specific_keyword() {
  let bm_info = { parentGuid: rootGuid,
                  url: NetUtil.newURI("http://test.edit.keyword") };
  bm_info.guid = await PT.NewBookmark(bm_info).transact();
  function ensureKeywordChange(aCurrentKeyword = "", aPreviousKeyword = "") {
    ensureItemsChanged({ guid: bm_info.guid,
                         property: "keyword",
                         newValue: aCurrentKeyword
                       });
  }

  await PlacesUtils.keywords.insert({ keyword: "kw1", url: bm_info.url.spec, postData: "postData1" });
  await PlacesUtils.keywords.insert({ keyword: "kw2", url: bm_info.url.spec, postData: "postData2" });
  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.EditKeyword({ guid: bm_info.guid, keyword: "keyword", oldKeyword: "kw2" }).transact();
  ensureKeywordChange("keyword", "kw2");
  let entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url.spec);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry.url.href, bm_info.url.spec);
  Assert.equal(entry.postData, "postData2");
  entry = await PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry, null);

  observer.reset();
  await PT.undo();
  ensureKeywordChange("kw2", "keyword");
  entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url.spec);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("kw2");
  Assert.equal(entry.url.href, bm_info.url.spec);
  Assert.equal(entry.postData, "postData2");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry, null);

  observer.reset();
  await PT.redo();
  ensureKeywordChange("keyword", "kw2");
  entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, bm_info.url.spec);
  Assert.equal(entry.postData, "postData1");
  entry = await PlacesUtils.keywords.fetch("keyword");
  Assert.equal(entry.url.href, bm_info.url.spec);
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
  let bm_info_a = { url: "http://bookmarked.uri",
                    parentGuid: rootGuid };
  let bm_info_b = { url: NetUtil.newURI("http://bookmarked2.uri"),
                    parentGuid: rootGuid };
  let unbookmarked_uri = NetUtil.newURI("http://un.bookmarked.uri");

  await PT.batch(async function() {
    bm_info_a.guid = await PT.NewBookmark(bm_info_a).transact();
    bm_info_b.guid = await PT.NewBookmark(bm_info_b).transact();
  });

  async function doTest(aInfo) {
    let urls = "url" in aInfo ? [aInfo.url] : aInfo.urls;
    let tags = "tag" in aInfo ? [aInfo.tag] : aInfo.tags;

    let ensureURI = url => typeof(url) == "string" ? NetUtil.newURI(url) : url;
    urls = urls.map(ensureURI);

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
        if (tagWillAlsoBookmark.has(url))
          Assert.ok(!(await bmsvc.fetch({ url })));
        else
          Assert.ok(await bmsvc.fetch({ url }));
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

  // Cleanup
  observer.reset();
  await PT.undo();
  ensureItemsRemoved(bm_info_a, bm_info_b);

  await PT.clearTransactionsHistory();
  ensureUndoState();
});

add_task(async function test_untag_uri() {
  let bm_info_a = { url: NetUtil.newURI("http://bookmarked.uri"),
                    parentGuid: rootGuid,
                    tags: ["A", "B"] };
  let bm_info_b = { url: NetUtil.newURI("http://bookmarked2.uri"),
                    parentGuid: rootGuid,
                    tag: "B" };

  await PT.batch(async function() {
    bm_info_a.guid = await PT.NewBookmark(bm_info_a).transact();
    ensureTagsForURI(bm_info_a.url, bm_info_a.tags);
    bm_info_b.guid = await PT.NewBookmark(bm_info_b).transact();
    ensureTagsForURI(bm_info_b.url, [bm_info_b.tag]);
  });

  async function doTest(aInfo) {
    let urls, tagsRemoved;
    if (aInfo instanceof Ci.nsIURI) {
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
      preRemovalTags.set(url, tagssvc.getTagsForURI(url));
    }

    function ensureTagsSet() {
      for (let url of urls) {
        ensureTagsForURI(url, preRemovalTags.get(url));
      }
    }
    function ensureTagsUnset() {
      for (let url of urls) {
        let expectedTags = tagsRemoved.length == 0 ?
           [] : preRemovalTags.get(url).filter(tag => !tagsRemoved.includes(tag));
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

add_task(async function test_annotate() {
  let bm_info = { url: NetUtil.newURI("http://test.item.annotation"),
                  parentGuid: rootGuid };
  let anno_info = { name: "TestAnno", value: "TestValue" };
  function ensureAnnoState(aSet) {
    ensureAnnotationsSet(bm_info.guid,
                         [{ name: anno_info.name,
                            value: aSet ? anno_info.value : null }]);
  }

  bm_info.guid = await PT.NewBookmark(bm_info).transact();

  observer.reset();
  await PT.Annotate({ guid: bm_info.guid, annotation: anno_info }).transact();
  ensureAnnoState(true);

  observer.reset();
  await PT.undo();
  ensureAnnoState(false);

  observer.reset();
  await PT.redo();
  ensureAnnoState(true);

  // Test removing the annotation by not passing the |value| property.
  observer.reset();
  await PT.Annotate({ guid: bm_info.guid,
                      annotation: { name: anno_info.name }}).transact();
  ensureAnnoState(false);

  observer.reset();
  await PT.undo();
  ensureAnnoState(true);

  observer.reset();
  await PT.redo();
  ensureAnnoState(false);

  // Cleanup
  await PT.undo();
  observer.reset();
});

add_task(async function test_annotate_multiple() {
  let guid = await PT.NewFolder(createTestFolderInfo()).transact();
  let itemId = await PlacesUtils.promiseItemId(guid);

  function AnnoObj(aName, aValue) {
    this.name = aName;
    this.value = aValue;
    this.flags = 0;
    this.expires = Ci.nsIAnnotationService.EXPIRE_NEVER;
  }

  function annos(a = null, b = null) {
    return [new AnnoObj("A", a), new AnnoObj("B", b)];
  }

  function verifyAnnoValues(a = null, b = null) {
    let currentAnnos = PlacesUtils.getAnnotationsForItem(itemId);
    let expectedAnnos = [];
    if (a !== null)
      expectedAnnos.push(new AnnoObj("A", a));
    if (b !== null)
      expectedAnnos.push(new AnnoObj("B", b));

    Assert.deepEqual(currentAnnos, expectedAnnos);
  }

  await PT.Annotate({ guid, annotations: annos(1, 2) }).transact();
  verifyAnnoValues(1, 2);
  await PT.undo();
  verifyAnnoValues();
  await PT.redo();
  verifyAnnoValues(1, 2);

  await PT.Annotate({ guid,
                      annotation: { name: "A" } }).transact();
  verifyAnnoValues(null, 2);

  await PT.Annotate({ guid,
                      annotation: { name: "B", value: 0 } }).transact();
  verifyAnnoValues(null, 0);
  await PT.undo();
  verifyAnnoValues(null, 2);
  await PT.redo();
  verifyAnnoValues(null, 0);
  await PT.undo();
  verifyAnnoValues(null, 2);
  await PT.undo();
  verifyAnnoValues(1, 2);
  await PT.undo();
  verifyAnnoValues();

  // Cleanup
  await PT.undo();
  observer.reset();
});

add_task(async function test_sort_folder_by_name() {
  let folder_info = createTestFolderInfo();

  let url = NetUtil.newURI("http://sort.by.name/");
  let preSep =  ["3", "2", "1"].map(i => ({ title: i, url }));
  let sep = {};
  let postSep = ["c", "b", "a"].map(l => ({ title: l, url }));
  let originalOrder = [...preSep, sep, ...postSep];
  let sortedOrder = [...preSep.slice(0).reverse(),
                     sep,
                     ...postSep.slice(0).reverse()];
  await PT.batch(async function() {
    folder_info.guid = await PT.NewFolder(folder_info).transact();
    for (let info of originalOrder) {
      info.parentGuid = folder_info.guid;
      info.guid = await (info == sep ?
                    PT.NewSeparator(info).transact() :
                    PT.NewBookmark(info).transact());
    }
  });

  let folderId = await PlacesUtils.promiseItemId(folder_info.guid);
  let folderContainer = PlacesUtils.getFolderContents(folderId).root;
  function ensureOrder(aOrder) {
    for (let i = 0; i < folderContainer.childCount; i++) {
      do_check_eq(folderContainer.getChild(i).bookmarkGuid, aOrder[i].guid);
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

add_task(async function test_livemark_txns() {
  let livemark_info =
    { feedUrl: NetUtil.newURI("http://test.feed.uri"),
      parentGuid: rootGuid,
      title: "Test Livemark" };
  function ensureLivemarkAdded() {
    ensureItemsAdded({ guid:       livemark_info.guid,
                       title:      livemark_info.title,
                       parentGuid: livemark_info.parentGuid,
                       itemType:   bmsvc.TYPE_FOLDER });
    let annos = [{ name:  PlacesUtils.LMANNO_FEEDURI,
                   value: livemark_info.feedUrl.spec }];
    if ("siteUrl" in livemark_info) {
      annos.push({ name: PlacesUtils.LMANNO_SITEURI,
                   value: livemark_info.siteUrl.spec });
    }
    ensureAnnotationsSet(livemark_info.guid, annos);
  }
  function ensureLivemarkRemoved() {
    ensureItemsRemoved({ guid:       livemark_info.guid,
                         parentGuid: livemark_info.parentGuid });
  }

  async function _testDoUndoRedoUndo() {
    observer.reset();
    livemark_info.guid = await PT.NewLivemark(livemark_info).transact();
    ensureLivemarkAdded();

    observer.reset();
    await PT.undo();
    ensureLivemarkRemoved();

    observer.reset();
    await PT.redo();
    ensureLivemarkAdded();

    await PT.undo();
    ensureLivemarkRemoved();
  }

  await _testDoUndoRedoUndo()
  livemark_info.siteUrl = NetUtil.newURI("http://feed.site.uri");
  await _testDoUndoRedoUndo();

  // Cleanup
  observer.reset();
  await PT.clearTransactionsHistory();
});

add_task(async function test_copy() {
  async function duplicate_and_test(aOriginalGuid) {
    let txn = PT.Copy({ guid: aOriginalGuid, newParentGuid: rootGuid });
    let duplicateGuid = await txn.transact();
    let originalInfo = await PlacesUtils.promiseBookmarksTree(aOriginalGuid);
    let duplicateInfo = await PlacesUtils.promiseBookmarksTree(duplicateGuid);
    await ensureEqualBookmarksTrees(originalInfo, duplicateInfo, false);

    async function redo() {
      await PT.redo();
      await ensureBookmarksTreeRestoredCorrectly(originalInfo);
      await PT.redo();
      await ensureBookmarksTreeRestoredCorrectly(duplicateInfo);
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

  // Test duplicating leafs (bookmark, separator, empty folder)
  PT.NewBookmark({ url: new URL("http://test.item.duplicate"),
                   parentGuid: rootGuid,
                   annos: [{ name: "Anno", value: "AnnoValue"}] });
  let sepTxn = PT.NewSeparator({ parentGuid: rootGuid, index: 1 });
  let livemarkTxn = PT.NewLivemark(
    { feedUrl: new URL("http://test.feed.uri"),
      parentGuid: rootGuid,
      title: "Test Livemark", index: 1 });
  let emptyFolderTxn = PT.NewFolder(createTestFolderInfo());
  for (let txn of [livemarkTxn, sepTxn, emptyFolderTxn]) {
    let guid = await txn.transact();
    await duplicate_and_test(guid);
  }

  // Test duplicating a folder having some contents.
  let filledFolderGuid = await PT.batch(async function() {
    let folderGuid = await PT.NewFolder(createTestFolderInfo()).transact();
    let nestedFolderGuid =
      await PT.NewFolder({ parentGuid: folderGuid,
                           title: "Nested Folder" }).transact();
    // Insert a bookmark under the nested folder.
    await PT.NewBookmark({ url: new URL("http://nested.nested.bookmark"),
                           parentGuid: nestedFolderGuid }).transact();
    // Insert a separator below the nested folder
    await PT.NewSeparator({ parentGuid: folderGuid }).transact();
    // And another bookmark.
    await PT.NewBookmark({ url: new URL("http://nested.bookmark"),
                           parentGuid: folderGuid }).transact();
    return folderGuid;
  });

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

  let ensureChildCount = async function(count) {
    let tree = await PlacesUtils.promiseBookmarksTree(folderGuid);
    if (count == 0)
      Assert.ok(!("children" in tree));
    else
      Assert.equal(tree.children.length, count);
  };

  await ensureChildCount(2);
  await PT.undo();
  await ensureChildCount(0);
  await PT.redo()
  await ensureChildCount(2);
  await PT.undo();
  await ensureChildCount(0);

  await PT.undo();
  Assert.equal((await PlacesUtils.promiseBookmarksTree(folderGuid)), null);

  // Cleanup
  await PT.clearTransactionsHistory();
});

add_task(async function test_copy_excluding_annotations() {
  let folderInfo = createTestFolderInfo();
  let anno = n => { return { name: n, value: 1 } };
  folderInfo.annotations = [anno("a"), anno("b"), anno("c")];
  let folderGuid = await PT.NewFolder(folderInfo).transact();

  let ensureAnnosSet = async function(guid, ...expectedAnnoNames) {
    let tree = await PlacesUtils.promiseBookmarksTree(guid);
    let annoNames = "annos" in tree ?
                      tree.annos.map(a => a.name).sort() : [];
    Assert.deepEqual(annoNames, expectedAnnoNames);
  };

  await ensureAnnosSet(folderGuid, "a", "b", "c");

  let excluding_a_dupeGuid =
    await PT.Copy({ guid: folderGuid,
                    newParentGuid: rootGuid,
                    excludingAnnotation: "a" }).transact();
  await ensureAnnosSet(excluding_a_dupeGuid, "b", "c");

  let excluding_ac_dupeGuid =
    await PT.Copy({ guid: folderGuid,
                    newParentGuid: rootGuid,
                    excludingAnnotations: ["a", "c"] }).transact();
  await ensureAnnosSet(excluding_ac_dupeGuid, "b");

  // Cleanup
  await PT.undo();
  await PT.undo();
  await PT.undo();
  await PT.clearTransactionsHistory();
});

add_task(async function test_invalid_uri_spec_throws() {
  Assert.throws(() =>
    PT.NewBookmark({ parentGuid: rootGuid,
                     url:        "invalid uri spec",
                     title:      "test bookmark" }));
  Assert.throws(() =>
    PT.Tag({ tag: "TheTag",
             urls: ["invalid uri spec"] }));
  Assert.throws(() =>
    PT.Tag({ tag: "TheTag",
             urls: ["about:blank", "invalid uri spec"] }));
});

add_task(async function test_annotate_multiple_items() {
  let parentGuid = rootGuid;
  let guids = [
    await PT.NewBookmark({ url: "about:blank", parentGuid }).transact(),
    await PT.NewFolder({ title: "Test Folder", parentGuid }).transact()];

  let annotation = { name: "TestAnno", value: "TestValue" };
  await PT.Annotate({ guids, annotation }).transact();

  async function ensureAnnoSet() {
    for (let guid of guids) {
      let itemId = await PlacesUtils.promiseItemId(guid);
      Assert.equal(annosvc.getItemAnnotation(itemId, annotation.name),
                   annotation.value);
    }
  }
  async function ensureAnnoUnset() {
    for (let guid of guids) {
      let itemId = await PlacesUtils.promiseItemId(guid);
      Assert.ok(!annosvc.itemHasAnnotation(itemId, annotation.name));
    }
  }

  await ensureAnnoSet();
  await PT.undo();
  await ensureAnnoUnset();
  await PT.redo();
  await ensureAnnoSet();
  await PT.undo();
  await ensureAnnoUnset();

  // Cleanup
  await PT.undo();
  await PT.undo();
  await ensureNonExistent(...guids);
  await PT.clearTransactionsHistory();
  observer.reset();
});

add_task(async function test_remove_multiple() {
  let guids = [];
  await PT.batch(async function() {
    let folderGuid = await PT.NewFolder({ title: "Test Folder",
                                          parentGuid: rootGuid }).transact();
    let nestedFolderGuid =
      await PT.NewFolder({ title: "Nested Test Folder",
                           parentGuid: folderGuid }).transact();
    await PT.NewSeparator(nestedFolderGuid).transact();

    guids.push(folderGuid);

    let bmGuid =
      await PT.NewBookmark({ url: new URL("http://test.bookmark.removed"),
                             parentGuid: rootGuid }).transact();
    guids.push(bmGuid);
  });

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
  await ensureBookmarksTreeRestoredCorrectly(...originalInfos);

  // Redo remove.
  await PT.redo();
  await ensureNonExistent(...guids);

  // Cleanup
  await PT.clearTransactionsHistory();
  observer.reset();
});

add_task(async function test_remove_bookmarks_for_urls() {
  let urls = [new URL("http://test.url.1"), new URL("http://test.url.2")];
  let guids = [];
  await PT.batch(async function() {
    for (let url of urls) {
      for (let title of ["test title a", "test title b"]) {
        let txn = PT.NewBookmark({ url, title, parentGuid: rootGuid });
        guids.push(await txn.transact());
      }
    }
  });

  let originalInfos = [];
  for (let guid of guids) {
    originalInfos.push(await PlacesUtils.promiseBookmarksTree(guid));
  }

  await PT.RemoveBookmarksForUrls(urls).transact();
  await ensureNonExistent(...guids);
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(...originalInfos);
  await PT.redo();
  await ensureNonExistent(...guids);
  await PT.undo();
  await ensureBookmarksTreeRestoredCorrectly(...originalInfos);

  // Cleanup.
  await PT.redo();
  await ensureNonExistent(...guids);
  await PT.clearTransactionsHistory();
  observer.reset();
});
