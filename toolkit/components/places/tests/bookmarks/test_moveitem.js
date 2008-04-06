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
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Marco Bonardo <mak77supereva.it>
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

// Get bookmark service
try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
}

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].
               getService(Ci.nsIAnnotationService);
} catch(ex) {
  do_throw("Could not get annotation service\n");
}

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {
    this._beginUpdateBatch = true;
  },
  onEndUpdateBatch: function() {
    this._endUpdateBatch = true;
  },
  onItemAdded: function(id, folder, index) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
  },
  onItemRemoved: function(id, folder, index) {
    this._itemRemovedId = id;
    this._itemRemovedFolder = folder;
    this._itemRemovedIndex = index;
  },
  onItemChanged: function(id, property, isAnnotationProperty, value) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  },
  onItemVisited: function(id, visitID, time) {
    this._itemVisitedId = id;
    this._itemVisitedVistId = visitID;
    this._itemVisitedTime = time;
  },
  onItemMoved: function(id, oldParent, oldIndex, newParent, newIndex) {
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
  },
};
bmsvc.addObserver(observer, false);

// get bookmarks menu folder id
var root = bmsvc.bookmarksMenuFolder;

var folder1;
var folder2;
var folder1_1;
var folder1_1_1;

// main
function run_test() {
  // create folders
  folder1 = bmsvc.createFolder(root, "Folder1", bmsvc.DEFAULT_INDEX);
  folder2 = bmsvc.createFolder(root, "Folder2", bmsvc.DEFAULT_INDEX);
  var folders = [folder1, folder2];

  // add items into folders
  for (var f = 0; f < folders.length; f++) {
    for (var i = 0 ;i < 10; i++) {
      bmsvc.insertBookmark(folders[f],
                           uri("http://test." + f + "." + i + ".mozilla.org/"),
                           bmsvc.DEFAULT_INDEX,
                           "test." + f + "." + i);
    }
  }

  // create subfolder in folder1 position 5
  folder1_1 = bmsvc.createFolder(folder1, "Folder1_1", 5);
  do_check_eq(bmsvc.getItemIndex(folder1_1), 5);
  do_check_eq(bmsvc.getFolderIdForItem(folder1_1), folder1);

  // add items into subfolder
  for (var i = 0 ;i < 10; i++) {
    bmsvc.insertBookmark(folder1_1,
                         uri("http://test." + i + ".mozilla.org/"),
                         bmsvc.DEFAULT_INDEX,
                         "test." + i);
  }

  // create subfolder in folder1_1
  folder1_1_1 = bmsvc.createFolder(folder1_1, "Folder1_1_1", bmsvc.DEFAULT_INDEX);

  // 1. move folder1_1 from folder1 to folder2, using default index (append)
  LOG("test 1");
  testMove(folder1_1, folder1, folder2, bmsvc.DEFAULT_INDEX);

  // 2. move folder1_1 from folder1 to folder2, using specified index
  LOG("test 2");
  testMove(folder1_1, folder1, folder2, 3);

  // 3. move folder1_1 from folder1 to folder1, using default index (append)
  LOG("test 3");
  testMove(folder1_1, folder1, folder1, bmsvc.DEFAULT_INDEX);

  // 4. move folder1_1 to itself
  LOG("test 4");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1));

  // 5. move folder1_1 from folder1 to folder1, one position upper
  LOG("test 5");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) - 1);

  // 6. move folder1_1 from folder1 to folder1, one position lower
  LOG("test 6");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) + 1);

  // 7. move folder1_1 from folder1 to folder1, valid number of position upper
  LOG("test 7");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) - 3);

  // 8. move folder1_1 from folder1 to folder1, valid number of position lower
  LOG("test 8");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) + 3);

  // 9. unable to move folder1_1 from folder1 to folder1, invalid number of position upper
  LOG("test 9");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) - 100, true);

  // 10. unable to move folder1_1 from folder1 to folder1, invalid number of position lower
  LOG("test 10");
  testMove(folder1_1, folder1, folder1, bmsvc.getItemIndex(folder1_1) + 100, true);

  // 11. unable to move folder1_1 to be its own parent
  LOG("test 11");
  testMove(folder1_1, folder1, folder1_1, bmsvc.DEFAULT_INDEX, true);

  // 12. unable to move folder1 to be a child of its child (folder1_1)
  LOG("test 12");
  testMove(folder1, root, folder1_1, bmsvc.DEFAULT_INDEX, true);

  // 13. unable to move folder1 to be a child of its grandchild (folder1_1_1)
  LOG("test 13");
  testMove(folder1, root, folder1_1_1, bmsvc.DEFAULT_INDEX, true);
}

