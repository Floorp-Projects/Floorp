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
 * The Original Code is Bug 388695 unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Asaf Romano <mano@mozilla.com>
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

var gTestRoot;
var gURI;
var gItemId1;
var gItemId2;

// main
function run_test() {
  gURI = uri("http://foo.tld.com/");
  gTestRoot = bmsvc.createFolder(bmsvc.placesRoot, "test folder",
                                 bmsvc.DEFAULT_INDEX);

  // test getBookmarkIdsForURI
  // getBookmarkIdsForURI sorts by the most recently added/modified (descending)
  //
  // we cannot rely on dateAdded growing when doing so in a simple iteration,
  // see PR_Now() documentation
  do_test_pending();

  gItemId1 = bmsvc.insertBookmark(gTestRoot, gURI, bmsvc.DEFAULT_INDEX, "");
  do_timeout(100, phase2);
}

function phase2() {
  gItemId2 = bmsvc.insertBookmark(gTestRoot, gURI, bmsvc.DEFAULT_INDEX, "");  
  var b = bmsvc.getBookmarkIdsForURI(gURI);
  do_check_eq(b[0], gItemId2);
  do_check_eq(b[1], gItemId1);
  do_timeout(100, phase3);
}

function phase3() {
  // trigger last modified change
  bmsvc.setItemTitle(gItemId1, "");
  var b = bmsvc.getBookmarkIdsForURI(gURI);
  do_check_eq(b[0], gItemId1);
  do_check_eq(b[1], gItemId2);
  do_test_finished();
}
