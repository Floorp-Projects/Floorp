/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is bug 395593 unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

const NUM_BOOKMARKS = 20;
const NUM_SEPARATORS = 5;
const NUM_FOLDERS = 10;
const NUM_ITEMS = NUM_BOOKMARKS + NUM_SEPARATORS + NUM_FOLDERS;
const MIN_RAND = -5;
const MAX_RAND = 40;

var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

function check_contiguous_indexes(aBookmarks) {
  var indexes = [];
  aBookmarks.forEach(function(aBookmarkId) {
    let bmIndex = bs.getItemIndex(aBookmarkId);
    dump("Index: " + bmIndex + "\n");
    dump("Checking duplicates\n");
    do_check_eq(indexes.indexOf(bmIndex), -1);
    dump("Checking out of range, found " + aBookmarks.length + " items\n");
    do_check_true(bmIndex >= 0 && bmIndex < aBookmarks.length);
    indexes.push(bmIndex);
  });
  dump("Checking all valid indexes have been used\n");
  do_check_eq(indexes.length, aBookmarks.length);
}

// main
function run_test() {
  var bookmarks = [];
  // Insert bookmarks with random indexes.
  for (let i = 0; bookmarks.length < NUM_BOOKMARKS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let id = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                 uri("http://" + i + ".mozilla.org/"),
                                 randIndex, "Test bookmark " + i);
      if (randIndex < -1)
        do_throw("Creating a bookmark at an invalid index should throw");
      bookmarks.push(id);
    }
    catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a bookmark at a valid index should not throw");
    }
  }
  check_contiguous_indexes(bookmarks);

  // Insert separators with random indexes.
  for (let i = 0; bookmarks.length < NUM_BOOKMARKS + NUM_SEPARATORS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let id = bs.insertSeparator(bs.unfiledBookmarksFolder, randIndex);
      if (randIndex < -1)
        do_throw("Creating a separator at an invalid index should throw");
      bookmarks.push(id);
    }
    catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a separator at a valid index should not throw");
    }
  }
  check_contiguous_indexes(bookmarks);

  // Insert folders with random indexes.
  for (let i = 0; bookmarks.length < NUM_ITEMS; i++) {
    let randIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    try {
      let id = bs.createFolder(bs.unfiledBookmarksFolder,
                               "Test folder " + i, randIndex);
      if (randIndex < -1)
        do_throw("Creating a folder at an invalid index should throw");
      bookmarks.push(id);
    }
    catch (ex) {
      if (randIndex >= -1)
        do_throw("Creating a folder at a valid index should not throw");
    }
  }
  check_contiguous_indexes(bookmarks);

  // Execute some random bookmark delete.
  for (let i = 0; i < Math.ceil(NUM_ITEMS / 4); i++) {
    let id = bookmarks.splice(Math.floor(Math.random() * bookmarks.length), 1);
    dump("Removing item with id " + id + "\n");
    bs.removeItem(id);
  }
  check_contiguous_indexes(bookmarks);

  // Execute some random bookmark move.  This will also try to move it to
  // invalid index values.
  for (let i = 0; i < Math.ceil(NUM_ITEMS / 4); i++) {
    let randIndex = Math.floor(Math.random() * bookmarks.length);
    let id = bookmarks[randIndex];
    let newIndex = Math.round(MIN_RAND + (Math.random() * (MAX_RAND - MIN_RAND)));
    dump("Moving item with id " + id + " to index " + newIndex + "\n");
    try {
      bs.moveItem(id, bs.unfiledBookmarksFolder, newIndex);
      if (newIndex < -1)
        do_throw("Moving an item to a negative index should throw\n");
    }
    catch (ex) {
      if (newIndex >= -1)
        do_throw("Moving an item to a valid index should not throw\n");
    }
    
  }
  check_contiguous_indexes(bookmarks);

  // Ensure setItemIndex throws if we pass it a negative index.
  try {
    bs.setItemIndex(bookmarks[0], -1);
    do_throw("setItemIndex should throw for a negative index");
  } catch (ex) {}
  // Ensure setItemIndex throws if we pass it a bad itemId.
  try {
    bs.setItemIndex(0, 5);
    do_throw("setItemIndex should throw for a bad itemId");
  } catch (ex) {}
}
