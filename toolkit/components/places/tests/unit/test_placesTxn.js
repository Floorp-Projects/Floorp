/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Sungjoon Steve Won <stevewon@gmail.com> (Original Author)
 *  Asaf Romano <mano@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var bmsvc = PlacesUtils.bookmarks;
var tagssvc = PlacesUtils.tagging;
var annosvc = PlacesUtils.annotations;
let txnManager = PlacesUtils.transactionManager;
const kDESCRIPTION_ANNO = "bookmarkProperties/description";
const kLOAD_IN_SIDEBAR_ANNO = "bookmarkProperties/loadInSidebar";

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  onItemAdded: function(id, folder, index, itemType, uri) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
    this._itemAddedType = itemType;
  },
  onBeforeItemRemoved: function(id) {
  },
  onItemRemoved: function(id, folder, index, itemType) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  onItemChanged: function(id, property, isAnnotationProperty, newValue,
                          lastModified, itemType) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = newValue;
  },
  onItemVisited: function(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  onItemMoved: function(id, oldParent, oldIndex, newParent, newIndex,
                        itemType) {
    this._itemMovedId = id
    this._itemMovedOldParent = oldParent;
    this._itemMovedOldIndex = oldIndex;
    this._itemMovedNewParent = newParent;
    this._itemMovedNewIndex = newIndex;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
bmsvc.addObserver(observer, false);

// index at which items should begin
var bmStartIndex = 0;

// main
function run_test() {
  // get bookmarks root index
  var root = bmsvc.bookmarksMenuFolder;

  //Test creating a folder with a description
  const TEST_DESCRIPTION = "this is my test description";
  var annos = [{ name: kDESCRIPTION_ANNO,
                 type: annosvc.TYPE_STRING,
                flags: 0,
                value: TEST_DESCRIPTION,
              expires: annosvc.EXPIRE_NEVER }];
  var txn1 = new PlacesCreateFolderTransaction("Testing folder", root, bmStartIndex, annos);
  txnManager.doTransaction(txn1);

  // This checks that calling undoTransaction on an "empty batch" doesn't
  // undo the previous transaction (getItemTitle will fail)
  txnManager.beginBatch();
  txnManager.endBatch();
  txnManager.undoTransaction();

  var folderId = observer._itemAddedId;
  do_check_eq(bmsvc.getItemTitle(folderId), "Testing folder");
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, folderId);
  do_check_eq(TEST_DESCRIPTION, 
              annosvc.getItemAnnotation(folderId, kDESCRIPTION_ANNO));

  txn1.undoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);
  txn1.redoTransaction();
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, folderId);
  txn1.undoTransaction();
  do_check_eq(observer._itemRemovedId, folderId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Test creating an item
  // Create to Root
  var txn2 = new PlacesCreateBookmarkTransaction(uri("http://www.example.com"),
                                                 root, bmStartIndex, "Testing1");
  txnManager.doTransaction(txn2); // Also testing doTransaction
  var b = (bmsvc.getBookmarkIdsForURI(uri("http://www.example.com")))[0];
  do_check_eq(observer._itemAddedId, b);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(uri("http://www.example.com")));
  txn2.undoTransaction();
  do_check_eq(observer._itemRemovedId, b);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);
  do_check_false(bmsvc.isBookmarked(uri("http://www.example.com")));
  txn2.redoTransaction();
  do_check_true(bmsvc.isBookmarked(uri("http://www.example.com")));
  var newId = (bmsvc.getBookmarkIdsForURI(uri("http://www.example.com")))[0];
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedId, newId);
  txn2.undoTransaction();
  do_check_eq(observer._itemRemovedId, newId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Create item to a folder
  var txn2a = new PlacesCreateFolderTransaction("Folder", root, bmStartIndex);
  txnManager.doTransaction(txn2a);
  var fldrId = observer._itemAddedId;
  do_check_eq(bmsvc.getItemTitle(fldrId), "Folder");

  var txn2b = new PlacesCreateBookmarkTransaction(uri("http://www.example2.com"),
                                                  fldrId, bmStartIndex, "Testing1b");
  txnManager.doTransaction(txn2b);
  var b2 = (bmsvc.getBookmarkIdsForURI(uri("http://www.example2.com")))[0];
  do_check_eq(observer._itemAddedId, b2);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_true(bmsvc.isBookmarked(uri("http://www.example2.com")));
  txn2b.undoTransaction();
  do_check_eq(observer._itemRemovedId, b2);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);
  txn2b.redoTransaction();
  newId = (bmsvc.getBookmarkIdsForURI(uri("http://www.example2.com")))[0];
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  do_check_eq(observer._itemAddedParent, fldrId);
  do_check_eq(observer._itemAddedId, newId);
  txn2b.undoTransaction();
  do_check_eq(observer._itemRemovedId, newId);
  do_check_eq(observer._itemRemovedFolder, fldrId);
  do_check_eq(observer._itemRemovedIndex, bmStartIndex);

  // Testing moving an item
  {
    let txnTesting2 = new PlacesCreateBookmarkTransaction(uri("http://www.example3.com"),
                                                          root, -1, "Testing2");
    txnManager.doTransaction(txnTesting2);
    let txnTesting3 = new PlacesCreateBookmarkTransaction(uri("http://www.example3.com"),
                                                          root, -1, "Testing3");
    txnManager.doTransaction(txnTesting3);
    let txnTesting4 = new PlacesCreateBookmarkTransaction(uri("http://www.example3.com"),
                                                          fldrId, -1, "Testing4");
    txnManager.doTransaction(txnTesting4);
  }
  var bkmkIds = bmsvc.getBookmarkIdsForURI(uri("http://www.example3.com"));
  bkmkIds.sort();
  var bkmk1Id = bkmkIds[0];
  var bkmk2Id = bkmkIds[1];
  var bkmk3Id = bkmkIds[2];

  // Moving items between the same folder
  var txn3 = new PlacesMoveItemTransaction(bkmk1Id, root, -1);
  txn3.doTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 2);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 2);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);
  txn3.redoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 2);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 2);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);

  // Moving items between different folders
  var txn3b = new PlacesMoveItemTransaction(bkmk1Id, fldrId, -1);
  txn3b.doTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, fldrId);
  do_check_eq(observer._itemMovedNewIndex, 1);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, fldrId);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);
  txn3b.redoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, root);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, fldrId);
  do_check_eq(observer._itemMovedNewIndex, 1);
  txn3.undoTransaction();
  do_check_eq(observer._itemMovedId, bkmk1Id);
  do_check_eq(observer._itemMovedOldParent, fldrId);
  do_check_eq(observer._itemMovedOldIndex, 1);
  do_check_eq(observer._itemMovedNewParent, root);
  do_check_eq(observer._itemMovedNewIndex, 1);

  // Test Removing a Folder
  let txnFolder2 = new PlacesCreateFolderTransaction("Folder2",
                                                     root,
                                                     bmsvc.DEFAULT_INDEX);
  txnManager.doTransaction(txnFolder2);
  let fldrId2 = observer._itemAddedId;
  do_check_eq(bmsvc.getItemTitle(fldrId2), "Folder2");

  var txn4 = new PlacesRemoveItemTransaction(fldrId2);
  txn4.doTransaction();
  do_check_eq(observer._itemRemovedId, fldrId2);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 3);
  txn4.undoTransaction();
  do_check_eq(observer._itemAddedId, fldrId2);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 3);
  txn4.redoTransaction();
  do_check_eq(observer._itemRemovedId, fldrId2);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 3);
  txn4.undoTransaction();
  do_check_eq(observer._itemAddedId, fldrId2);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 3);

  // Test removing an item with a keyword and a tag.
  // Notice in this case the tag persists since other bookmarks have same uri.
  bmsvc.setKeywordForBookmark(bkmk2Id, "test_keyword");
  tagssvc.tagURI(uri("http://www.example3.com"), ["test-tag"]);
  var txn5 = new PlacesRemoveItemTransaction(bkmk2Id);
  txn5.doTransaction();
  do_check_eq(observer._itemRemovedId, bkmk2Id);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 2);
  do_check_eq(bmsvc.getKeywordForBookmark(bkmk2Id), null);
  do_check_eq(tagssvc.getTagsForURI(uri("http://www.example3.com"))[0], "test-tag");
  txn5.undoTransaction();
  var newbkmk2Id = observer._itemAddedId;
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 2);
  do_check_eq(bmsvc.getKeywordForBookmark(newbkmk2Id), "test_keyword");
  do_check_eq(tagssvc.getTagsForURI(uri("http://www.example3.com"))[0], "test-tag");
  txn5.redoTransaction();
  do_check_eq(observer._itemRemovedId, newbkmk2Id);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 2);
  do_check_eq(bmsvc.getKeywordForBookmark(newbkmk2Id), null);
  do_check_eq(tagssvc.getTagsForURI(uri("http://www.example3.com"))[0], "test-tag");
  txn5.undoTransaction();
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, 2);
  do_check_eq(tagssvc.getTagsForURI(uri("http://www.example3.com"))[0], "test-tag");
  tagssvc.untagURI(uri("http://www.example3.com"), ["test-tag"]);

  {
    // Test removing an item with a tag (last bookmark for a uri).
    let testURI = uri("http://www.taggedbm.com/");
    let txnTaggedBm = new PlacesCreateBookmarkTransaction(testURI, fldrId,
                                                          bmStartIndex, "TaggedBm");
    txnManager.doTransaction(txnTaggedBm);
    tagssvc.tagURI(testURI, ["test-tag"]);

    let itemId = observer._itemAddedId;
    let txn = new PlacesRemoveItemTransaction(itemId);
    txn.doTransaction();
    do_check_true(tagssvc.getTagsForURI(testURI).length == 0);
    txn.undoTransaction();
    do_check_eq(tagssvc.getTagsForURI(testURI)[0], "test-tag");
    txn.redoTransaction();
    do_check_true(tagssvc.getTagsForURI(testURI).length == 0);
    txn.undoTransaction();
    do_check_eq(tagssvc.getTagsForURI(testURI)[0], "test-tag");
    txn.redoTransaction();
  }

  // Test creating a separator
  var txn6 = new PlacesCreateSeparatorTransaction(root, 1);
  txn6.doTransaction();
  var sepId = observer._itemAddedId;
  do_check_eq(observer._itemAddedIndex, 1);
  do_check_eq(observer._itemAddedParent, root);
  txn6.undoTransaction();
  do_check_eq(observer._itemRemovedId, sepId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 1);
  txn6.redoTransaction();
  var newSepId = observer._itemAddedId;
  do_check_eq(observer._itemAddedIndex, 1);
  do_check_eq(observer._itemAddedParent, root);
  txn6.undoTransaction();
  do_check_eq(observer._itemRemovedId, newSepId);
  do_check_eq(observer._itemRemovedFolder, root);
  do_check_eq(observer._itemRemovedIndex, 1);

  // Test removing a separator
  {
    let separatorTxn = new PlacesCreateSeparatorTransaction(root, 1);
    txnManager.doTransaction(separatorTxn);
    var sepId2 = observer._itemAddedId;
    var txn7 = new PlacesRemoveItemTransaction(sepId2);
    txn7.doTransaction();
    do_check_eq(observer._itemRemovedId, sepId2);
    do_check_eq(observer._itemRemovedFolder, root);
    do_check_eq(observer._itemRemovedIndex, 1);
    txn7.undoTransaction();
    do_check_eq(observer._itemAddedId, sepId2); //New separator created
    do_check_eq(observer._itemAddedParent, root);
    do_check_eq(observer._itemAddedIndex, 1);
    txn7.redoTransaction();
    do_check_eq(observer._itemRemovedId, sepId2);
    do_check_eq(observer._itemRemovedFolder, root);
    do_check_eq(observer._itemRemovedIndex, 1);
    txn7.undoTransaction();
    do_check_eq(observer._itemAddedId, sepId2); //New separator created
    do_check_eq(observer._itemAddedParent, root);
    do_check_eq(observer._itemAddedIndex, 1);
  }
  // Test editing item title
  var txn8 = new PlacesEditItemTitleTransaction(bkmk1Id, "Testing2_mod");
  txn8.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2_mod");
  txn8.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2");
  txn8.redoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2_mod");
  txn8.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id); 
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Testing2");

  // Test editing item uri
  var txn9 = new PlacesEditBookmarkURITransaction(bkmk1Id, uri("http://newuri.com"));
  txn9.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://newuri.com/");
  txn9.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://www.example3.com/");
  txn9.redoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://newuri.com/");
  txn9.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://www.example3.com/");
  
  // Test edit description transaction.
  let txn10AnnoObj = {
    name: kDESCRIPTION_ANNO,
    type: Ci.nsIAnnotationService.TYPE_STRING,
    flags: 0,
    value: "Description1",
    expires: Ci.nsIAnnotationService.EXPIRE_NEVER,
  };
  var txn10 = new PlacesSetItemAnnotationTransaction(bkmk1Id, txn10AnnoObj);
  txn10.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, kDESCRIPTION_ANNO);

  // Testing edit keyword
  var txn11 = new PlacesEditBookmarkKeywordTransaction(bkmk1Id, "kw1");
  txn11.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, "kw1"); 
  txn11.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "keyword");
  do_check_eq(observer._itemChangedValue, ""); 

  // Test LoadInSidebar transaction.
  let txn16AnnoObj = { name: kLOAD_IN_SIDEBAR_ANNO,
                       type: Ci.nsIAnnotationService.TYPE_INT32,
                       flags: 0,
                       value: true,
                       expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  var txn16 = new PlacesSetItemAnnotationTransaction(bkmk1Id, txn16AnnoObj);
  txn16.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, kLOAD_IN_SIDEBAR_ANNO);
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);
  txn16.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, kLOAD_IN_SIDEBAR_ANNO);
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  // Test generic item annotation
  var itemAnnoObj = { name: "testAnno/testInt",
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: 123,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  var genItemAnnoTxn = new PlacesSetItemAnnotationTransaction(bkmk1Id, itemAnnoObj);
  genItemAnnoTxn.doTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);
  genItemAnnoTxn.undoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);
  genItemAnnoTxn.redoTransaction();
  do_check_eq(observer._itemChangedId, bkmk1Id);
  do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
  do_check_eq(observer._itemChanged_isAnnotationProperty, true);

  // Test generic page annotation
  var pageAnnoObj = { name: "testAnno/testInt",
                      type: Ci.nsIAnnotationService.TYPE_INT32,
                      flags: 0,
                      value: 123,
                      expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  hs.addVisit(uri("http://www.mozilla.org/"), Date.now() * 1000, null,
              hs.TRANSITION_TYPED, false, 0);
  var genPageAnnoTxn = new PlacesSetPageAnnotationTransaction(uri("http://www.mozilla.org/"), pageAnnoObj);
  genPageAnnoTxn.doTransaction();
  do_check_true(annosvc.pageHasAnnotation(uri("http://www.mozilla.org/"), "testAnno/testInt"));
  genPageAnnoTxn.undoTransaction();
  do_check_false(annosvc.pageHasAnnotation(uri("http://www.mozilla.org/"), "testAnno/testInt"));
  genPageAnnoTxn.redoTransaction();
  do_check_true(annosvc.pageHasAnnotation(uri("http://www.mozilla.org/"), "testAnno/testInt"));

  // sortFolderByName
  {
    let sortFolderTxn = new PlacesCreateFolderTransaction("Sorting folder", root,
                                                          bmStartIndex, [], null);
    txnManager.doTransaction(sortFolderTxn);
    var srtFldId = observer._itemAddedId;
    do_check_eq(bmsvc.getItemTitle(srtFldId), "Sorting folder");
    let cTxn = new PlacesCreateBookmarkTransaction(uri("http://www.sortingtest.com"),
                                                   srtFldId, -1, "c");
    txnManager.doTransaction(cTxn);
    let bTxn = new PlacesCreateBookmarkTransaction(uri("http://www.sortingtest.com"),
                                                   srtFldId, -1, "b");
    txnManager.doTransaction(bTxn);
    let aTxn = new PlacesCreateBookmarkTransaction(uri("http://www.sortingtest.com"),
                                                   srtFldId, bmsvc.DEFAULT_INDEX, "a");
    txnManager.doTransaction(aTxn);

    var b = bmsvc.getBookmarkIdsForURI(uri("http://www.sortingtest.com"));
    b.sort();
    var b1 = b[0];
    var b2 = b[1];
    var b3 = b[2];
    do_check_eq(0, bmsvc.getItemIndex(b1));
    do_check_eq(1, bmsvc.getItemIndex(b2));
    do_check_eq(2, bmsvc.getItemIndex(b3));
    var txn17 = new PlacesSortFolderByNameTransaction(srtFldId);
    txn17.doTransaction();
    do_check_eq(2, bmsvc.getItemIndex(b1));
    do_check_eq(1, bmsvc.getItemIndex(b2));
    do_check_eq(0, bmsvc.getItemIndex(b3));
    txn17.undoTransaction();
    do_check_eq(0, bmsvc.getItemIndex(b1));
    do_check_eq(1, bmsvc.getItemIndex(b2));
    do_check_eq(2, bmsvc.getItemIndex(b3));
    txn17.redoTransaction();
    do_check_eq(2, bmsvc.getItemIndex(b1));
    do_check_eq(1, bmsvc.getItemIndex(b2));
    do_check_eq(0, bmsvc.getItemIndex(b3));
    txn17.undoTransaction();
    do_check_eq(0, bmsvc.getItemIndex(b1));
    do_check_eq(1, bmsvc.getItemIndex(b2));
    do_check_eq(2, bmsvc.getItemIndex(b3));
  }

  // Testing edit Post Data
  {
    const POST_DATA_ANNO = "bookmarkProperties/POSTData";
    var postData = "foo";
    var postDataURI = uri("http://foo.com");
    let txn = new PlacesCreateBookmarkTransaction(postDataURI, root, -1,
                                                  "postdata test", null, null, null)
    txnManager.doTransaction(txn);
    var postDataId = (bmsvc.getBookmarkIdsForURI(postDataURI))[0];
    var postDataTxn = new PlacesEditBookmarkPostDataTransaction(postDataId, postData);
    postDataTxn.doTransaction();
    do_check_true(annosvc.itemHasAnnotation(postDataId, POST_DATA_ANNO))
    do_check_eq(annosvc.getItemAnnotation(postDataId, POST_DATA_ANNO), postData);
    postDataTxn.undoTransaction();
    do_check_false(annosvc.itemHasAnnotation(postDataId, POST_DATA_ANNO))
  }

  // Test editing item date added
  var oldAdded = bmsvc.getItemDateAdded(bkmk1Id);
  var newAdded = Date.now();
  var eidaTxn = new PlacesEditItemDateAddedTransaction(bkmk1Id, newAdded);
  eidaTxn.doTransaction();
  do_check_eq(newAdded, bmsvc.getItemDateAdded(bkmk1Id));
  eidaTxn.undoTransaction();
  do_check_eq(oldAdded, bmsvc.getItemDateAdded(bkmk1Id));

  // Test editing item last modified 
  var oldModified = bmsvc.getItemLastModified(bkmk1Id);
  var newModified = Date.now();
  var eilmTxn = new PlacesEditItemLastModifiedTransaction(bkmk1Id, newModified);
  eilmTxn.doTransaction();
  do_check_eq(newModified, bmsvc.getItemLastModified(bkmk1Id));
  eilmTxn.undoTransaction();
  do_check_eq(oldModified, bmsvc.getItemLastModified(bkmk1Id));

  // Test tagURI/untagURI
  var tagURI = uri("http://foo.tld");
  var tagTxn = new PlacesTagURITransaction(tagURI, ["foo", "bar"]);
  tagTxn.doTransaction();
  do_check_eq(uneval(tagssvc.getTagsForURI(tagURI)), uneval(["bar","foo"]));
  tagTxn.undoTransaction();
  do_check_eq(tagssvc.getTagsForURI(tagURI).length, 0);
  tagTxn.redoTransaction();
  do_check_eq(uneval(tagssvc.getTagsForURI(tagURI)), uneval(["bar","foo"]));
  var untagTxn = new PlacesUntagURITransaction(tagURI, ["bar"]);
  untagTxn.doTransaction();
  do_check_eq(uneval(tagssvc.getTagsForURI(tagURI)), uneval(["foo"]));
  untagTxn.undoTransaction();
  do_check_eq(uneval(tagssvc.getTagsForURI(tagURI)), uneval(["bar","foo"]));
  untagTxn.redoTransaction();
  do_check_eq(uneval(tagssvc.getTagsForURI(tagURI)), uneval(["foo"]));

  // Test aggregate removeItem transaction
  var bkmk1Id = bmsvc.insertBookmark(root, uri("http://www.mozilla.org/"), 0, "Mozilla");
  var bkmk2Id = bmsvc.insertSeparator(root, 1);
  var bkmk3Id = bmsvc.createFolder(root, "folder", 2);
  var bkmk3_1Id = bmsvc.insertBookmark(bkmk3Id, uri("http://www.mozilla.org/"), 0, "Mozilla");
  var bkmk3_2Id = bmsvc.insertSeparator(bkmk3Id, 1);
  var bkmk3_3Id = bmsvc.createFolder(bkmk3Id, "folder", 2);

  var transactions = [];
  {
    let txn1 = new PlacesRemoveItemTransaction(bkmk1Id);
    transactions.push(txn1);
    let txn2 = new PlacesRemoveItemTransaction(bkmk2Id);
    transactions.push(txn2);
    let txn3 = new PlacesRemoveItemTransaction(bkmk3Id);
    transactions.push(txn3);
  }
  var txn = new PlacesAggregatedTransaction("RemoveItems", transactions);

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
  var newBkmk1Id = bmsvc.getIdForItemAt(root, 0);
  var newBkmk2Id = bmsvc.getIdForItemAt(root, 1);
  var newBkmk3Id = bmsvc.getIdForItemAt(root, 2);
  var newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  var newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  var newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  do_check_eq(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk1Id).spec, "http://www.mozilla.org/");
  do_check_eq(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3Id), "folder");
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, "http://www.mozilla.org/");
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3_3Id), "folder");
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
  newBkmk1Id = bmsvc.getIdForItemAt(root, 0);
  newBkmk2Id = bmsvc.getIdForItemAt(root, 1);
  newBkmk3Id = bmsvc.getIdForItemAt(root, 2);
  newBkmk3_1Id = bmsvc.getIdForItemAt(newBkmk3Id, 0);
  newBkmk3_2Id = bmsvc.getIdForItemAt(newBkmk3Id, 1);
  newBkmk3_3Id = bmsvc.getIdForItemAt(newBkmk3Id, 2);
  do_check_eq(bmsvc.getItemType(newBkmk1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk1Id).spec, "http://www.mozilla.org/");
  do_check_eq(bmsvc.getItemType(newBkmk2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getItemType(newBkmk3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3Id), "folder");
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_1Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_1Id), bmsvc.TYPE_BOOKMARK);
  do_check_eq(bmsvc.getBookmarkURI(newBkmk3_1Id).spec, "http://www.mozilla.org/");
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_2Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_2Id), bmsvc.TYPE_SEPARATOR);
  do_check_eq(bmsvc.getFolderIdForItem(newBkmk3_3Id), newBkmk3Id);
  do_check_eq(bmsvc.getItemType(newBkmk3_3Id), bmsvc.TYPE_FOLDER);
  do_check_eq(bmsvc.getItemTitle(newBkmk3_3Id), "folder");
  // Check last added back item id.
  // Notice items are restored in reverse order.
  do_check_eq(observer._itemAddedId, newBkmk1Id);

  // Test creating an item with child transactions.
  {
    var childTxns = [];
    var newDateAdded = Date.now() - 20000;
    let editDateAdddedTxn = new PlacesEditItemDateAddedTransaction(null, newDateAdded);
    childTxns.push(editDateAdddedTxn);
    var itemChildAnnoObj = { name: "testAnno/testInt",
                             type: Ci.nsIAnnotationService.TYPE_INT32,
                             flags: 0,
                             value: 123,
                             expires: Ci.nsIAnnotationService.EXPIRE_NEVER };
    let annoTxn = new PlacesSetItemAnnotationTransaction(null, itemChildAnnoObj);
    childTxns.push(annoTxn);
    var itemWChildTxn = new PlacesCreateBookmarkTransaction(uri("http://www.example.com"), root,
                                                            bmStartIndex, "Testing1", null, null,
                                                            childTxns);
    try {
      txnManager.doTransaction(itemWChildTxn); // Also testing doTransaction
      var itemId = (bmsvc.getBookmarkIdsForURI(uri("http://www.example.com")))[0];
      do_check_eq(observer._itemAddedId, itemId);
      do_check_eq(newDateAdded, bmsvc.getItemDateAdded(itemId));
      do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
      do_check_eq(observer._itemChanged_isAnnotationProperty, true);
      do_check_true(annosvc.itemHasAnnotation(itemId, itemChildAnnoObj.name))
      do_check_eq(annosvc.getItemAnnotation(itemId, itemChildAnnoObj.name),
                  itemChildAnnoObj.value);
      itemWChildTxn.undoTransaction();
      do_check_eq(observer._itemRemovedId, itemId);
      itemWChildTxn.redoTransaction();
      do_check_true(bmsvc.isBookmarked(uri("http://www.example.com")));
      var newId = (bmsvc.getBookmarkIdsForURI(uri("http://www.example.com")))[0];
      do_check_eq(newDateAdded, bmsvc.getItemDateAdded(newId));
      do_check_eq(observer._itemAddedId, newId);
      do_check_eq(observer._itemChangedProperty, "testAnno/testInt");
      do_check_eq(observer._itemChanged_isAnnotationProperty, true);
      do_check_true(annosvc.itemHasAnnotation(newId, itemChildAnnoObj.name))
      do_check_eq(annosvc.getItemAnnotation(newId, itemChildAnnoObj.name),
                  itemChildAnnoObj.value);
      itemWChildTxn.undoTransaction();
      do_check_eq(observer._itemRemovedId, newId);
    } catch (ex) {
      do_throw("Setting a child transaction in a createItem transaction did throw: " + ex);
    }
  }

  // Create a folder with child item transactions.
  var childItemTxn = new PlacesCreateBookmarkTransaction(uri("http://www.childItem.com"), root, bmStartIndex, "childItem");
  var folderWChildItemTxn = new PlacesCreateFolderTransaction("Folder", root, bmStartIndex, null, [childItemTxn]);
  try {
    txnManager.doTransaction(folderWChildItemTxn);
    var childItemId = (bmsvc.getBookmarkIdsForURI(uri("http://www.childItem.com")))[0];
    do_check_eq(observer._itemAddedId, childItemId);
    do_check_eq(observer._itemAddedIndex, 0);
    do_check_true(bmsvc.isBookmarked(uri("http://www.childItem.com")));
    folderWChildItemTxn.undoTransaction();
    do_check_false(bmsvc.isBookmarked(uri("http://www.childItem.com")));
    folderWChildItemTxn.redoTransaction();
    var newchildItemId = (bmsvc.getBookmarkIdsForURI(uri("http://www.childItem.com")))[0];
    do_check_eq(observer._itemAddedIndex, 0);
    do_check_eq(observer._itemAddedId, newchildItemId);
    do_check_true(bmsvc.isBookmarked(uri("http://www.childItem.com")));
    folderWChildItemTxn.undoTransaction();
    do_check_false(bmsvc.isBookmarked(uri("http://www.childItem.com")));
  } catch (ex) {
    do_throw("Setting a child item transaction in a createFolder transaction did throw: " + ex);
  }

  bmsvc.removeObserver(observer, false);
}
