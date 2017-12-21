/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var bmsvc = PlacesUtils.bookmarks;
var tagssvc = PlacesUtils.tagging;
var annosvc = PlacesUtils.annotations;
var txnManager = PlacesUtils.transactionManager;
const DESCRIPTION_ANNO = "bookmarkProperties/description";

async function promiseKeyword(keyword, href, postData) {
  while (true) {
    let entry = await PlacesUtils.keywords.fetch(keyword);
    if (href == null && !entry)
      break;
    if (entry && entry.url.href == href && entry.postData == postData) {
      break;
    }

    await new Promise(resolve => do_timeout(100, resolve));
  }
}

// create and add bookmarks observer
var observer = {

  onBeginUpdateBatch() {
    this._beginUpdateBatch = true;
  },
  _beginUpdateBatch: false,

  onEndUpdateBatch() {
    this._endUpdateBatch = true;
  },
  _endUpdateBatch: false,

  onItemAdded(id, folder, index, itemType, uri) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
    this._itemAddedType = itemType;
  },
  _itemAddedId: null,
  _itemAddedParent: null,
  _itemAddedIndex: null,
  _itemAddedType: null,

  onItemRemoved(id, folder, index, itemType) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  _itemRemovedId: null,
  _itemRemovedFolder: null,
  _itemRemovedIndex: null,

  onItemChanged(id, property, isAnnotationProperty, newValue,
                          lastModified, itemType) {
    // The transaction manager is being rewritten in bug 891303, so just
    // skip checking this for now.
    if (property == "tags")
      return;
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = newValue;
  },
  _itemChangedId: null,
  _itemChangedProperty: null,
  _itemChanged_isAnnotationProperty: null,
  _itemChangedValue: null,

  onItemVisited(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  _itemVisitedId: null,
  _itemVisitedVistId: null,
  _itemVisitedTime: null,

  onItemMoved(id, oldParent, oldIndex, newParent, newIndex,
                        itemType) {
    this._itemMovedId = id;
    this._itemMovedOldParent = oldParent;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewParent = newParent;
    this._itemMovedNewIndex = newIndex;
  },
  _itemMovedId: null,
  _itemMovedOldParent: null,
  _itemMovedOldIndex: null,
  _itemMovedNewParent: null,
  _itemMovedNewIndex: null,

  QueryInterface(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

// index at which items should begin
var bmStartIndex = 0;

// get bookmarks root id
var root = PlacesUtils.bookmarksMenuFolderId;

add_task(async function init() {
  bmsvc.addObserver(observer);
  do_register_cleanup(function() {
    bmsvc.removeObserver(observer);
  });
});

add_task(async function test_create_folder_with_description() {
  const TEST_FOLDERNAME = "Test creating a folder with a description";
  const TEST_DESCRIPTION = "this is my test description";

  let annos = [{ name: DESCRIPTION_ANNO,
                 type: annosvc.TYPE_STRING,
                 flags: 0,
                 value: TEST_DESCRIPTION,
                 expires: annosvc.EXPIRE_NEVER }];
  let txn = new PlacesCreateFolderTransaction(TEST_FOLDERNAME, root, bmStartIndex, annos);
  txnManager.doTransaction(txn);

  // This checks that calling undoTransaction on an "empty batch" doesn't
  // undo the previous transaction (getItemTitle will fail)
  txnManager.beginBatch(null);
  txnManager.endBatch(false);
  txnManager.undoTransaction();

  let folderId = observer._itemAddedId;
  Assert.equal(bmsvc.getItemTitle(folderId), TEST_FOLDERNAME);
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.equal(observer._itemAddedParent, root);
  Assert.equal(observer._itemAddedId, folderId);
  Assert.equal(TEST_DESCRIPTION, annosvc.getItemAnnotation(folderId, DESCRIPTION_ANNO));

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, folderId);
  Assert.equal(observer._itemRemovedFolder, root);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);

  txn.redoTransaction();
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.equal(observer._itemAddedParent, root);
  Assert.equal(observer._itemAddedId, folderId);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, folderId);
  Assert.equal(observer._itemRemovedFolder, root);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);
});

