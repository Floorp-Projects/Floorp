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
 * The Original Code is Bug 384370 code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

Components.utils.import("resource://gre/modules/utils.js");
var tests = [];


const DEFAULT_INDEX = PlacesUtils.bookmarks.DEFAULT_INDEX;

function run_test() {
  // folder to hold this test
  var folderId =
    PlacesUtils.bookmarks.createFolder(PlacesUtils.toolbarFolderId,
                                       "", DEFAULT_INDEX);

  // add a bookmark to the new folder
  var bookmarkURI = uri("http://iasdjkf");
  do_check_false(PlacesUtils.bookmarks.isBookmarked(bookmarkURI));
  var bookmarkId = PlacesUtils.bookmarks.insertBookmark(folderId, bookmarkURI,
                                                        DEFAULT_INDEX, "");
  do_check_eq(PlacesUtils.bookmarks.getItemTitle(bookmarkId), "");

  // try to remove the bookmark using removeFolder
  try {
    PlacesUtils.bookmarks.removeFolder(bookmarkId);
    do_throw("no exception when removing a bookmark via removeFolder()!");
  } catch(ex) {}
  do_check_true(PlacesUtils.bookmarks.isBookmarked(bookmarkURI));

  // remove the folder using removeItem
  PlacesUtils.bookmarks.removeItem(folderId);
  do_check_eq(PlacesUtils.bookmarks.getBookmarkIdsForURI(bookmarkURI, {}).length, 0);
  do_check_false(PlacesUtils.bookmarks.isBookmarked(bookmarkURI));
  do_check_eq(PlacesUtils.bookmarks.getItemIndex(bookmarkId), -1);
}
