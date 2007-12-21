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

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// Get global history service
try {
  var bhist = Cc["@mozilla.org/browser/global-history;2"].getService(Ci.nsIBrowserHistory);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

// adds a test URI visit to the database, and checks for a valid place ID
function add_visit(aURI, aDate) {
  var date = aDate || Date.now();
  var placeID = histsvc.addVisit(aURI,
                                 date,
                                 null, // no referrer
                                 histsvc.TRANSITION_TYPED, // user typed in URL bar
                                 false, // not redirect
                                 0);
  do_check_true(placeID > 0);
  return placeID;
}

var viewer = {
  insertedItem: null,
  itemInserted: function(parent, item, newIndex) {
    this.insertedItem = item;
  },
  removedItem: null,
  itemRemoved: function(parent, item, oldIndex) {
    this.removedItem = item;
  },
  changedItem: null,
  itemChanged: function(item) {
    this.changedItem = item;
  },
  replacedItem: null,
  itemReplaced: function(parent, oldItem, newItem, index) {
    dump("itemReplaced: " + newItem.uri + "\n");
    this.replacedItem = item;
  },
  openedContainer: null,
  containerOpened: function(item) {
    this.openedContainer = item;
  },
  closedContainer: null,
  containerClosed: function(item) {
    this.closedContainer = item;
  },
  invalidatedContainer: null,
  invalidateContainer: function(item) {
    dump("invalidateContainer()\n");
    this.invalidatedContainer = item;
  },
  allInvalidated: null,
  invalidateAll: function() {
    this.allInvalidated = true;
  },
  sortingMode: null,
  sortingChanged: function(sortingMode) {
    this.sortingMode = sortingMode;
  },
  result: null,
  ignoreInvalidateContainer: false,
  addViewObserver: function(observer, ownsWeak) {},
  removeViewObserver: function(observer) {},
  reset: function() {
    this.insertedItem = null;
    this.removedItem = null;
    this.changedItem = null;
    this.replacedItem = null;
    this.openedContainer = null;
    this.closedContainer = null;
    this.invalidatedContainer = null;
    this.allInvalidated = null;
    this.sortingMode = null;
  }
};

// main
function run_test() {

  // history query
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  result.viewer = viewer;
  var root = result.root;
  root.containerOpen = true;

  // nsINavHistoryResultViewer.containerOpened
  do_check_neq(viewer.openedContainer, null);

  // nsINavHistoryResultViewer.itemInserted
  // add a visit
  var testURI = uri("http://mozilla.com");
  add_visit(testURI);
  do_check_eq(testURI.spec, viewer.insertedItem.uri);

  // nsINavHistoryResultViewer.itemChanged
  // adding a visit causes itemChanged for the folder
  do_check_eq(root.uri, viewer.changedItem.uri);

  // nsINavHistoryResultViewer.itemChanged for a leaf node
  bhist.addPageWithDetails(testURI, "baz", Date.now());
  do_check_eq(viewer.changedItem.title, "baz");

  // nsINavHistoryResultViewer.itemRemoved
  var removedURI = uri("http://google.com");
  add_visit(removedURI);
  bhist.removePage(removedURI);
  do_check_eq(removedURI.spec, viewer.removedItem.uri);

  // XXX nsINavHistoryResultViewer.itemReplaced
  // NHQRN.onVisit()->NHCRN.MergeResults()->NHCRN.ReplaceChildURIAt()->NHRV.ItemReplaced()

  // nsINavHistoryResultViewer.invalidateContainer
  bhist.removePagesFromHost("mozilla.com", false);
  do_check_eq(root.uri, viewer.invalidatedContainer.uri);

  // nsINavHistoryResultViewer.invalidateAll
  // nsINavHistoryResultViewer.sortingChanged
  result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  do_check_true(viewer.allInvalidated);
  do_check_eq(viewer.sortingMode, options.SORT_BY_TITLE_ASCENDING);

  // nsINavHistoryResultViewer.containerClosed
  root.containerOpen = false;
  do_check_eq(viewer.closedContainer, viewer.openedContainer);
  result.viewer = null;

  // bookmarks query
  
  // reset the viewer
  viewer.reset();

  try {
    var bmsvc = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].getService(Ci.nsINavBookmarksService);
  } catch(ex) {
    do_throw("Could not get nav-bookmarks-service\n");
  }

  var options = histsvc.getNewQueryOptions();
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.bookmarksMenuFolder], 1);
  var result = histsvc.executeQuery(query, options);
  result.viewer = viewer;
  var root = result.root;
  root.containerOpen = true;

  // nsINavHistoryResultViewer.containerOpened
  do_check_neq(viewer.openedContainer, null);

  // nsINavHistoryResultViewer.itemInserted
  // add a bookmark
  var testBookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, testURI, bmsvc.DEFAULT_INDEX, "foo");
  do_check_eq("foo", viewer.insertedItem.title);
  do_check_eq(testURI.spec, viewer.insertedItem.uri);

  // nsINavHistoryResultViewer.itemChanged
  // adding a visit causes itemChanged for the folder
  do_check_eq(root.uri, viewer.changedItem.uri);

  // nsINavHistoryResultViewer.itemChanged for a leaf node
  bmsvc.setItemTitle(testBookmark, "baz");
  do_check_eq(viewer.changedItem.title, "baz");

  // nsINavHistoryResultViewer.itemRemoved
  var removedBookmark = bmsvc.insertBookmark(bmsvc.bookmarksMenuFolder, uri("http://google.com"), bmsvc.DEFAULT_INDEX, "foo");
  bmsvc.removeItem(removedBookmark);
  do_check_eq(removedBookmark, viewer.removedItem.itemId);

  // XXX nsINavHistoryResultViewer.itemReplaced
  // NHQRN.onVisit()->NHCRN.MergeResults()->NHCRN.ReplaceChildURIAt()->NHRV.ItemReplaced()

  // XXX nsINavHistoryResultViewer.invalidateContainer

  // nsINavHistoryResultViewer.invalidateAll
  // nsINavHistoryResultViewer.sortingChanged
  result.sortingMode = options.SORT_BY_TITLE_ASCENDING;
  do_check_true(viewer.allInvalidated);
  do_check_eq(viewer.sortingMode, options.SORT_BY_TITLE_ASCENDING);

  // nsINavHistoryResultViewer.containerClosed
  root.containerOpen = false;
  do_check_eq(viewer.closedContainer, viewer.openedContainer);
  result.viewer = null;
}