add_task(async function test_create_item() {
  let testURI = NetUtil.newURI("http://test_create_item.com");

  let txn = new PlacesCreateBookmarkTransaction(testURI, root, bmStartIndex,
                                                "Test creating an item");

  txnManager.doTransaction(txn);
  let id = bmsvc.getBookmarkIdsForURI(testURI)[0];
  Assert.equal(observer._itemAddedId, id);
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: testURI}), null);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, id);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);
  Assert.equal(await PlacesUtils.bookmarks.fetch({url: testURI}), null);

  txn.redoTransaction();
  Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: testURI}), null);
  let newId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.equal(observer._itemAddedParent, root);
  Assert.equal(observer._itemAddedId, newId);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, newId);
  Assert.equal(observer._itemRemovedFolder, root);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);
});

add_task(async function test_create_item_to_folder() {
  const TEST_FOLDERNAME = "Test creating item to a folder";
  let testURI = NetUtil.newURI("http://test_create_item_to_folder.com");
  let folderId = bmsvc.createFolder(root, TEST_FOLDERNAME, bmsvc.DEFAULT_INDEX);

  let txn = new PlacesCreateBookmarkTransaction(testURI, folderId, bmStartIndex,
                                                "Test creating item");
  txnManager.doTransaction(txn);
  let bkmId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  Assert.equal(observer._itemAddedId, bkmId);
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: testURI}), null);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, bkmId);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);

  txn.redoTransaction();
  let newBkmId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  Assert.equal(observer._itemAddedIndex, bmStartIndex);
  Assert.equal(observer._itemAddedParent, folderId);
  Assert.equal(observer._itemAddedId, newBkmId);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, newBkmId);
  Assert.equal(observer._itemRemovedFolder, folderId);
  Assert.equal(observer._itemRemovedIndex, bmStartIndex);
});

add_task(async function test_move_items_to_folder() {
  let testFolderId = bmsvc.createFolder(root, "Test move items", bmsvc.DEFAULT_INDEX);
  let testURI = NetUtil.newURI("http://test_move_items.com");
  let testBkmId = bmsvc.insertBookmark(testFolderId, testURI, bmsvc.DEFAULT_INDEX, "1: Test move items");
  bmsvc.insertBookmark(testFolderId, testURI, bmsvc.DEFAULT_INDEX, "2: Test move items");

  // Moving items between the same folder
  let sameTxn = new PlacesMoveItemTransaction(testBkmId, testFolderId, bmsvc.DEFAULT_INDEX);

  sameTxn.doTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 1);

  sameTxn.undoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 1);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 0);

  sameTxn.redoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 1);

  sameTxn.undoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 1);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 0);

  // Moving items between different folders
  let folderId = bmsvc.createFolder(testFolderId,
                                    "Test move items between different folders",
                                    bmsvc.DEFAULT_INDEX);
  let diffTxn = new PlacesMoveItemTransaction(testBkmId, folderId, bmsvc.DEFAULT_INDEX);

  diffTxn.doTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, folderId);
  Assert.equal(observer._itemMovedNewIndex, 0);

  sameTxn.undoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, folderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 0);

  diffTxn.redoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, testFolderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, folderId);
  Assert.equal(observer._itemMovedNewIndex, 0);

  sameTxn.undoTransaction();
  Assert.equal(observer._itemMovedId, testBkmId);
  Assert.equal(observer._itemMovedOldParent, folderId);
  Assert.equal(observer._itemMovedOldIndex, 0);
  Assert.equal(observer._itemMovedNewParent, testFolderId);
  Assert.equal(observer._itemMovedNewIndex, 0);
});

add_task(async function test_remove_folder() {
  let testFolder = bmsvc.createFolder(root, "Test Removing a Folder", bmsvc.DEFAULT_INDEX);
  let folderId = bmsvc.createFolder(testFolder, "Removed Folder", bmsvc.DEFAULT_INDEX);

  let txn = new PlacesRemoveItemTransaction(folderId);

  txn.doTransaction();
  Assert.equal(observer._itemRemovedId, folderId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedId, folderId);
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);

  txn.redoTransaction();
  Assert.equal(observer._itemRemovedId, folderId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedId, folderId);
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
});

