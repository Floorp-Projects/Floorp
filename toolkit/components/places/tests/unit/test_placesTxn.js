/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let bmsvc = PlacesUtils.bookmarks;
let tagssvc = PlacesUtils.tagging;
let annosvc = PlacesUtils.annotations;
let txnManager = PlacesUtils.transactionManager;
const DESCRIPTION_ANNO = "bookmarkProperties/description";

// create and add bookmarks observer
let observer = {

  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  _beginUpdateBatch: false,

  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  _endUpdateBatch: false,

  onItemAdded: function(id, folder, index, itemType, uri) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
    this._itemAddedType = itemType;
  },
  _itemAddedId: null,
  _itemAddedParent: null,
  _itemAddedIndex: null,
  _itemAddedType: null,

  onItemRemoved: function(id, folder, index, itemType) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  _itemRemovedId: null,
  _itemRemovedFolder: null,
  _itemRemovedIndex: null,

  onItemChanged: function(id, property, isAnnotationProperty, newValue,
                          lastModified, itemType) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = newValue;
  },
  _itemChangedId: null,
  _itemChangedProperty: null,
  _itemChanged_isAnnotationProperty: null,
  _itemChangedValue: null,

  onItemVisited: function(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  _itemVisitedId: null,
  _itemVisitedVistId: null,
  _itemVisitedTime: null,

  onItemMoved: function(id, oldParent, oldIndex, newParent, newIndex,
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

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

// index at which items should begin
let bmStartIndex = 0;

// get bookmarks root id
let root = PlacesUtils.bookmarksMenuFolderId;

function run_test() {
  bmsvc.addObserver(observer, false);
  do_register_cleanup(function () {
    bmsvc.removeObserver(observer);
  });

  run_next_test();
}

add_test(function test_create_folder_with_description() {
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
  do_check_eq(bmsvc.getItemTitle(folderId), TEST_FOLDERNAME);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, folderId);
  do_check_eq(TEST_DESCRIPTION, annosvc.getItemAnnotation(folderId, DESCRIPTION_ANNO));

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  txn.redoTransaction();
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, folderId);

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  run_next_test();
});

add_test(function test_create_item() {
  let testURI = NetUtil.newURI("http://test_create_item.com");

  let txn = new PlacesCreateBookmarkTransaction(testURI, root, bmStartIndex,
                                                "Test creating an item");

  txnManager.doTransaction(txn);
  let id = bmsvc.getBookmarkIdsForURI(testURI)[0];
  do_check_eq(observer._itemAddedId, id);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(testURI));

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, id);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);
  do_check_false(bmsvc.isBookmarked(testURI));

  txn.redoTransaction();
  do_check_true(bmsvc.isBookmarked(testURI));
  let newId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, newId);

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, newId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  run_next_test();
});

add_test(function test_create_item_to_folder() {
  const TEST_FOLDERNAME = "Test creating item to a folder";
  let testURI = NetUtil.newURI("http://test_create_item_to_folder.com");
  let folderId = bmsvc.createFolder(root, TEST_FOLDERNAME, bmsvc.DEFAULT_INDEX);

  let txn = new PlacesCreateBookmarkTransaction(testURI, folderId, bmStartIndex,
                                                "Test creating item");
  txnManager.doTransaction(txn);
  let bkmId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  do_check_eq(observer._itemAddedId, bkmId);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(testURI));

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, bkmId);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  txn.redoTransaction();
  let newBkmId = bmsvc.getBookmarkIdsForURI(testURI)[0];
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, folderId);
  do_check_eq(observer._itemAddedId, newBkmId);

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, newBkmId);
  do_check_eq(observer._itemRemovedFolder, folderId);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  run_next_test();
});

add_test(function test_move_items_to_folder() {
  let testFolderId = bmsvc.createFolder(root, "Test move items", bmsvc.DEFAULT_INDEX);
  let testURI = NetUtil.newURI("http://test_move_items.com");
  let testBkmId = bmsvc.insertBookmark(testFolderId, testURI, bmsvc.DEFAULT_INDEX, "1: Test move items");
  bmsvc.insertBookmark(testFolderId, testURI, bmsvc.DEFAULT_INDEX, "2: Test move items");

  // Moving items between the same folder
  let sameTxn = new PlacesMoveItemTransaction(testBkmId, testFolderId, bmsvc.DEFAULT_INDEX);

  sameTxn.doTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 1);

  sameTxn.undoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  sameTxn.redoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 1);

  sameTxn.undoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  // Moving items between different folders
  let folderId = bmsvc.createFolder(testFolderId,
                                    "Test move items between different folders",
                                    bmsvc.DEFAULT_INDEX);
  let diffTxn = new PlacesMoveItemTransaction(testBkmId, folderId, bmsvc.DEFAULT_INDEX);

  diffTxn.doTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, folderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  sameTxn.undoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, folderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  diffTxn.redoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, testFolderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, folderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  sameTxn.undoTransaction();
  do_check_eq(observer._itemMovedId, testBkmId);
  do_check_eq(observer._itemMovedOldParent, folderId);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, testFolderId);
  do_check_eq(observer._itemMovedNewIndex, 0);

  run_next_test();
});

