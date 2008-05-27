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
 * The Original Code is Bug 360134 (item guids) unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
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

// main
function run_test() {
  var URI = uri("http://foo.tld.com/");
  var folderId = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                    bmsvc.DEFAULT_INDEX);
  var bookmarkId = bmsvc.insertBookmark(folderId, URI, bmsvc.DEFAULT_INDEX, "a title");
  var separatorId = bmsvc.insertSeparator(folderId, bmsvc.DEFAULT_INDEX);

  // get & uniqueness
  do_check_eq(bmsvc.getItemGUID(bookmarkId), bmsvc.getItemGUID(bookmarkId));

  do_check_neq(bmsvc.getItemGUID(folderId), bmsvc.getItemGUID(bookmarkId));
  do_check_neq(bmsvc.getItemGUID(folderId), "");

  do_check_neq(bmsvc.getItemGUID(separatorId), bmsvc.getItemGUID(bookmarkId));
  do_check_neq(bmsvc.getItemGUID(separatorId), "");

  // set
  bmsvc.setItemGUID(bookmarkId, "asdf123");
  do_check_eq(bmsvc.getItemGUID(bookmarkId), "asdf123");

  bmsvc.setItemGUID(folderId, "123asdf");
  do_check_eq(bmsvc.getItemGUID(folderId), "123asdf");

  try {
    bmsvc.setItemGUID(bookmarkId, "123asdf"); // should fail
  } catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  // GUID -> itemId
  do_check_eq(bmsvc.getItemIdForGUID("asdf123"), bookmarkId);
  do_check_eq(bmsvc.getItemIdForGUID("123asdf"), folderId);

}