add_task(async function test_remove_item_with_tag() {
  // Notice in this case the tag persists since other bookmarks have same uri.
  let testFolder = bmsvc.createFolder(root, "Test removing an item with a tag",
                                      bmsvc.DEFAULT_INDEX);

  const TAG_NAME = "tag-test_remove_item_with_tag";
  let testURI = NetUtil.newURI("http://test_remove_item_with_tag.com");
  let testBkmId = bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "test-item1");

  // create bookmark for not removing tag.
  bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "test-item2");

  // set tag
  tagssvc.tagURI(testURI, [TAG_NAME]);

  let txn = new PlacesRemoveItemTransaction(testBkmId);

  txn.doTransaction();
  Assert.equal(observer._itemRemovedId, testBkmId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);
  Assert.equal(tagssvc.getTagsForURI(testURI), TAG_NAME);

  txn.undoTransaction();
  let newbkmk2Id = observer._itemAddedId;
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
  Assert.equal(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.redoTransaction();
  Assert.equal(observer._itemRemovedId, newbkmk2Id);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);
  Assert.equal(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
  Assert.equal(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);
});

add_task(async function test_remove_item_with_keyword() {
  // Notice in this case the tag persists since other bookmarks have same uri.
  let testFolder = bmsvc.createFolder(root, "Test removing an item with a keyword",
                                      bmsvc.DEFAULT_INDEX);

  const KEYWORD = "test: test removing an item with a keyword";
  let testURI = NetUtil.newURI("http://test_remove_item_with_keyword.com");
  let testBkmId = bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "test-item1");

  // set keyword
  await PlacesUtils.keywords.insert({ url: testURI.spec, keyword: KEYWORD});

  let txn = new PlacesRemoveItemTransaction(testBkmId);

  txn.doTransaction();
  Assert.equal(observer._itemRemovedId, testBkmId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);
  await promiseKeyword(KEYWORD, null);

  txn.undoTransaction();
  let newbkmk2Id = observer._itemAddedId;
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
  await promiseKeyword(KEYWORD, testURI.spec);

  txn.redoTransaction();
  Assert.equal(observer._itemRemovedId, newbkmk2Id);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);
  await promiseKeyword(KEYWORD, null);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
});

add_task(async function test_creating_separator() {
  let testFolder = bmsvc.createFolder(root, "Test creating a separator", bmsvc.DEFAULT_INDEX);

  let txn = new PlacesCreateSeparatorTransaction(testFolder, 0);
  txn.doTransaction();

  let sepId = observer._itemAddedId;
  Assert.equal(observer._itemAddedIndex, 0);
  Assert.equal(observer._itemAddedParent, testFolder);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, sepId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);

  txn.redoTransaction();
  let newSepId = observer._itemAddedId;
  Assert.equal(observer._itemAddedIndex, 0);
  Assert.equal(observer._itemAddedParent, testFolder);

  txn.undoTransaction();
  Assert.equal(observer._itemRemovedId, newSepId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);
});

add_task(async function test_removing_separator() {
  let testFolder = bmsvc.createFolder(root, "Test removing a separator", bmsvc.DEFAULT_INDEX);

  let sepId = bmsvc.insertSeparator(testFolder, 0);
  let txn = new PlacesRemoveItemTransaction(sepId);

  txn.doTransaction();
  Assert.equal(observer._itemRemovedId, sepId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedId, sepId); // New separator created
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);

  txn.redoTransaction();
  Assert.equal(observer._itemRemovedId, sepId);
  Assert.equal(observer._itemRemovedFolder, testFolder);
  Assert.equal(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  Assert.equal(observer._itemAddedId, sepId); // New separator created
  Assert.equal(observer._itemAddedParent, testFolder);
  Assert.equal(observer._itemAddedIndex, 0);
});