function testMove(item, oldParent, newParent, index, expectingFailure) {
  var oldParentCount = getChildCount(oldParent);
  var oldIndex = bmsvc.getItemIndex(item);
  var newParentCount = getChildCount(newParent);
  var newIndex = index == bmsvc.DEFAULT_INDEX ? newParentCount: index;
  // fix expected index when moving down into same container
  if (oldParent == newParent && newIndex > oldIndex) {
    newIndex--;
  }

  try {
    bmsvc.moveItem(item, newParent, index);
  }
  catch (e) {
    if (expectingFailure)
      return;
    else
      do_throw("moveItem Failed");
  }

  if (newParent != oldParent || newIndex != oldIndex) {
    // check observer results
    // if we have not moved the item observer is not called
    do_check_eq(observer._itemMovedId, item);
    do_check_eq(observer._itemMovedOldParent, oldParent);
    do_check_eq(observer._itemMovedOldIndex, oldIndex);
    do_check_eq(observer._itemMovedNewParent, newParent);
    do_check_eq(observer._itemMovedNewIndex, newIndex);
  }

  // test number of children
  if (oldParent != newParent) {
    do_check_eq(getChildCount(oldParent), oldParentCount - 1);
    do_check_eq(getChildCount(newParent), newParentCount + 1);
  }
  else
    do_check_eq(getChildCount(newParent), oldParentCount);

  // test that the new index is properly stored
  do_check_eq(bmsvc.getItemIndex(item), newIndex);
  do_check_eq(bmsvc.getFolderIdForItem(item), newParent);

  if (oldParent != newParent) {
    // try to get index of the item from within the old parent folder
    // check that it has been really removed from there
    do_check_neq(bmsvc.getIdForItemAt(oldParent, oldIndex), item);
  }
  // check the index of the item within the new parent folder
  do_check_eq(bmsvc.getIdForItemAt(newParent, newIndex), item);

  if (index == bmsvc.DEFAULT_INDEX) {
    // try to get index of the last item within the new parent folder
    do_check_eq(bmsvc.getIdForItemAt(newParent, -1), item);
  }

  // revert the changes
  // adjust index when restoring movement into same container
  if (oldParent == newParent && oldIndex > newIndex)
    bmsvc.moveItem(item, oldParent, oldIndex + 1);
  else
    bmsvc.moveItem(item, oldParent, oldIndex);

  // check that we have reverted correctly
  do_check_eq(getChildCount(folder1), 11);
  do_check_eq(getChildCount(folder2), 10);
  do_check_eq(getChildCount(folder1_1), 11);
  do_check_eq(getChildCount(folder1_1_1), 0);
  do_check_eq(bmsvc.getItemIndex(folder1), 0);
  do_check_eq(bmsvc.getItemIndex(folder2), 1);
  do_check_eq(bmsvc.getItemIndex(folder1_1), 5);
  do_check_eq(bmsvc.getItemIndex(folder1_1_1), 10);
  do_check_eq(bmsvc.getItemIndex(item), oldIndex);
  do_check_eq(bmsvc.getFolderIdForItem(item), oldParent);
  do_check_eq(bmsvc.getIdForItemAt(oldParent, oldIndex), item);
}

function getChildCount(aFolderId) {
  var cc = -1;
  try {
    var options = histsvc.getNewQueryOptions();
    var query = histsvc.getNewQuery();
    query.setFolders([aFolderId], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    cc = rootNode.childCount;
    rootNode.containerOpen = false;
  } catch(ex) {
    do_throw("getChildCount failed: " + ex);
  }
  return cc;
}
