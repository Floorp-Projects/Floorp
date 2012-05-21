/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

try {
  var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get services\n");
}

// create and add bookmarks observer
var observer = {
  onBeginUpdateBatch: function() {},
  onEndUpdateBatch: function() {},
  onItemAdded: function(id, folder, index, itemType, uri) {
    this._itemAddedId = id;
    this._itemAddedParent = folder;
    this._itemAddedIndex = index;
  },
  onBeforeItemRemoved: function() {},
  onItemRemoved: function() {},
  _itemChangedProperty: null,
  onItemChanged: function(id, property, isAnnotationProperty, value,
                          lastModified, itemType) {
    this._itemChangedId = id;
    this._itemChangedProperty = property;
    this._itemChanged_isAnnotationProperty = isAnnotationProperty;
    this._itemChangedValue = value;
  },
  onItemVisited: function() {},
  onItemMoved: function() {},
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};
bmsvc.addObserver(observer, false);

// main
function run_test() {
  var testFolder = bmsvc.createFolder(bmsvc.placesRoot, "test Folder",
                                      bmsvc.DEFAULT_INDEX);
  var bookmarkId = bmsvc.insertBookmark(testFolder, uri("http://google.com/"),
                                   bmsvc.DEFAULT_INDEX, "");
  do_check_true(observer.itemChangedProperty == null);

  // We set lastModified in the past to workaround a timing bug on
  // virtual machines, see bug 427142 for details.
  var newDate = (Date.now() - 10) * 1000;
  bmsvc.setItemDateAdded(bookmarkId, newDate);
  // test notification
  do_check_eq(observer._itemChangedProperty, "dateAdded");
  do_check_eq(observer._itemChangedValue, newDate);
  // test saved value
  var dateAdded = bmsvc.getItemDateAdded(bookmarkId);
  do_check_eq(dateAdded, newDate);

  // after just inserting, modified should not be set
  var lastModified = bmsvc.getItemLastModified(bookmarkId);
  do_check_eq(lastModified, dateAdded);

  bmsvc.setItemLastModified(bookmarkId, newDate);
  // test notification
  do_check_eq(observer._itemChangedProperty, "lastModified");
  do_check_eq(observer._itemChangedValue, newDate);
  // test saved value
  do_check_eq(bmsvc.getItemLastModified(bookmarkId), newDate);

  // set bookmark title
  bmsvc.setItemTitle(bookmarkId, "Google");
  do_check_eq(observer._itemChangedId, bookmarkId);
  do_check_eq(observer._itemChangedProperty, "title");
  do_check_eq(observer._itemChangedValue, "Google");

  // check lastModified has been updated
  do_check_true(bmsvc.getItemLastModified(bookmarkId) > newDate);

  // check that node properties are updated 
  var query = histsvc.getNewQuery();
  query.setFolders([testFolder], 1);
  var result = histsvc.executeQuery(query, histsvc.getNewQueryOptions());
  var rootNode = result.root;
  rootNode.containerOpen = true;
  do_check_eq(rootNode.childCount, 1);
  var childNode = rootNode.getChild(0);

  // confirm current dates match node properties
  do_check_eq(bmsvc.getItemDateAdded(bookmarkId), childNode.dateAdded);
  do_check_eq(bmsvc.getItemLastModified(bookmarkId), childNode.lastModified);

  // test live update of lastModified caused by other changes:
  // We set lastModified in the past to workaround timers resolution,
  // see bug 427142 for details.
  var pastDate = (Date.now() - 10) * 1000;
  bmsvc.setItemLastModified(bookmarkId, pastDate);
  // set title (causing update of last modified)
  var oldLastModified = bmsvc.getItemLastModified(bookmarkId);
  bmsvc.setItemTitle(bookmarkId, "Google");
  // test that lastModified is updated
  is_time_ordered(oldLastModified, childNode.lastModified);
  // test that node value matches db value
  do_check_eq(bmsvc.getItemLastModified(bookmarkId), childNode.lastModified);

  // test live update of the exposed date apis
  newDate = Date.now() * 1000;
  bmsvc.setItemDateAdded(bookmarkId, newDate);
  do_check_eq(childNode.dateAdded, newDate);
  bmsvc.setItemLastModified(bookmarkId, newDate);
  do_check_eq(childNode.lastModified, newDate);

  rootNode.containerOpen = false;
}