add_task(async function test_editing_item_title() {
  const TITLE = "Test editing item title";
  const MOD_TITLE = "Mod: Test editing item title";
  let testURI = NetUtil.newURI("http://www.test_editing_item_title.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, TITLE);

  let txn = new PlacesEditItemTitleTransaction(testBkmId, MOD_TITLE);

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "title");
  Assert.equal(observer._itemChangedValue, MOD_TITLE);

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "title");
  Assert.equal(observer._itemChangedValue, TITLE);

  txn.redoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "title");
  Assert.equal(observer._itemChangedValue, MOD_TITLE);

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "title");
  Assert.equal(observer._itemChangedValue, TITLE);
});

add_task(async function test_editing_item_uri() {
  const OLD_TEST_URI = NetUtil.newURI("http://old.test_editing_item_uri.com/");
  const NEW_TEST_URI = NetUtil.newURI("http://new.test_editing_item_uri.com/");
  let testBkmId = bmsvc.insertBookmark(root, OLD_TEST_URI, bmsvc.DEFAULT_INDEX,
                                       "Test editing item title");
  tagssvc.tagURI(OLD_TEST_URI, ["tag"]);

  let txn = new PlacesEditBookmarkURITransaction(testBkmId, NEW_TEST_URI);

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "uri");
  Assert.equal(observer._itemChangedValue, NEW_TEST_URI.spec);
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(NEW_TEST_URI)), JSON.stringify(["tag"]));
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(OLD_TEST_URI)), JSON.stringify([]));

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "uri");
  Assert.equal(observer._itemChangedValue, OLD_TEST_URI.spec);
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(OLD_TEST_URI)), JSON.stringify(["tag"]));
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(NEW_TEST_URI)), JSON.stringify([]));

  txn.redoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "uri");
  Assert.equal(observer._itemChangedValue, NEW_TEST_URI.spec);
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(NEW_TEST_URI)), JSON.stringify(["tag"]));
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(OLD_TEST_URI)), JSON.stringify([]));

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "uri");
  Assert.equal(observer._itemChangedValue, OLD_TEST_URI.spec);
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(OLD_TEST_URI)), JSON.stringify(["tag"]));
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(NEW_TEST_URI)), JSON.stringify([]));
});

add_task(async function test_edit_description_transaction() {
  let testURI = NetUtil.newURI("http://test_edit_description_transaction.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test edit description transaction");

  let anno = {
    name: DESCRIPTION_ANNO,
    type: Ci.nsIAnnotationService.TYPE_STRING,
    flags: 0,
    value: "Test edit Description",
    expires: Ci.nsIAnnotationService.EXPIRE_NEVER,
  };
  let txn = new PlacesSetItemAnnotationTransaction(testBkmId, anno);

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, DESCRIPTION_ANNO);
});

add_task(async function test_edit_keyword() {
  const KEYWORD = "keyword-test_edit_keyword";

  let testURI = NetUtil.newURI("http://test_edit_keyword.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test edit keyword");

  let txn = new PlacesEditBookmarkKeywordTransaction(testBkmId, KEYWORD, "postData");

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "keyword");
  Assert.equal(observer._itemChangedValue, KEYWORD);
  Assert.equal(PlacesUtils.getPostDataForBookmark(testBkmId), "postData");

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "keyword");
  Assert.equal(observer._itemChangedValue, "");
  Assert.equal(PlacesUtils.getPostDataForBookmark(testBkmId), null);
});