add_test(function test_remove_folder() {
  let testFolder = bmsvc.createFolder(root, "Test Removing a Folder", bmsvc.DEFAULT_INDEX);
  let folderId = bmsvc.createFolder(testFolder, "Removed Folder", bmsvc.DEFAULT_INDEX);

  let txn = new PlacesRemoveItemTransaction(folderId);

  txn.doTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  do_check_eq(observer._itemAddedId, folderId);
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  txn.redoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  do_check_eq(observer._itemAddedId, folderId);
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  run_next_test();
});

add_test(function test_remove_item_with_keyword_and_tag() {
  // Notice in this case the tag persists since other bookmarks have same uri.
  let testFolder = bmsvc.createFolder(root, "Test removing an item with a keyword and a tag",
                                      bmsvc.DEFAULT_INDEX);

  const KEYWORD = "test: test removing an item with a keyword and a tag";
  const TAG_NAME = "tag-test_remove_item_with_keyword_and_tag";
  let testURI = NetUtil.newURI("http://test_remove_item_with_keyword_and_tag.com");
  let testBkmId = bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "test-item1");

  // create bookmark for not removing tag.
  bmsvc.insertBookmark(testFolder, testURI, bmsvc.DEFAULT_INDEX, "test-item2");

  // set tag & keyword
  bmsvc.setKeywordForBookmark(testBkmId, KEYWORD);
  tagssvc.tagURI(testURI, [TAG_NAME]);

  let txn = new PlacesRemoveItemTransaction(testBkmId);

  txn.doTransaction();
  do_check_eq(observer._itemRemovedId, testBkmId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);
  do_check_eq(bmsvc.getKeywordForBookmark(testBkmId), null);
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.undoTransaction();
  let newbkmk2Id = observer._itemAddedId;
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);
  do_check_eq(bmsvc.getKeywordForBookmark(newbkmk2Id), KEYWORD);
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.redoTransaction();
  do_check_eq(observer._itemRemovedId, newbkmk2Id);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);
  do_check_eq(bmsvc.getKeywordForBookmark(newbkmk2Id), null);
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.undoTransaction();
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  run_next_test();
});

add_test(function test_remove_item_with_tag() {
  const TAG_NAME = "tag-test_remove_item_with_tag";
  let testURI = NetUtil.newURI("http://test_remove_item_with_tag.com/");
  let itemId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test removing an item with a tag");
  tagssvc.tagURI(testURI, [TAG_NAME]);

  let txn = new PlacesRemoveItemTransaction(itemId);

  txn.doTransaction();
  do_check_true(tagssvc.getTagsForURI(testURI).length == 0);

  txn.undoTransaction();
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.redoTransaction();
  do_check_true(tagssvc.getTagsForURI(testURI).length == 0);

  txn.undoTransaction();
  do_check_eq(tagssvc.getTagsForURI(testURI)[0], TAG_NAME);

  txn.redoTransaction();

  run_next_test();
});

