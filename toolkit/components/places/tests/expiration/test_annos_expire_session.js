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
 * The Original Code is Places Unit Tests.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net> (Original Author)
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
 * What this is aimed to test:
 *
 * Session annotations should be expired when browsing session ends.
 */

let os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
let hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
let bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
let as = Cc["@mozilla.org/browser/annotation-service;1"].
         getService(Ci.nsIAnnotationService);

function run_test() {
  do_test_pending();

  // Set interval to a large value so we don't expire on it.
  setInterval(3600); // 1h

  // Add some visited page and a couple session annotations for each.
  let now = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    let pageURI = uri("http://session_page_anno." + i + ".mozilla.org/");
    hs.addVisit(pageURI, now++, null, hs.TRANSITION_TYPED, false, 0);
    as.setPageAnnotation(pageURI, "test1", "test", 0, as.EXPIRE_SESSION);
    as.setPageAnnotation(pageURI, "test2", "test", 0, as.EXPIRE_SESSION);
  }

  // Add some bookmarked page and a couple session annotations for each.
  for (let i = 0; i < 10; i++) {
    let pageURI = uri("http://session_item_anno." + i + ".mozilla.org/");
    let id = bs.insertBookmark(bs.unfiledBookmarksFolder, pageURI,
                               bs.DEFAULT_INDEX, null);
    as.setItemAnnotation(id, "test1", "test", 0, as.EXPIRE_SESSION);
    as.setItemAnnotation(id, "test2", "test", 0, as.EXPIRE_SESSION);
  }


  let pages = as.getPagesWithAnnotation("test1");
  do_check_eq(pages.length, 10);
  pages = as.getPagesWithAnnotation("test2");
  do_check_eq(pages.length, 10);
  let items = as.getItemsWithAnnotation("test1");
  do_check_eq(items.length, 10);
  items = as.getItemsWithAnnotation("test2");
  do_check_eq(items.length, 10);

  waitForAsyncUpdates(function() {
    let stmt = DBConn().createStatement(
      "SELECT id FROM moz_annos "
    + "UNION "
    + "SELECT id FROM moz_items_annos "
    );
    do_check_false(stmt.executeStep());
    stmt.finalize();

    do_test_finished();
  });

  shutdownPlaces();
}
