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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
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
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
} catch(ex) {
  do_throw("Could not get nav-bookmarks-service\n");
}

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get annotation service
try {
  var annosvc= Cc["@mozilla.org/browser/annotation-service;1"].getService(Ci.nsIAnnotationService);
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
    this._itemAddedFolder = folder;
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
  onFolderAdded: function(folder, parent, index) {
    this._folderAdded = folder;
    this._folderAddedParent = parent;
    this._folderAddedIndex = index;
  },
  onFolderRemoved: function(folder, parent, index) {
    this._folderRemoved = folder;
    this._folderRemovedParent = parent;
    this._folderRemovedIndex = index;
  },
  onFolderMoved: function(folder, oldParent, oldIndex, newParent, newIndex) {
    this._folderMoved = folder;
    this._folderMovedOldParent = oldParent;
    this._folderMovedOldIndex = oldIndex;
    this._folderMovedNewParent = newParent;
    this._folderMovedNewIndex = newIndex;
  },
  onFolderChanged: function(folder, property) {
    this._folderChanged = folder;
    this._folderChangedProperty = property;
  },
  onSeparatorAdded: function(folder, index) {
    this._separatorAdded = folder;
    this._separatorAddedIndex = index;
  },
  onSeparatorRemoved: function(folder, index) {
    this._separatorRemoved = folder;
    this._separatorRemovedIndex = index;
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

// get bookmarks root index
var root = bmsvc.bookmarksRoot;

// index at which items should begin
var bmStartIndex = 1;

// main
function run_test() {
  // test roots
  do_check_true(bmsvc.placesRoot > 0);
  do_check_true(bmsvc.bookmarksRoot > 0);
  do_check_true(bmsvc.toolbarFolder > 0);

  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to default_places.html
  var testRoot = bmsvc.createFolder(root, "places bookmarks xpcshell tests", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._folderAdded, testRoot);
  do_check_eq(observer._folderAddedParent, root);
  do_check_eq(observer._folderAddedIndex, bmStartIndex);
  var testStartIndex = 0;

  // test getItemIndex for folders
  do_check_eq(bmsvc.getItemIndex(testRoot), bmStartIndex);

  // test getItemType for folders
  do_check_eq(bmsvc.getItemType(testRoot), bmsvc.TYPE_FOLDER);

  // insert a bookmark 
  var newId = bmsvc.insertItem(testRoot, uri("http://google.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId);
  do_check_eq(observer._itemAddedFolder, testRoot);
  do_check_eq(observer._itemAddedIndex, testStartIndex);
  do_check_eq(bmsvc.getBookmarkURI(newId).spec, "http://google.com/");

  // set bookmark title
  bmsvc.setItemTitle(newId, "Google");
  do_check_eq(observer._itemChangedId, newId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // get item title
  var title = bmsvc.getItemTitle(newId);
  do_check_eq(title, "Google");

  // test getItemType for bookmarks
  do_check_eq(bmsvc.getItemType(newId), bmsvc.TYPE_BOOKMARK);

  // get item title bad input
  try {
    var title = bmsvc.getItemTitle(-3);
    do_throw("getItemTitle accepted bad input");
  } catch(ex) {}

  // get the folder that the bookmark is in
  var folderId = bmsvc.getFolderIdForItem(newId);
  do_check_eq(folderId, testRoot);

  // test getItemIndex for bookmarks 
  do_check_eq(bmsvc.getItemIndex(newId), testStartIndex);

  // create a folder at a specific index
  var workFolder = bmsvc.createFolder(testRoot, "Work", 0);
  do_check_eq(observer._folderAdded, workFolder);
  do_check_eq(observer._folderAddedParent, testRoot);
  do_check_eq(observer._folderAddedIndex, 0);
  
  //XXX - test creating a folder at an invalid index

  //XXX - test setFolderTitle
  //XXX - test getFolderTitle

  // add item into subfolder, specifying index
  var newId2 = bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), 0);
  do_check_eq(observer._itemAddedId, newId2);
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId2, "DevMo");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item into subfolder
  var newId3 = bmsvc.insertItem(workFolder, uri("http://msdn.microsoft.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId3);
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);

  // change item
  bmsvc.setItemTitle(newId3, "MSDN");
  do_check_eq(observer._itemChangedProperty, "title");

  // remove item
  bmsvc.removeItem(newId2);
  do_check_eq(observer._itemRemovedId, newId2);
  do_check_eq(observer._itemRemovedFolder, workFolder);
  do_check_eq(observer._itemRemovedIndex, 0);

  // insert item into subfolder
  var newId4 = bmsvc.insertItem(workFolder, uri("http://developer.mozilla.org/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId4);
  do_check_eq(observer._itemAddedFolder, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);
  
  // create folder
  var homeFolder = bmsvc.createFolder(testRoot, "Home", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._folderAdded, homeFolder);
  do_check_eq(observer._folderAddedParent, testRoot);
  do_check_eq(observer._folderAddedIndex, 2);

  // insert item
  var newId5 = bmsvc.insertItem(homeFolder, uri("http://espn.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId5);
  do_check_eq(observer._itemAddedFolder, homeFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId5, "ESPN");
  do_check_eq(observer._itemChangedId, newId5);
  do_check_eq(observer._itemChangedProperty, "title");

  // insert query item
  var newId6 = bmsvc.insertItem(testRoot, uri("place:domain=google.com&group=1"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedFolder, testRoot);
  do_check_eq(observer._itemAddedIndex, 3);

  // change item
  bmsvc.setItemTitle(newId6, "Google Sites");
  do_check_eq(observer._itemChangedProperty, "title");

  // move folder, appending, to different folder
  var oldParentCC = getChildCount(testRoot);
  bmsvc.moveFolder(workFolder, homeFolder, bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._folderMoved, workFolder);
  do_check_eq(observer._folderMovedOldParent, testRoot);
  do_check_eq(observer._folderMovedOldIndex, 0);
  do_check_eq(observer._folderMovedNewParent, homeFolder);
  do_check_eq(observer._folderMovedNewIndex, 1);

  // test that the new index is properly stored
  do_check_eq(bmsvc.getItemIndex(workFolder), 1);

  // XXX use getFolderIdForItem to test for the new parent

  // try to get index of the folder from it's ex-parent
  // XXX expose getItemAtIndex(folder, idx) to test that the item was *removed* from the old parent?
  // XXX or expose FolderCount, and check that the old parent has one less kids?
  do_check_eq(getChildCount(testRoot), oldParentCC-1);

  // XXX move folder, specified index, to different folder
  // XXX move folder, specified index, within the same folder
  // XXX move folder, specify same index, within the same folder
  // XXX move folder, appending, within the same folder

  // XXX move item, appending, to different folder 
  // XXX move item, specified index, to different folder 
  // XXX move item, specified index, within the same folder 
  // XXX move item, specify same index, within the same folder
  // XXX move item, appending, within the same folder 

  // Test expected failure of moving a folder to be its own parent
  try {
    bmsvc.moveFolder(workFolder, workFolder, bmsvc.DEFAULT_INDEX);
    do_throw("moveFolder() allowed moving a folder to be it's own parent.");
  } catch (e) {}

  // test insertSeparator and removeChildAt
  // XXX - this should also query bookmarks for the folder children
  // and then test the node type at our index
  try {
    bmsvc.insertSeparator(testRoot, 1);
    bmsvc.removeChildAt(testRoot, 1);
  } catch(ex) {
    do_throw("insertSeparator: " + ex);
  }

  // XXX test getItemType for separators 
  // add when 379952 is fixed

  // removeChildAt w/ folder
  bmsvc.createFolder(testRoot, "tmp", 1);
  bmsvc.removeChildAt(testRoot, 1);

  // removeChildAt w/ bookmark
  bmsvc.insertItem(root, uri("http://blah.com"), 1);
  bmsvc.removeChildAt(root, 1);

  // test get folder's index 
  var tmpFolder = bmsvc.createFolder(testRoot, "tmp", 2);
  do_check_eq(bmsvc.getItemIndex(tmpFolder), 2);

  // test setKeywordForURI
  var kwTestItemId = bmsvc.insertItem(testRoot, uri("http://keywordtest.com"), bmsvc.DEFAULT_INDEX);
  try {
    bmsvc.setKeywordForBookmark(kwTestItemId, "bar");
  } catch(ex) {
    do_throw("setKeywordForBookmark: " + ex);
  }

  // test getKeywordForBookmark
  var k = bmsvc.getKeywordForBookmark(kwTestItemId);
  do_check_eq("bar", k);

  // test getKeywordForURI
  var k = bmsvc.getKeywordForURI(uri("http://keywordtest.com/"));
  do_check_eq("bar", k);

  // test getURIForKeyword
  var u = bmsvc.getURIForKeyword("bar");
  do_check_eq("http://keywordtest.com/", u.spec);

  // test getBookmarkIdsForURI
  var newId8 = bmsvc.insertItem(testRoot, uri("http://foo8.com/"), bmsvc.DEFAULT_INDEX);
  var b = bmsvc.getBookmarkIdsForURI(uri("http://foo8.com/"), {});
  do_check_eq(b[0], newId8);

  // test removeFolderChildren
  // 1) add/remove each child type (bookmark, separator, folder)
  var tmpFolder = bmsvc.createFolder(testRoot, "removeFolderChildren", bmsvc.DEFAULT_INDEX);
  bmsvc.insertItem(tmpFolder, uri("http://foo9.com/"), bmsvc.DEFAULT_INDEX);
  bmsvc.createFolder(tmpFolder, "subfolder", bmsvc.DEFAULT_INDEX);
  bmsvc.insertSeparator(tmpFolder, bmsvc.DEFAULT_INDEX);
  // 2) confirm that folder has 3 children
  try {
    var options = histsvc.getNewQueryOptions();
    var query = histsvc.getNewQuery();
    query.setFolders([tmpFolder], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
  } catch(ex) { do_throw("test removeFolderChildren() - querying for children failed: " + ex); }
  do_check_eq(rootNode.childCount, 3);
  rootNode.containerOpen = false;
  // 3) remove all children
  bmsvc.removeFolderChildren(tmpFolder);
  // 4) confirm that folder has 0 children
  try {
    result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    do_check_eq(rootNode.childCount, 0);
    rootNode.containerOpen = false;
  } catch(ex) { do_throw("removeFolderChildren(): " + ex); }

  // test getItemURI
  var newId9 = bmsvc.insertItem(testRoot, uri("http://foo9.com/"), bmsvc.DEFAULT_INDEX);
  var placeURI = bmsvc.getItemURI(newId9);
  do_check_eq(placeURI.spec, "place:moz_bookmarks.id=" + newId9 + "&group=3");

  // XXX - test folderReadOnly

  // test bookmark id in query output
  try {
    var options = histsvc.getNewQueryOptions();
    options.maxResults = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = histsvc.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    for (var i=0; i < cc; ++i) {
      var node = rootNode.getChild(i);
      if (node.type == node.RESULT_TYPE_FOLDER)
        do_check_eq(node.itemId, -1);
      else
        do_check_true(node.itemId > 0);
    }
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test that multiple bookmarks with same URI show up right in bookmark
  // folder queries, todo: also to do for complex folder queries
  try {
    // test uri
    var mURI = uri("http://multiple.uris.in.query");

    var testFolder = bmsvc.createFolder(testRoot, "test Folder", bmsvc.DEFAULT_INDEX);
    // add 2 bookmarks
    bmsvc.setItemTitle(bmsvc.insertItem(testFolder, mURI, bmsvc.DEFAULT_INDEX),
                       "title 1");
    bmsvc.setItemTitle(bmsvc.insertItem(testFolder, mURI, bmsvc.DEFAULT_INDEX),
                       "title 2");

    // query
    var options = histsvc.getNewQueryOptions();
    options.setGroupingMode([Ci.nsINavHistoryQueryOptions.GROUP_BY_FOLDER], 1);
    var query = histsvc.getNewQuery();
    query.setFolders([testFolder], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 2);
    do_check_eq(rootNode.getChild(0).title, "title 1");
    do_check_eq(rootNode.getChild(1).title, "title 2");
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test change bookmark uri
  var newId10 = bmsvc.insertItem(testRoot, uri("http://foo10.com/"), bmsvc.DEFAULT_INDEX);
  bmsvc.changeBookmarkURI(newId10, uri("http://foo11.com/"));
  do_check_eq(observer._itemChangedId, newId10);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://foo11.com/");

  // test getBookmarkURI
  var newId11 = bmsvc.insertItem(testRoot, uri("http://foo11.com/"), bmsvc.DEFAULT_INDEX);
  var bmURI = bmsvc.getBookmarkURI(newId11);
  do_check_eq("http://foo11.com/", bmURI.spec);

  // test getBookmarkURI with non-bookmark items
  try {
    bmsvc.getBookmarkURI(testRoot);
    do_throw("getBookmarkURI() should throw for non-bookmark items!");
  } catch(ex) {}

  // test getItemIndex
  var newId12 = bmsvc.insertItem(testRoot, uri("http://foo11.com/"), 1);
  var bmIndex = bmsvc.getItemIndex(newId12);
  do_check_eq(1, bmIndex);

  // test changing the bookmarks toolbar folder
  var oldToolbarFolder = bmsvc.toolbarFolder;
  var newToolbarFolderId = bmsvc.createFolder(testRoot, "new toolbar folder", -1);
  bmsvc.toolbarFolder = newToolbarFolderId;
  do_check_eq(bmsvc.toolbarFolder, newToolbarFolderId);
  do_check_eq(observer._itemChangedId, newToolbarFolderId);
  do_check_eq(observer._itemChangedProperty, "became_toolbar_folder");
  do_check_eq(observer._itemChangedValue, "");

  // insert a bookmark with title ZZZXXXYYY and then search for it.
  // this test confirms that we can find bookmarks that we haven't visited
  // (which are "hidden") and that we can find by title.
  // see bug #369887 for more details
  var newId13 = bmsvc.insertItem(testRoot, uri("http://foobarcheese.com/"), bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, newId13);
  do_check_eq(observer._itemAddedFolder, testRoot);
  do_check_eq(observer._itemAddedIndex, 13);

  // set bookmark title
  bmsvc.setItemTitle(newId13, "ZZZXXXYYY");
  do_check_eq(observer._itemChangedId, newId13);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "ZZZXXXYYY");

  // check if setting an item annotation triggers onItemChanged
  observer._itemChangedId = -1;
  annosvc.setItemAnnotationString(newId3, "test-annotation", "foo", 0, 0);
  do_check_eq(observer._itemChangedId, newId3);
  do_check_eq(observer._itemChangedProperty, "test-annotation");
  do_check_true(observer._itemChanged_isAnnotationProperty);
  do_check_eq(observer._itemChangedValue, "");

  // test search on bookmark title ZZZXXXYYY
  try {
    var options = histsvc.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var query = histsvc.getNewQuery();
    query.onlyBookmarked = true;
    query.searchTerms = "ZZZXXXYYY";
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 1);
    var node = rootNode.getChild(0);
    do_check_eq(node.title, "ZZZXXXYYY");
    do_check_true(node.itemId > 0);
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  // ensure that removing an item removes its annotations
  do_check_true(annosvc.itemHasAnnotation(newId3, "test-annotation"));
  bmsvc.removeItem(newId3);
  do_check_false(annosvc.itemHasAnnotation(newId3, "test-annotation"));

  // bug 378820
  var uri1 = uri("http://foo.tld/a");
  bmsvc.insertItem(testRoot, uri1, bmsvc.DEFAULT_INDEX);
  histsvc.addVisit(uri1, Date.now(), 0, histsvc.TRANSITION_TYPED, false, 0);
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
