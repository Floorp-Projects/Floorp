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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
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

/**
 * Both SetItemtitle and insertBookmark should allow for null titles.
 */

const bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
           getService(Ci.nsINavBookmarksService);

const TEST_URL = "http://www.mozilla.org";

function run_test() {
  // Insert a bookmark with an empty title.
  var itemId = bs.insertBookmark(bs.toolbarFolder,
                                 uri(TEST_URL),
                                 bs.DEFAULT_INDEX,
                                 "");
  // Check returned title is an empty string.
  do_check_eq(bs.getItemTitle(itemId), "");
  // Set title to null.
  bs.setItemTitle(itemId, null);
  // Check returned title is null.
  do_check_eq(bs.getItemTitle(itemId), null);
  // Cleanup.
  bs.removeItem(itemId);

  // Insert a bookmark with a null title.
  itemId = bs.insertBookmark(bs.toolbarFolder,
                             uri(TEST_URL),
                             bs.DEFAULT_INDEX,
                             null);
  // Check returned title is null.
  do_check_eq(bs.getItemTitle(itemId), null);
  // Set title to an empty string.
  bs.setItemTitle(itemId, "");
  // Check returned title is an empty string.
  do_check_eq(bs.getItemTitle(itemId), "");
  // Cleanup.
  bs.removeItem(itemId);
}