add_test(function test_creating_separator() {
  let testFolder = bmsvc.createFolder(root, "Test creating a separator", bmsvc.DEFAULT_INDEX);

  let txn = new PlacesCreateSeparatorTransaction(testFolder, 0);
  txn.doTransaction();

  let sepId = observer._itemAddedId;
  do_check_eq(observer._itemAddedIndex, 0);
  do_check_eq(observer._itemAddedParent, testFolder);

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, sepId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  txn.redoTransaction();
  let newSepId = observer._itemAddedId;
  do_check_eq(observer._itemAddedIndex, 0);
  do_check_eq(observer._itemAddedParent, testFolder);

  txn.undoTransaction();
  do_check_eq(observer._itemRemovedId, newSepId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  run_next_test();
});

add_test(function test_removing_separator() {
  let testFolder = bmsvc.createFolder(root, "Test removing a separator", bmsvc.DEFAULT_INDEX);

  let sepId = bmsvc.insertSeparator(testFolder, 0);
  let txn = new PlacesRemoveItemTransaction(sepId);

  txn.doTransaction();
  do_check_eq(observer._itemRemovedId, sepId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  do_check_eq(observer._itemAddedId, sepId); //New separator created
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  txn.redoTransaction();
  do_check_eq(observer._itemRemovedId, sepId);
  do_check_eq(observer._itemRemovedFolder, testFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  txn.undoTransaction();
  do_check_eq(observer._itemAddedId, sepId); //New separator created
  do_check_eq(observer._itemAddedParent, testFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  run_next_test();
});

add_test(function test_editing_item_title() {
  const TITLE = "Test editing item title";
  const MOD_TITLE = "Mod: Test editing item title";
  let testURI = NetUtil.newURI("http://www.test_editing_item_title.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, TITLE);

  let txn = new PlacesEditItemTitleTransaction(testBkmId, MOD_TITLE);

  txn.doTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, MOD_TITLE);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, TITLE);

  txn.redoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, MOD_TITLE);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, TITLE);

  run_next_test();
});

add_test(function test_editing_item_uri() {
  const OLD_TEST_URL = "http://old.test_editing_item_uri.com/";
  const NEW_TEST_URL = "http://new.test_editing_item_uri.com/";
  let testBkmId = bmsvc.insertBookmark(root, NetUtil.newURI(OLD_TEST_URL), bmsvc.DEFAULT_INDEX, "Test editing item title");

  let txn = new PlacesEditBookmarkURITransaction(testBkmId, NetUtil.newURI(NEW_TEST_URL));

  txn.doTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, NEW_TEST_URL);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, OLD_TEST_URL);

  txn.redoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, NEW_TEST_URL);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, OLD_TEST_URL);

  run_next_test();
});

add_test(function test_edit_description_transaction() {
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
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, DESCRIPTION_ANNO);

  run_next_test();
});

add_test(function test_edit_keyword() {
  const KEYWORD = "keyword-test_edit_keyword";

  let testURI = NetUtil.newURI("http://test_edit_keyword.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test edit keyword");

  let txn = new PlacesEditBookmarkKeywordTransaction(testBkmId, KEYWORD);

  txn.doTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, KEYWORD);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, "");

  run_next_test();
});

add_test(function test_LoadInSidebar_transaction() {
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
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, LOAD_IN_SIDEBAR_ANNO);
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, LOAD_IN_SIDEBAR_ANNO);
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  run_next_test();
});