add_task(async function test_edit_specific_keyword() {
  const KEYWORD = "keyword-test_edit_keyword2";

  let testURI = NetUtil.newURI("http://test_edit_keyword2.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test edit keyword");
  // Add multiple keyword to this uri.
  await PlacesUtils.keywords.insert({ keyword: "kw1", url: testURI.spec, postData: "postData1" });
  await PlacesUtils.keywords.insert({keyword: "kw2", url: testURI.spec, postData: "postData2" });

  // Try to change only kw2.
  let txn = new PlacesEditBookmarkKeywordTransaction(testBkmId, KEYWORD, "postData2", "kw2");

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "keyword");
  Assert.equal(observer._itemChangedValue, KEYWORD);
  let entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, testURI.spec);
  Assert.equal(entry.postData, "postData1");
  await promiseKeyword(KEYWORD, testURI.spec, "postData2");
  await promiseKeyword("kw2", null);

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "keyword");
  Assert.equal(observer._itemChangedValue, "kw2");
  Assert.equal(PlacesUtils.getPostDataForBookmark(testBkmId), "postData1");
  entry = await PlacesUtils.keywords.fetch("kw1");
  Assert.equal(entry.url.href, testURI.spec);
  Assert.equal(entry.postData, "postData1");
  await promiseKeyword("kw2", testURI.spec, "postData2");
  await promiseKeyword("keyword", null);
});

add_task(async function test_LoadInSidebar_transaction() {
  let testURI = NetUtil.newURI("http://test_LoadInSidebar_transaction.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test LoadInSidebar transaction");

  const LOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";
  let anno = { name: LOAD_IN_SIDEBAR_ANNO,
               type: Ci.nsIAnnotationService.TYPE_INT32,
               flags: 0,
               value: true,
               expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  let txn = new PlacesSetItemAnnotationTransaction(testBkmId, anno);

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, LOAD_IN_SIDEBAR_ANNO);
  Assert.equal(observer._itemChanged_isAnnotationProperty, true);

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, LOAD_IN_SIDEBAR_ANNO);
  Assert.equal(observer._itemChanged_isAnnotationProperty, true);
});

add_task(async function test_generic_item_annotation() {
  let testURI = NetUtil.newURI("http://test_generic_item_annotation.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test generic item annotation");

  let itemAnnoObj = { name: "testAnno/testInt",
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: 123,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  let txn = new PlacesSetItemAnnotationTransaction(testBkmId, itemAnnoObj);

  txn.doTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "testAnno/testInt");
  Assert.equal(observer._itemChanged_isAnnotationProperty, true);

  txn.undoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "testAnno/testInt");
  Assert.equal(observer._itemChanged_isAnnotationProperty, true);

  txn.redoTransaction();
  Assert.equal(observer._itemChangedId, testBkmId);
  Assert.equal(observer._itemChangedProperty, "testAnno/testInt");
  Assert.equal(observer._itemChanged_isAnnotationProperty, true);
});

add_task(async function test_editing_item_date_added() {
  let testURI = NetUtil.newURI("http://test_editing_item_date_added.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX,
                                       "Test editing item date added");

  let oldAdded = bmsvc.getItemDateAdded(testBkmId);
  let newAdded = Date.now() * 1000 + 1000;
  let txn = new PlacesEditItemDateAddedTransaction(testBkmId, newAdded);

  txn.doTransaction();
  Assert.equal(newAdded, bmsvc.getItemDateAdded(testBkmId));

  txn.undoTransaction();
  Assert.equal(oldAdded, bmsvc.getItemDateAdded(testBkmId));
});

add_task(async function test_edit_item_last_modified() {
  let testURI = NetUtil.newURI("http://test_edit_item_last_modified.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX,
                                       "Test editing item last modified");

  let oldModified = bmsvc.getItemLastModified(testBkmId);
  let newModified = Date.now() * 1000 + 1000;
  let txn = new PlacesEditItemLastModifiedTransaction(testBkmId, newModified);

  txn.doTransaction();
  Assert.equal(newModified, bmsvc.getItemLastModified(testBkmId));

  txn.undoTransaction();
  Assert.equal(oldModified, bmsvc.getItemLastModified(testBkmId));
});

