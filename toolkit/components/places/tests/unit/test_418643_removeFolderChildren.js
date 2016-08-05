/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get services.
try {
  var histSvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  var bmSvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
              getService(Ci.nsINavBookmarksService);
  var annoSvc = Cc["@mozilla.org/browser/annotation-service;1"]
                  .getService(Ci.nsIAnnotationService);
} catch (ex) {
  do_throw("Could not get services\n");
}

var validAnnoName = "validAnno";
var validItemName = "validItem";
var deletedAnnoName = "deletedAnno";
var deletedItemName = "deletedItem";
var bookmarkedURI = uri("http://www.mozilla.org/");
// set lastModified to the past to prevent VM timing bugs
var pastDate = Date.now() * 1000 - 1;
var deletedBookmarkIds = [];

// bookmarks observer
var observer = {
  // cached ordered array of notified items
  _onItemRemovedItemIds: [],
  onItemRemoved: function(aItemId, aParentId, aIndex) {
    // We should first get notifications for children, then for their parent
    do_check_eq(this._onItemRemovedItemIds.indexOf(aParentId), -1);
    // Ensure we are not wrongly removing 1 level up
    do_check_neq(aParentId, bmSvc.toolbarFolder);
    // Removed item must be one of those we have manually deleted
    do_check_neq(deletedBookmarkIds.indexOf(aItemId), -1);
    this._onItemRemovedItemIds.push(aItemId);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsINavBookmarkObserver) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

bmSvc.addObserver(observer, false);

function add_bookmarks() {
  // This is the folder we will cleanup
  var validFolderId = bmSvc.createFolder(bmSvc.toolbarFolder,
                                         validItemName,
                                         bmSvc.DEFAULT_INDEX);
  annoSvc.setItemAnnotation(validFolderId, validAnnoName,
                            "annotation", 0,
                            annoSvc.EXPIRE_NEVER);
  bmSvc.setItemLastModified(validFolderId, pastDate);

  // This bookmark should not be deleted
  var validItemId = bmSvc.insertBookmark(bmSvc.toolbarFolder,
                                         bookmarkedURI,
                                         bmSvc.DEFAULT_INDEX,
                                         validItemName);
  annoSvc.setItemAnnotation(validItemId, validAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);

  // The following contents should be deleted
  var deletedItemId = bmSvc.insertBookmark(validFolderId,
                                           bookmarkedURI,
                                           bmSvc.DEFAULT_INDEX,
                                           deletedItemName);
  annoSvc.setItemAnnotation(deletedItemId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkIds.push(deletedItemId);

  var internalFolderId = bmSvc.createFolder(validFolderId,
                                           deletedItemName,
                                           bmSvc.DEFAULT_INDEX);
  annoSvc.setItemAnnotation(internalFolderId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkIds.push(internalFolderId);

  deletedItemId = bmSvc.insertBookmark(internalFolderId,
                                       bookmarkedURI,
                                       bmSvc.DEFAULT_INDEX,
                                       deletedItemName);
  annoSvc.setItemAnnotation(deletedItemId, deletedAnnoName,
                            "annotation", 0, annoSvc.EXPIRE_NEVER);
  deletedBookmarkIds.push(deletedItemId);

  return validFolderId;
}

function check_bookmarks(aFolderId) {
  // check that we still have valid bookmarks
  var bookmarks = bmSvc.getBookmarkIdsForURI(bookmarkedURI);
  for (var i = 0; i < bookmarks.length; i++) {
    do_check_eq(bmSvc.getItemTitle(bookmarks[i]), validItemName);
    do_check_true(annoSvc.itemHasAnnotation(bookmarks[i],validAnnoName));
  }

  // check that folder exists and has still its annotation
  do_check_eq(bmSvc.getItemTitle(aFolderId), validItemName);
  do_check_true(annoSvc.itemHasAnnotation(aFolderId, validAnnoName));

  // check that folder is empty
  var options = histSvc.getNewQueryOptions();
  var query = histSvc.getNewQuery();
  query.setFolders([aFolderId], 1);
  var result = histSvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 0);
  root.containerOpen = false;

  // test that lastModified got updated
  do_check_true(pastDate < bmSvc.getItemLastModified(aFolderId));

  // test that all children have been deleted, we use annos for that
  var deletedItems = annoSvc.getItemsWithAnnotation(deletedAnnoName);
  do_check_eq(deletedItems.length, 0);

  // test that observer has been called for (and only for) deleted items
  do_check_eq(observer._onItemRemovedItemIds.length, deletedBookmarkIds.length);

  // Sanity check: all roots should be intact
  do_check_eq(bmSvc.getFolderIdForItem(bmSvc.placesRoot), 0);
  do_check_eq(bmSvc.getFolderIdForItem(bmSvc.bookmarksMenuFolder), bmSvc.placesRoot);
  do_check_eq(bmSvc.getFolderIdForItem(bmSvc.tagsFolder), bmSvc.placesRoot);
  do_check_eq(bmSvc.getFolderIdForItem(bmSvc.unfiledBookmarksFolder), bmSvc.placesRoot);
  do_check_eq(bmSvc.getFolderIdForItem(bmSvc.toolbarFolder), bmSvc.placesRoot);
}

// main
function run_test() {
  var folderId = add_bookmarks();
  bmSvc.removeFolderChildren(folderId);
  check_bookmarks(folderId);
}