add_test(function test_generic_item_annotation() {
  let testURI = NetUtil.newURI("http://test_generic_item_annotation.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test generic item annotation");

  let itemAnnoObj = { name: "testAnno/testInt",
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: 123,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  let txn = new PlacesSetItemAnnotationTransaction(testBkmId, itemAnnoObj);

  txn.doTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  txn.undoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  txn.redoTransaction();
  do_check_eq(observer._itemChangedId, testBkmId);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  run_next_test();
});

add_test(function test_editing_item_date_added() {
  let testURI = NetUtil.newURI("http://test_editing_item_date_added.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX,
                                       "Test editing item date added");

  let oldAdded = bmsvc.getItemDateAdded(testBkmId);
  let newAdded = Date.now() + 1000;
  let txn = new PlacesEditItemDateAddedTransaction(testBkmId, newAdded);

  txn.doTransaction();
  do_check_eq(newAdded, bmsvc.getItemDateAdded(testBkmId));

  txn.undoTransaction();
  do_check_eq(oldAdded, bmsvc.getItemDateAdded(testBkmId));

  run_next_test();
});

add_test(function test_edit_item_last_modified() {
  let testURI = NetUtil.newURI("http://test_edit_item_last_modified.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX,
                                       "Test editing item last modified");

  let oldModified = bmsvc.getItemLastModified(testBkmId);
  let newModified = Date.now() + 1000;
  let txn = new PlacesEditItemLastModifiedTransaction(testBkmId, newModified);

  txn.doTransaction();
  do_check_eq(newModified, bmsvc.getItemLastModified(testBkmId));

  txn.undoTransaction();
  do_check_eq(oldModified, bmsvc.getItemLastModified(testBkmId));

  run_next_test();
});

add_test(function test_generic_page_annotation() {
  const TEST_ANNO = "testAnno/testInt";
  let testURI = NetUtil.newURI("http://www.mozilla.org/");
  promiseAddVisits(testURI).then(function () {
    let pageAnnoObj = { name: TEST_ANNO,
                        type: Ci.nsIAnnotationService.TYPE_INT32,
                        flags: 0,
                        value: 123,
                        expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
    let txn = new PlacesSetPageAnnotationTransaction(testURI, pageAnnoObj);

    txn.doTransaction();
    do_check_true(annosvc.pageHasAnnotation(testURI, TEST_ANNO));

    txn.undoTransaction();
    do_check_false(annosvc.pageHasAnnotation(testURI, TEST_ANNO));

    txn.redoTransaction();
    do_check_true(annosvc.pageHasAnnotation(testURI, TEST_ANNO));

    run_next_test();
  });
});

add_test(function test_sort_folder_by_name() {
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

  do_check_eq(0, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(2, bmsvc.getItemIndex(b3));

  let txn = new PlacesSortFolderByNameTransaction(testFolder);

  txn.doTransaction();
  do_check_eq(2, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(0, bmsvc.getItemIndex(b3));

  txn.undoTransaction();
  do_check_eq(0, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(2, bmsvc.getItemIndex(b3));

  txn.redoTransaction();
  do_check_eq(2, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(0, bmsvc.getItemIndex(b3));

  txn.undoTransaction();
  do_check_eq(0, bmsvc.getItemIndex(b1));
  do_check_eq(1, bmsvc.getItemIndex(b2));
  do_check_eq(2, bmsvc.getItemIndex(b3));

  run_next_test();
});

add_test(function test_edit_postData() {
  const POST_DATA_ANNO = "bookmarkProperties/POSTData";
  let postData = "post-test_edit_postData";
  let testURI = NetUtil.newURI("http://test_edit_postData.com");
  let testBkmId = bmsvc.insertBookmark(root, testURI, bmsvc.DEFAULT_INDEX, "Test edit Post Data");

  let txn = new PlacesEditBookmarkPostDataTransaction(testBkmId, postData);

  txn.doTransaction();
  do_check_true(annosvc.itemHasAnnotation(testBkmId, POST_DATA_ANNO));
  do_check_eq(annosvc.getItemAnnotation(testBkmId, POST_DATA_ANNO), postData);

  txn.undoTransaction();
  do_check_false(annosvc.itemHasAnnotation(testBkmId, POST_DATA_ANNO));

  run_next_test();
});

add_test(function test_tagURI_untagURI() {
  const TAG_1 = "tag-test_tagURI_untagURI-bar";
  const TAG_2 = "tag-test_tagURI_untagURI-foo";
  let tagURI = NetUtil.newURI("http://test_tagURI_untagURI.com");

  // Test tagURI
  let tagTxn = new PlacesTagURITransaction(tagURI, [TAG_1, TAG_2]);

  tagTxn.doTransaction();
  do_check_eq(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1,TAG_2]));

  tagTxn.undoTransaction();
  do_check_eq(tagssvc.getTagsForURI(tagURI).length, 0);

  tagTxn.redoTransaction();
  do_check_eq(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1,TAG_2]));

  // Test untagURI
  let untagTxn = new PlacesUntagURITransaction(tagURI, [TAG_1]);

  untagTxn.doTransaction();
  do_check_eq(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_2]));

  untagTxn.undoTransaction();
  do_check_eq(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_1,TAG_2]));

  untagTxn.redoTransaction();
  do_check_eq(JSON.stringify(tagssvc.getTagsForURI(tagURI)), JSON.stringify([TAG_2]));

  run_next_test();
});

add_test(function test_aggregate_removeItem_Txn() {
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
  do_check_eq(bmsvc.getItemIndex(bkmk1Id), -1);
  do_check_eq(bmsvc.getItemIndex(bkmk2Id), -1);
  do_check_eq(bmsvc.getItemIndex(bkmk3Id), -1);
  do_check_eq(bmsvc.getItemIndex(bkmk3_1Id), -1);
  do_check_eq(bmsvc.getItemIndex(bkmk3_2Id), -1);
  do_check_eq(bmsvc.getItemIndex(bkmk3_3Id), -1);
  // Check last removed item id.
  do_check_eq(observer._itemRemovedId, bkmk3Id);

  txn.undoTransaction();
  let newBkmk1Id = bmsvc.getIdForItemAt(testFolder, 0);
  let newBkmk2Id = bmsvc.getIdForItemAt(testFolder, 1);
  let newBkmk3Id = bmsvc.getIdForItemAt(testFolder, 2);
  let newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  let newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  let newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  do_check_eq(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk1Id).spec, TEST_URL);
  do_check_eq(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3Id), FOLDERNAME);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, TEST_URL);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3_3Id), FOLDERNAME);
  // Check last added back item id.
  // Notice items are restored in reverse order.
  do_check_eq(observer._itemAddedId, newBkmk1Id);

  txn.redoTransaction();
  do_check_eq(bmsvc.getItemIndex(newBkmk1Id), -1);
  do_check_eq(bmsvc.getItemIndex(newBkmk2Id), -1);
  do_check_eq(bmsvc.getItemIndex(newBkmk3Id), -1);
  do_check_eq(bmsvc.getItemIndex(newBkmk3_1Id), -1);
  do_check_eq(bmsvc.getItemIndex(newBkmk3_2Id), -1);
  do_check_eq(bmsvc.getItemIndex(newBkmk3_3Id), -1);
  // Check last removed item id.
  do_check_eq(observer._itemRemovedId, newBkmk3Id);

  txn.undoTransaction();
  newBkmk1Id = bmsvc.getIdForItemAt(testFolder, 0);
  newBkmk2Id = bmsvc.getIdForItemAt(testFolder, 1);
  newBkmk3Id = bmsvc.getIdForItemAt(testFolder, 2);
  newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  do_check_eq(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk1Id).spec, TEST_URL);
  do_check_eq(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3Id), FOLDERNAME);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, TEST_URL);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3_3Id), FOLDERNAME);
  // Check last added back item id.
  // Notice items are restored in reverse order.
  do_check_eq(observer._itemAddedId, newBkmk1Id);

  run_next_test();
});