add_task(async function test_generic_page_annotation() {
  const TEST_ANNO = "testAnno/testInt";
  let testURI = NetUtil.newURI("http://www.mozilla.org/");
  PlacesTestUtils.addVisits(testURI).then(function() {
    let pageAnnoObj = { name: TEST_ANNO,
                        type: Ci.nsIAnnotationService.TYPE_INT32,
                        flags: 0,
                        value: 123,
                        expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
    let txn = new PlacesSetPageAnnotationTransaction(testURI, pageAnnoObj);

    txn.doTransaction();
    Assert.ok(annosvc.pageHasAnnotation(testURI, TEST_ANNO));

    txn.undoTransaction();
    Assert.ok(!annosvc.pageHasAnnotation(testURI, TEST_ANNO));

    txn.redoTransaction();
    Assert.ok(annosvc.pageHasAnnotation(testURI, TEST_ANNO));
  });
});

add_task(async function test_sort_folder_by_name() {
  let testFolder = bmsvc.createFolder(root, "Test PlacesSortFolderByNameTransaction",
                                      bmsvc.DEFAULT_INDEX);
  let testURI = NetUtil.newURI("http://test_sort_folder_by_name.com");

  bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "bookmark3");
  bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "bookmark2");
  bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "bookmark1");

  let bkmIds = bmsvc.getBookmarkIdsForURI(testURI);
  bkmIds.sort();

  let b1 = bkmIds[0];
  let b2 = bkmIds[1];
  let b3 = bkmIds[2];

  Assert.equal(0, bmsvc.getItemIndex(b1));
  Assert.equal(1, bmsvc.getItemIndex(b2));
  Assert.equal(2, bmsvc.getItemIndex(b3));

  let txn = new PlacesSortFolderByNameTransaction(testFolder);

  txn.doTransaction();
  Assert.equal(2, bmsvc.getItemIndex(b1));
  Assert.equal(1, bmsvc.getItemIndex(b2));
  Assert.equal(0, bmsvc.getItemIndex(b3));

  txn.undoTransaction();
  Assert.equal(0, bmsvc.getItemIndex(b1));
  Assert.equal(1, bmsvc.getItemIndex(b2));
  Assert.equal(2, bmsvc.getItemIndex(b3));

  txn.redoTransaction();
  Assert.equal(2, bmsvc.getItemIndex(b1));
  Assert.equal(1, bmsvc.getItemIndex(b2));
  Assert.equal(0, bmsvc.getItemIndex(b3));

  txn.undoTransaction();
  Assert.equal(0, bmsvc.getItemIndex(b1));
  Assert.equal(1, bmsvc.getItemIndex(b2));
  Assert.equal(2, bmsvc.getItemIndex(b3));
});

add_task(async function test_tagURI_untagURI() {
  const TAG_1 = "tag-test_tagURI_untagURI-bar";
  const TAG_2 = "tag-test_tagURI_untagURI-foo";
  let tagURI = NetUtil.newURI("http://test_tagURI_untagURI.com");

  // Test tagURI
  let tagTxn = new PlacesTagURITransaction(tagURI, [TAG_1, TAG_2]);

  tagTxn.doTransaction();
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1, TAG_2]));

  tagTxn.undoTransaction();
  Assert.equal(tagssvc.getTagsForURI(tagURI).length, 0);

  tagTxn.redoTransaction();
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1, TAG_2]));

  // Test untagURI
  let untagTxn = new PlacesUntagURITransaction(tagURI, [TAG_1]);

  untagTxn.doTransaction();
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_2]));

  untagTxn.undoTransaction();
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1, TAG_2]));

  untagTxn.redoTransaction();
  Assert.equal(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_2]));
});

