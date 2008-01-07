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

// index at which items should begin
var bmStartIndex = 0;

// main
function run_test() {
  // test special folders
  do_check_true(bmsvc.placesRoot > 0);
  do_check_true(bmsvc.bookmarksMenuFolder > 0);
  do_check_true(bmsvc.tagsFolder > 0);
  do_check_true(bmsvc.toolbarFolder > 0);
  do_check_true(bmsvc.unfiledBookmarksFolder > 0);

  // test getFolderIdForItem() with bogus item id will throw
  try {
    var title = bmsvc.getFolderIdForItem(0);
    do_throw("getFolderIdForItem accepted bad input");
  } catch(ex) {}

  // test getFolderIdForItem() with bogus item id will throw
  try {
    var title = bmsvc.getFolderIdForItem(-1);
    do_throw("getFolderIdForItem accepted bad input");
  } catch(ex) {}

  // test root parentage
  do_check_eq(bmsvc.getFolderIdForItem(bmsvc.bookmarksMenuFolder), bmsvc.placesRoot);
  do_check_eq(bmsvc.getFolderIdForItem(bmsvc.tagsFolder), bmsvc.placesRoot);
  do_check_eq(bmsvc.getFolderIdForItem(bmsvc.toolbarFolder), bmsvc.placesRoot);
  do_check_eq(bmsvc.getFolderIdForItem(bmsvc.unfiledBookmarksFolder), bmsvc.placesRoot);

  // create a folder to hold all the tests
  // this makes the tests more tolerant of changes to default_places.html
  var testRoot = bmsvc.createFolder(root, "places bookmarks xpcshell tests", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, testRoot);
  do_check_eq(observer._itemAddedParent, root);
  do_check_eq(observer._itemAddedIndex, bmStartIndex);
  var testStartIndex = 0;

  // test getItemIndex for folders
  do_check_eq(bmsvc.getItemIndex(testRoot), bmStartIndex);

  // test getItemType for folders
  do_check_eq(bmsvc.getItemType(testRoot), bmsvc.TYPE_FOLDER);

  // insert a bookmark 
  // the time before we insert, in microseconds
  var beforeInsert = Date.now() * 1000;
  do_check_true(beforeInsert > 0);

  var newId = bmsvc.insertBookmark(testRoot, uri("http://google.com/"),
                                   bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedId, newId);
  do_check_eq(observer._itemAddedParent, testRoot);
  do_check_eq(observer._itemAddedIndex, testStartIndex);
  do_check_eq(bmsvc.getBookmarkURI(newId).spec, "http://google.com/");

  var dateAdded = bmsvc.getItemDateAdded(newId);
  // dateAdded can equal beforeInsert
  do_check_true(dateAdded >= beforeInsert);

  // after just inserting, modified should not be set
  var lastModified = bmsvc.getItemLastModified(newId);
  do_check_eq(lastModified, 0);

  // the time before we set the title, in microseconds
  var beforeSetTitle = Date.now() * 1000;
  do_check_true(beforeSetTitle >= beforeInsert);

  // set bookmark title
  bmsvc.setItemTitle(newId, "Google");
  do_check_eq(observer._itemChangedId, newId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // check that dateAdded hasn't changed
  var dateAdded2 = bmsvc.getItemDateAdded(newId);
  do_check_eq(dateAdded2, dateAdded);

  // check lastModified after we set the title
  var lastModified2 = bmsvc.getItemLastModified(newId);
  LOG("test setItemTitle");
  LOG("dateAdded = " + dateAdded);
  LOG("beforeSetTitle = " + beforeSetTitle);
  LOG("lastModified = " + lastModified);
  LOG("lastModified2 = " + lastModified2);
  // XXX bug 381240
  //do_check_true(lastModified2 > lastModified);
  //do_check_true(lastModified2 >= dateAdded);
  //do_check_true(lastModified2 >= beforeSetTitle);

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
  do_check_eq(observer._itemAddedId, workFolder);
  do_check_eq(observer._itemAddedParent, testRoot);
  do_check_eq(observer._itemAddedIndex, 0);
  
  //XXX - test creating a folder at an invalid index

  do_check_eq(bmsvc.getItemTitle(workFolder), "Work");
  bmsvc.setItemTitle(workFolder, "Work #");
  do_check_eq(bmsvc.getItemTitle(workFolder), "Work #");

  // add item into subfolder, specifying index
  var newId2 = bmsvc.insertBookmark(workFolder, uri("http://developer.mozilla.org/"),
                                    0, "");
  do_check_eq(observer._itemAddedId, newId2);
  do_check_eq(observer._itemAddedParent, workFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId2, "DevMo");
  do_check_eq(observer._itemChangedProperty, "title");

  // insert item into subfolder
  var newId3 = bmsvc.insertBookmark(workFolder, uri("http://msdn.microsoft.com/"),
                                    bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedId, newId3);
  do_check_eq(observer._itemAddedParent, workFolder);
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
  var newId4 = bmsvc.insertBookmark(workFolder, uri("http://developer.mozilla.org/"),
                                    bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedId, newId4);
  do_check_eq(observer._itemAddedParent, workFolder);
  do_check_eq(observer._itemAddedIndex, 1);
  
  // create folder
  var homeFolder = bmsvc.createFolder(testRoot, "Home", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, homeFolder);
  do_check_eq(observer._itemAddedParent, testRoot);
  do_check_eq(observer._itemAddedIndex, 2);

  // insert item
  var newId5 = bmsvc.insertBookmark(homeFolder, uri("http://espn.com/"),
                                    bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedId, newId5);
  do_check_eq(observer._itemAddedParent, homeFolder);
  do_check_eq(observer._itemAddedIndex, 0);

  // change item
  bmsvc.setItemTitle(newId5, "ESPN");
  do_check_eq(observer._itemChangedId, newId5);
  do_check_eq(observer._itemChangedProperty, "title");

  // insert query item
  var newId6 = bmsvc.insertBookmark(testRoot, uri("place:domain=google.com&group=1"),
                                    bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedParent, testRoot);
  do_check_eq(observer._itemAddedIndex, 3);

  // change item
  bmsvc.setItemTitle(newId6, "Google Sites");
  do_check_eq(observer._itemChangedProperty, "title");

  // move folder, appending, to different folder
  var oldParentCC = getChildCount(testRoot);
  bmsvc.moveItem(workFolder, homeFolder, bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemMovedId, workFolder);
  do_check_eq(observer._itemMovedOldParent, testRoot);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, homeFolder);
  do_check_eq(observer._itemMovedNewIndex, 1);

  // test that the new index is properly stored
  do_check_eq(bmsvc.getItemIndex(workFolder), 1);
  do_check_eq(bmsvc.getFolderIdForItem(workFolder), homeFolder);

  // try to get index of the folder from it's ex-parent
  // XXX expose getItemAtIndex(folder, idx) to test that the item was *removed* from the old parent?
  // XXX or expose FolderCount, and check that the old parent has one less kids?
  do_check_eq(getChildCount(testRoot), oldParentCC-1);

  // XXX move folder, specified index, to different folder
  // XXX move folder, specified index, within the same folder
  // XXX move folder, specify same index, within the same folder
  // XXX move folder, appending, within the same folder

  // move item, appending, to different folder
  bmsvc.moveItem(newId5, testRoot, bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemMovedId, newId5);
  do_check_eq(observer._itemMovedOldParent, homeFolder);
  do_check_eq(observer._itemMovedOldIndex, 0);
  do_check_eq(observer._itemMovedNewParent, testRoot);
  do_check_eq(observer._itemMovedNewIndex, 3);

  // XXX move item, specified index, to different folder 
  // XXX move item, specified index, within the same folder 
  // XXX move item, specify same index, within the same folder
  // XXX move item, appending, within the same folder 

  // Test expected failure of moving a folder to be its own parent
  try {
    bmsvc.moveItem(workFolder, workFolder, bmsvc.DEFAULT_INDEX);
    do_throw("moveItem() allowed moving a folder to be its own parent.");
  } catch (e) {}

  // Test expected failure of moving a folder to be a child of its child
  // or of its grandchild.  see bug #383678 
  var childFolder = bmsvc.createFolder(workFolder, "childFolder", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, childFolder);
  do_check_eq(observer._itemAddedParent, workFolder);
  var grandchildFolder = bmsvc.createFolder(childFolder, "grandchildFolder", bmsvc.DEFAULT_INDEX);
  do_check_eq(observer._itemAddedId, grandchildFolder);
  do_check_eq(observer._itemAddedParent, childFolder);
  try {
    bmsvc.moveItem(workFolder, childFolder, bmsvc.DEFAULT_INDEX);
    do_throw("moveItem() allowed moving a folder to be a child of its child");
  } catch (e) {}
  try {
    bmsvc.moveItem(workFolder, grandchildFolder, bmsvc.DEFAULT_INDEX);
    do_throw("moveItem() allowed moving a folder to be a child of its grandchild");
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
  bmsvc.insertBookmark(root, uri("http://blah.com"), 1, "");
  bmsvc.removeChildAt(root, 1);

  // test get folder's index 
  var tmpFolder = bmsvc.createFolder(testRoot, "tmp", 2);
  do_check_eq(bmsvc.getItemIndex(tmpFolder), 2);

  // test setKeywordForBookmark
  var kwTestItemId = bmsvc.insertBookmark(testRoot, uri("http://keywordtest.com"),
                                          bmsvc.DEFAULT_INDEX, "");
  try {
    var dateAdded = bmsvc.getItemDateAdded(kwTestItemId);
    // after just inserting, modified should not be set
    var lastModified = bmsvc.getItemLastModified(kwTestItemId);
    do_check_eq(lastModified, 0);

    bmsvc.setKeywordForBookmark(kwTestItemId, "bar");

    var lastModified2 = bmsvc.getItemLastModified(kwTestItemId);
    LOG("test setKeywordForBookmark");
    LOG("dateAdded = " + dateAdded);
    LOG("lastModified = " + lastModified);
    LOG("lastModified2 = " + lastModified2);
    // XXX bug 381240
    //do_check_true(lastModified2 > lastModified);
    //do_check_true(lastModified2 >= dateAdded);
  } catch(ex) {
    do_throw("setKeywordForBookmark: " + ex);
  }

  var lastModified3 = bmsvc.getItemLastModified(kwTestItemId);
  // test getKeywordForBookmark
  var k = bmsvc.getKeywordForBookmark(kwTestItemId);
  do_check_eq("bar", k);

  // test getKeywordForURI
  var k = bmsvc.getKeywordForURI(uri("http://keywordtest.com/"));
  do_check_eq("bar", k);

  // test getURIForKeyword
  var u = bmsvc.getURIForKeyword("bar");
  do_check_eq("http://keywordtest.com/", u.spec);

  // test removeFolderChildren
  // 1) add/remove each child type (bookmark, separator, folder)
  var tmpFolder = bmsvc.createFolder(testRoot, "removeFolderChildren", bmsvc.DEFAULT_INDEX);
  bmsvc.insertBookmark(tmpFolder, uri("http://foo9.com/"), bmsvc.DEFAULT_INDEX,
                       "");
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

  // XXX - test folderReadOnly

  // test bookmark id in query output
  try {
    var options = histsvc.getNewQueryOptions();
    var query = histsvc.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    LOG("bookmark itemId test: CC = " + cc);
    do_check_true(cc > 0);
    for (var i=0; i < cc; ++i) {
      var node = rootNode.getChild(i);
      if (node.type == node.RESULT_TYPE_FOLDER ||
          node.type == node.RESULT_TYPE_URI ||
          node.type == node.RESULT_TYPE_SEPARATOR ||
          node.type == node.RESULT_TYPE_QUERY) {
        do_check_true(node.itemId > 0);
      }
      else {
        do_check_eq(node.itemId, -1);
      }
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
    bmsvc.insertBookmark(testFolder, mURI, bmsvc.DEFAULT_INDEX, "title 1");
    bmsvc.insertBookmark(testFolder, mURI, bmsvc.DEFAULT_INDEX, "title 2");

    // query
    var options = histsvc.getNewQueryOptions();
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
  var newId10 = bmsvc.insertBookmark(testRoot, uri("http://foo10.com/"),
                                     bmsvc.DEFAULT_INDEX, "");
  var dateAdded = bmsvc.getItemDateAdded(newId10);
  // after just inserting, modified should not be set
  var lastModified = bmsvc.getItemLastModified(newId10);
  do_check_eq(lastModified, 0);

  bmsvc.changeBookmarkURI(newId10, uri("http://foo11.com/"));

  // check that lastModified is set after we change the bookmark uri
  var lastModified2 = bmsvc.getItemLastModified(newId10);
  LOG("test changeBookmarkURI");
  LOG("dateAdded = " + dateAdded);
  LOG("lastModified = " + lastModified);
  LOG("lastModified2 = " + lastModified2);
  // XXX bug 381240
  //do_check_true(lastModified2 > lastModified);
  //do_check_true(lastModified2 >= dateAdded);

  do_check_eq(observer._itemChangedId, newId10);
  do_check_eq(observer._itemChangedProperty, "uri");
  do_check_eq(observer._itemChangedValue, "http://foo11.com/");

  // test getBookmarkURI
  var newId11 = bmsvc.insertBookmark(testRoot, uri("http://foo11.com/"),
                                     bmsvc.DEFAULT_INDEX, "");
  var bmURI = bmsvc.getBookmarkURI(newId11);
  do_check_eq("http://foo11.com/", bmURI.spec);

  // test getBookmarkURI with non-bookmark items
  try {
    bmsvc.getBookmarkURI(testRoot);
    do_throw("getBookmarkURI() should throw for non-bookmark items!");
  } catch(ex) {}

  // test getItemIndex
  var newId12 = bmsvc.insertBookmark(testRoot, uri("http://foo11.com/"), 1, "");
  var bmIndex = bmsvc.getItemIndex(newId12);
  do_check_eq(1, bmIndex);

  // insert a bookmark with title ZZZXXXYYY and then search for it.
  // this test confirms that we can find bookmarks that we haven't visited
  // (which are "hidden") and that we can find by title.
  // see bug #369887 for more details
  var newId13 = bmsvc.insertBookmark(testRoot, uri("http://foobarcheese.com/"),
                                     bmsvc.DEFAULT_INDEX, "");
  do_check_eq(observer._itemAddedId, newId13);
  do_check_eq(observer._itemAddedParent, testRoot);
  do_check_eq(observer._itemAddedIndex, 11);

  // set bookmark title
  bmsvc.setItemTitle(newId13, "ZZZXXXYYY");
  do_check_eq(observer._itemChangedId, newId13);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "ZZZXXXYYY");

  // check if setting an item annotation triggers onItemChanged
  observer._itemChangedId = -1;
  annosvc.setItemAnnotation(newId3, "test-annotation", "foo", 0, 0);
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

  // test dateAdded and lastModified properties
  // for a search query
  try {
    var options = histsvc.getNewQueryOptions();
    options.excludeQueries = 1;
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    var query = histsvc.getNewQuery();
    query.searchTerms = "ZZZXXXYYY";
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_eq(cc, 1);
    var node = rootNode.getChild(0);

    do_check_eq(typeof node.dateAdded, "number");
    do_check_true(node.dateAdded > 0);
    
    do_check_eq(typeof node.lastModified, "number");
    do_check_true(node.lastModified > 0);

    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  // test dateAdded and lastModified properties
  // for a folder query
  try {
    var options = histsvc.getNewQueryOptions();
    var query = histsvc.getNewQuery();
    query.setFolders([testRoot], 1);
    var result = histsvc.executeQuery(query, options);
    var rootNode = result.root;
    rootNode.containerOpen = true;
    var cc = rootNode.childCount;
    do_check_true(cc > 0);
    for (var i = 0; i < cc; i++) {
      var node = rootNode.getChild(i);

      if (node.type == node.RESULT_TYPE_URI) {
        do_check_eq(typeof node.dateAdded, "number");
        do_check_true(node.dateAdded > 0);

        do_check_eq(typeof node.lastModified, "number");
        do_check_true(node.lastModified > 0);
        break;
      }
    }
    rootNode.containerOpen = false;
  }
  catch(ex) {
    do_throw("bookmarks query: " + ex);
  }

  // check setItemLastModified() and setItemDateAdded()
  var newId14 = bmsvc.insertBookmark(testRoot, uri("http://bar.tld/"),
                                     bmsvc.DEFAULT_INDEX, "");
  var dateAdded = bmsvc.getItemDateAdded(newId14);
  var lastModified = bmsvc.getItemLastModified(newId14);
  do_check_eq(lastModified, 0);
  do_check_true(dateAdded > lastModified);
  bmsvc.setItemLastModified(newId14, 1234);
  var fakeLastModified = bmsvc.getItemLastModified(newId14);
  do_check_eq(fakeLastModified, 1234);
  bmsvc.setItemDateAdded(newId14, 4321);
  var fakeDateAdded = bmsvc.getItemDateAdded(newId14);
  do_check_eq(fakeDateAdded, 4321);
  
  // ensure that removing an item removes its annotations
  do_check_true(annosvc.itemHasAnnotation(newId3, "test-annotation"));
  bmsvc.removeItem(newId3);
  do_check_false(annosvc.itemHasAnnotation(newId3, "test-annotation"));

  // bug 378820
  var uri1 = uri("http://foo.tld/a");
  bmsvc.insertBookmark(testRoot, uri1, bmsvc.DEFAULT_INDEX, "");
  histsvc.addVisit(uri1, Date.now(), null, histsvc.TRANSITION_TYPED, false, 0);

  testSimpleFolderResult();
}

function testSimpleFolderResult() {
  // the time before we create a folder, in microseconds
  var beforeCreate = Date.now() * 1000;
  do_check_true(beforeCreate > 0);

  // create a folder
  var parent = bmsvc.createFolder(root, "test", bmsvc.DEFAULT_INDEX);

  var dateCreated = bmsvc.getItemDateAdded(parent);
  do_check_true(dateCreated > 0);
  // dateCreated can equal beforeCreate
  LOG("check that the folder was created with a valid dateAdded");
  LOG("beforeCreate = " + beforeCreate);
  LOG("dateCreated = " + dateCreated);
  // XXX bug 381240
  //do_check_true(dateCreated >= beforeCreate);

  // the time before we insert, in microseconds
  var beforeInsert = Date.now() * 1000;
  do_check_true(beforeInsert > 0);

  // insert a separator 
  var sep = bmsvc.insertSeparator(parent, bmsvc.DEFAULT_INDEX);

  var dateAdded = bmsvc.getItemDateAdded(sep);
  do_check_true(dateAdded > 0);
  // dateAdded can equal beforeInsert
  LOG("check that the separator was created with a valid dateAdded");
  LOG("beforeInsert = " + beforeInsert);
  LOG("dateAdded = " + dateAdded);
  // XXX bug 381240
  //do_check_true(dateAdded >= beforeInsert);

  // re-set item title separately so can test nodes' last modified
  var item = bmsvc.insertBookmark(parent, uri("about:blank"),
                                  bmsvc.DEFAULT_INDEX, "");
  bmsvc.setItemTitle(item, "test bookmark");

  // see above
  var folder = bmsvc.createFolder(parent, "test folder", bmsvc.DEFAULT_INDEX);
  bmsvc.setItemTitle(folder, "test folder");

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([parent], 1);
  var result = histsvc.executeQuery(query, options);
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 3);

  var node = rootNode.getChild(0);
  do_check_true(node.dateAdded > 0);
  do_check_eq(node.lastModified, 0);
  do_check_eq(node.itemId, sep);
  do_check_eq(node.title, "");
  node = rootNode.getChild(1);
  do_check_eq(node.itemId, item);
  do_check_true(node.dateAdded > 0);
  do_check_true(node.lastModified > 0);
  do_check_eq(node.title, "test bookmark");
  node = rootNode.getChild(2);
  do_check_eq(node.itemId, folder);
  do_check_eq(node.title, "test folder");
  do_check_true(node.dateAdded > 0);
  do_check_true(node.lastModified > 0);

  rootNode.containerOpen = false;
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
