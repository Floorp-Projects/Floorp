/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Added with bug 468305 to test that onBeforeItemRemoved is dispatched before
 * removed always with the right id.
 */

////////////////////////////////////////////////////////////////////////////////
//// Globals and Constants

let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

////////////////////////////////////////////////////////////////////////////////
//// Observer

function Observer(aExpectedId)
{
}
Observer.prototype =
{
  checked: false,
  onItemMovedCalled: false,
  onItemRemovedCalled: false,
  onBeginUpdateBatch: function() {
  },
  onEndUpdateBatch: function() {
  },
  onItemAdded: function(id, folder, index, itemType, uri) {
  },
  onBeforeItemRemoved: function(id) {
    this.removedId = id;
  },
  onItemRemoved: function(id, folder, index) {
    do_check_false(this.checked);
    do_check_eq(this.removedId, id);
    this.onItemRemovedCalled = true;
  },
  onItemChanged: function(id, property, isAnnotationProperty, value) {
  },
  onItemVisited: function(id, visitID, time) {
  },
  onItemMoved: function(id, oldParent, oldIndex, newParent, newIndex) {
    this.onItemMovedCalled = true;
  },
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsINavBookmarkObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_removeItem()
{
  // First we add the item we are going to remove.
  let id = bs.insertBookmark(bs.unfiledBookmarksFolder,
                             uri("http://mozilla.org"), bs.DEFAULT_INDEX, "t");

  // Add our observer, and remove it.
  let observer = new Observer(id);
  bs.addObserver(observer, false);
  bs.removeItem(id);

  // Make sure we were notified!
  do_check_true(observer.onItemRemovedCalled);
  bs.removeObserver(observer);
}

function test_removeFolder()
{
  // First we add the item we are going to remove.
  let id = bs.createFolder(bs.unfiledBookmarksFolder, "t", bs.DEFAULT_INDEX);

  // Add our observer, and remove it.
  let observer = new Observer(id);
  bs.addObserver(observer, false);
  bs.removeItem(id);

  // Make sure we were notified!
  do_check_true(observer.onItemRemovedCalled);
  bs.removeObserver(observer);
}

function test_removeFolderChildren()
{
  // First we add the item we are going to remove.
  let fid = bs.createFolder(bs.unfiledBookmarksFolder, "tf", bs.DEFAULT_INDEX);
  let id = bs.insertBookmark(fid, uri("http://mozilla.org"), bs.DEFAULT_INDEX,
                             "t");

  // Add our observer, and remove it.
  let observer = new Observer(id);
  bs.addObserver(observer, false);
  bs.removeFolderChildren(fid);

  // Make sure we were notified!
  do_check_true(observer.onItemRemovedCalled);
  bs.removeObserver(observer);
}

function test_setItemIndex()
{
  // First we add the item we are going to move, and one additional one.
  let id = bs.insertBookmark(bs.unfiledBookmarksFolder,
                             uri("http://mozilla.org/1"), 0, "t1");
  bs.insertBookmark(bs.unfiledBookmarksFolder, uri("http://mozilla.org/2"), 1,
                    "t1");

  // Add our observer, and move it.
  let observer = new Observer(id);
  bs.addObserver(observer, false);
  bs.setItemIndex(id, 2);

  // Make sure we were notified!
  do_check_true(observer.onItemMovedCalled);
  bs.removeObserver(observer);
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_removeItem,
  test_removeFolder,
  test_removeFolderChildren,
  test_setItemIndex,
];
function run_test()
{
  tests.forEach(function(test) test());
}