add_test(function test_create_item_with_childTxn() {
  let testFolder = bmsvc.createFolder(root, "Test creating an item with childTxns", bmsvc.DEFAULT_INDEX);

  const BOOKMARK_TITLE = "parent item";
  let testURI = NetUtil.newURI("http://test_create_item_with_childTxn.com");
  let childTxns = [];
  let newDateAdded = Date.now() - 20000;
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
    do_check_eq(observer._itemAddedId, itemId);
    do_check_eq(newDateAdded, bmsvc.getItemDateAdded(itemId));
    do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
    do_check_eq(observer._itemChanged_isAnnotationProperty, true);
    do_check_true(annosvc.itemHasAnnotation(itemId, itemChildAnnoObj.name))
    do_check_eq(annosvc.getItemAnnotation(itemId, itemChildAnnoObj.name), itemChildAnnoObj.value);

    itemWChildTxn.undoTransaction();
    do_check_eq(observer._itemRemovedId, itemId);

    itemWChildTxn.redoTransaction();
    do_check_true(bmsvc.isBookmarked(testURI));
    let newId = bmsvc.getBookmarkIdsForURI(testURI)[0];
    do_check_eq(newDateAdded, bmsvc.getItemDateAdded(newId));
    do_check_eq(observer._itemAddedId, newId);
    do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
    do_check_eq(observer._itemChanged_isAnnotationProperty, true);
    do_check_true(annosvc.itemHasAnnotation(newId, itemChildAnnoObj.name))
    do_check_eq(annosvc.getItemAnnotation(newId, itemChildAnnoObj.name), itemChildAnnoObj.value);

    itemWChildTxn.undoTransaction();
    do_check_eq(observer._itemRemovedId, newId);
  }
  catch (ex) {
    do_throw("Setting a child transaction in a createItem transaction did throw: " + ex);
  }

  run_next_test();
});

add_test(function test_create_folder_with_child_itemTxn() {
  let childURI = NetUtil.newURI("http://test_create_folder_with_child_itemTxn.com");
  let childItemTxn = new PlacesCreateBookmarkTransaction(childURI, root,
                                                         bmStartIndex, "childItem");
  let txn = new PlacesCreateFolderTransaction("Test creating a folder with child itemTxns",
                                              root, bmStartIndex, null, [childItemTxn]);
  try {
    txnManager.doTransaction(txn);
    let childItemId = bmsvc.getBookmarkIdsForURI(childURI)[0];
    do_check_eq(observer._itemAddedId, childItemId);
    do_check_eq(observer._itemAddedIndex, 0);
    do_check_true(bmsvc.isBookmarked(childURI));

    txn.undoTransaction();
    do_check_false(bmsvc.isBookmarked(childURI));

    txn.redoTransaction();
    let newchildItemId = bmsvc.getBookmarkIdsForURI(childURI)[0];
    do_check_eq(observer._itemAddedIndex, 0);
    do_check_eq(observer._itemAddedId, newchildItemId);
    do_check_true(bmsvc.isBookmarked(childURI));

    txn.undoTransaction();
    do_check_false(bmsvc.isBookmarked(childURI));
  }
  catch (ex) {
    do_throw("Setting a child item transaction in a createFolder transaction did throw: " + ex);
  }

  run_next_test();
});
