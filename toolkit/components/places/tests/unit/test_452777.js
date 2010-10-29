/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 expandtab
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
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
 * This test ensures that when removing a folder within a transaction, undoing
 * the transaction restores it with the same id (as received by the observers).
 */

var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);

function run_test()
{
  const TITLE = "test folder";

  // Create two test folders; remove the first one.  This ensures that undoing
  // the removal will not get the same id by chance (the insert id's can be
  // reused in SQLite).
  let id = bs.createFolder(bs.placesRoot, TITLE, -1);
  bs.createFolder(bs.placesRoot, "test folder 2", -1);
  let transaction = bs.getRemoveFolderTransaction(id);
  transaction.doTransaction();

  // Now check to make sure it gets added with the right id
  bs.addObserver({
    onItemAdded: function(aItemId, aFolder, aIndex, aItemType, aURI)
    {
      do_check_eq(aItemId, id);
      do_check_eq(bs.getItemTitle(aItemId), TITLE);
    }
  }, false);
  transaction.undoTransaction();
}
