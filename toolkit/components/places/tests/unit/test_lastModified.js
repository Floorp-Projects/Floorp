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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
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

 /**
  * Test that inserting a new bookmark will set lastModified to the same
  * values as dateAdded.
  */
// main
function run_test() {
  var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);
  var itemId = bs.insertBookmark(bs.bookmarksMenuFolder,
                                 uri("http://www.mozilla.org/"),
                                 bs.DEFAULT_INDEX,
                                 "itemTitle");
  var dateAdded = bs.getItemDateAdded(itemId);
  do_check_eq(dateAdded, bs.getItemLastModified(itemId));

  // Change lastModified, then change dateAdded.  LastModified should be set
  // to the new dateAdded.
  // This could randomly fail on virtual machines due to timing issues, so
  // we manually increase the time value.  See bug 500640 for details.
  bs.setItemLastModified(itemId, dateAdded + 1);
  do_check_true(bs.getItemLastModified(itemId) === dateAdded + 1);
  do_check_true(bs.getItemDateAdded(itemId) < bs.getItemLastModified(itemId));
  bs.setItemDateAdded(itemId, dateAdded + 2);
  do_check_true(bs.getItemDateAdded(itemId) === dateAdded + 2);
  do_check_eq(bs.getItemDateAdded(itemId), bs.getItemLastModified(itemId));

  bs.removeItem(itemId);
}