add_task(async function test_aggregate_removeItem_Txn() {
  let testFolder = bmsvc.createFolder(root, "Test aggregate removeItem transaction", bmsvc.DEFAULT_INDEX);

  const TEST_URL = "http://test_aggregate_removeitem_txn.com/";
  const FOLDERNAME = "Folder";
  let testURI = NetUtil.newURI(TEST_URL);

  let bkmk1Id = bmsvc.insertBookmark(testFolder, testURI, 0, "Mozilla");
  let bkmk2Id = bmsvc.insertSeparator(testFolder, 1);
  let bkmk3Id = bmsvc.createFolder(testFolder, FOLDERNAME, 2);

  let bkmk3_1Id = bmsvc.insertBookmark(bkmk3Id, testURI, 0, "Mozilla");
  let bkmk3_2Id = bmsvc.insertSeparator(bkmk3Id, 1);
  let bkmk3_3Id = bmsvc.createFolder(bkmk3Id, FOLDERNAME, 2);

  let childTxn1 = new PlacesRemoveItemTransaction(bkmk1Id);
  let childTxn2 = new PlacesRemoveItemTransaction(bkmk2Id);
  let childTxn3 = new PlacesRemoveItemTransaction(bkmk3Id);
  let transactions = [childTxn1, childTxn2, childTxn3];
  let txn = new PlacesAggregatedTransaction("RemoveItems", transactions);

  txn.doTransaction();
  Assert.equal(bmsvc.getItemIndex(bkmk1Id), -1);
  Assert.equal(bmsvc.getItemIndex(bkmk2Id), -1);
  Assert.equal(bmsvc.getItemIndex(bkmk3Id), -1);
  Assert.equal(bmsvc.getItemIndex(bkmk3_1Id), -1);
  Assert.equal(bmsvc.getItemIndex(bkmk3_2Id), -1);
  Assert.equal(bmsvc.getItemIndex(bkmk3_3Id), -1);
  // Check last removed item id.
  Assert.equal(observer._itemRemovedId, bkmk3Id);

  txn.undoTransaction();
  let newBkmk1Id = bmsvc.getIdForItemAt(testFolder, 0);
  let newBkmk2Id = bmsvc.getIdForItemAt(testFolder, 1);
  let newBkmk3Id = bmsvc.getIdForItemAt(testFolder, 2);
  let newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  let newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  let newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  Assert.equal(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  Assert.equal(bmsvc.getBookmarkURI(newBkmk1Id).spec, TEST_URL);
  Assert.equal(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  Assert.equal(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  Assert.equal(bmsvc.getItemTitle(newBkmk3Id), FOLDERNAME);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  Assert.equal(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, TEST_URL);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  Assert.equal(bmsvc.getItemTitle(newBkmk3_3Id), FOLDERNAME);
  // Check last added back item id.
  // Notice items are restored in reverse order.
  Assert.equal(observer._itemAddedId, newBkmk1Id);

  txn.redoTransaction();
  Assert.equal(bmsvc.getItemIndex(newBkmk1Id), -1);
  Assert.equal(bmsvc.getItemIndex(newBkmk2Id), -1);
  Assert.equal(bmsvc.getItemIndex(newBkmk3Id), -1);
  Assert.equal(bmsvc.getItemIndex(newBkmk3_1Id), -1);
  Assert.equal(bmsvc.getItemIndex(newBkmk3_2Id), -1);
  Assert.equal(bmsvc.getItemIndex(newBkmk3_3Id), -1);
  // Check last removed item id.
  Assert.equal(observer._itemRemovedId, newBkmk3Id);

  txn.undoTransaction();
  newBkmk1Id = bmsvc.getIdForItemAt(testFolder, 0);
  newBkmk2Id = bmsvc.getIdForItemAt(testFolder, 1);
  newBkmk3Id = bmsvc.getIdForItemAt(testFolder, 2);
  newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  Assert.equal(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  Assert.equal(bmsvc.getBookmarkURI(newBkmk1Id).spec, TEST_URL);
  Assert.equal(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  Assert.equal(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  Assert.equal(bmsvc.getItemTitle(newBkmk3Id), FOLDERNAME);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  Assert.equal(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, TEST_URL);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  Assert.equal(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  Assert.equal(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  Assert.equal(bmsvc.getItemTitle(newBkmk3_3Id), FOLDERNAME);
  // Check last added back item id.
  // Notice items are restored in reverse order.
  Assert.equal(observer._itemAddedId, newBkmk1Id);
});

add_task(async function test_create_item_with_childTxn() {
  let testFolder = bmsvc.createFolder(root, "Test creating an item with childTxns", bmsvc.DEFAULT_INDEX);

  const BOOKMARK_TITLE = "parent item";
  let testURI = NetUtil.newURI("http://test_create_item_with_childTxn.com");
  let childTxns = [];
  let newDateAdded = Date.now() * 1000 - 20000;
  let editDateAdddedTxn = new PlacesEditItemDateAddedTransaction(null, newDateAdded);
  childTxns.push(editDateAdddedTxn);

  let itemChildAnnoObj = { name: "testAnno/testInt",
                           type: Ci.nsIAnnotationService.TYPE_INT32,
                           flags: 0,
                           value: 123,
                           expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  let annoTxn = new PlacesSetItemAnnotationTransaction(null, itemChildAnnoObj);
  childTxns.push(annoTxn);

  let itemWChildTxn = new PlacesCreateBookmarkTransaction(testURI, testFolder, bmStartIndex,
                                                          BOOKMARK_TITLE, null, null,
                                                          childTxns);
  try {
    txnManager.doTransaction(itemWChildTxn);
    let itemId = bmsvc.getBookmarkIdsForURI(testURI)[0];
    Assert.equal(observer._itemAddedId, itemId);
    Assert.equal(newDateAdded, bmsvc.getItemDateAdded(itemId));
    Assert.equal(observer._itemChangedProperty, "testAnno/testInt");
    Assert.equal(observer._itemChanged_isAnnotationProperty, true);
    Assert.ok(annosvc.itemHasAnnotation(itemId, itemChildAnnoObj.name));
    Assert.equal(annosvc.getItemAnnotation(itemId, itemChildAnnoObj.name), itemChildAnnoObj.value);

    itemWChildTxn.undoTransaction();
    Assert.equal(observer._itemRemovedId, itemId);

    itemWChildTxn.redoTransaction();
    Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: testURI}), null);
    let newId = bmsvc.getBookmarkIdsForURI(testURI)[0];
    Assert.equal(newDateAdded, bmsvc.getItemDateAdded(newId));
    Assert.equal(observer._itemAddedId, newId);
    Assert.equal(observer._itemChangedProperty, "testAnno/testInt");
    Assert.equal(observer._itemChanged_isAnnotationProperty, true);
    Assert.ok(annosvc.itemHasAnnotation(newId, itemChildAnnoObj.name));
    Assert.equal(annosvc.getItemAnnotation(newId, itemChildAnnoObj.name), itemChildAnnoObj.value);

    itemWChildTxn.undoTransaction();
    Assert.equal(observer._itemRemovedId, newId);
  } catch (ex) {
    do_throw("Setting a child transaction in a createItem transaction did throw: " + ex);
  }
});

add_task(async function test_create_folder_with_child_itemTxn() {
  let childURI = NetUtil.newURI("http://test_create_folder_with_child_itemTxn.com");
  let childItemTxn = new PlacesCreateBookmarkTransaction(childURI, root,
                                                         bmStartIndex, "childItem");
  let txn = new PlacesCreateFolderTransaction("Test creating a folder with child itemTxns",
                                              root, bmStartIndex, null, [childItemTxn]);
  try {
    txnManager.doTransaction(txn);
    let childItemId = bmsvc.getBookmarkIdsForURI(childURI)[0];
    Assert.equal(observer._itemAddedId, childItemId);
    Assert.equal(observer._itemAddedIndex, 0);
    Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: childURI}), null);

    txn.undoTransaction();
    Assert.equal(await PlacesUtils.bookmarks.fetch({url: childURI}), null);

    txn.redoTransaction();
    let newchildItemId = bmsvc.getBookmarkIdsForURI(childURI)[0];
    Assert.equal(observer._itemAddedIndex, 0);
    Assert.equal(observer._itemAddedId, newchildItemId);
    Assert.notEqual(await PlacesUtils.bookmarks.fetch({url: childURI}), null);

    txn.undoTransaction();
    Assert.equal(await PlacesUtils.bookmarks.fetch({url: childURI}), null);
  } catch (ex) {
    do_throw("Setting a child item transaction in a createFolder transaction did throw: " + ex);
  }
});
