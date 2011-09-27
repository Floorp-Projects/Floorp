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
 *  Dan Mills <thunder@mozilla.com>
 *  Marco Bonardo <mak77@supereva.it>
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

const TEST_URL = "http://mozilla.com/";
const TEST_SUBDOMAIN_URL = "http://foobar.mozilla.com/";

/**
 * Adds a page using addPageWithDetails API.
 *
 * @param aURL
 *        URLString of the page to be added.
 * @param [optional] aTime
 *        Microseconds from the epoch.  Current time if not specified.
 */
function add_page(aURL, aTime)
{
  PlacesUtils.bhistory.addPageWithDetails(NetUtil.newURI(aURL), "test",
                                          aTime || Date.now() * 1000);
}

add_test(function test_addPageWithDetails()
{
  add_page(TEST_URL);
  do_check_eq(TEST_URL, PlacesUtils.bhistory.lastPageVisited);
  do_check_eq(1, PlacesUtils.bhistory.count);
  run_next_test();
});

add_test(function test_removePage()
{
  PlacesUtils.bhistory.removePage(NetUtil.newURI(TEST_URL));
  do_check_eq(0, PlacesUtils.bhistory.count);
  do_check_eq("", PlacesUtils.bhistory.lastPageVisited);
  run_next_test();
});

add_test(function test_removePages()
{
  let pages = [];
  for (let i = 0; i < 8; i++) {
    pages.push(NetUtil.newURI(TEST_URL + i));
    add_page(TEST_URL + i);
  }

  // Bookmarked item should not be removed from moz_places.
  const ANNO_INDEX = 1;
  const ANNO_NAME = "testAnno";
  const ANNO_VALUE = "foo";
  const BOOKMARK_INDEX = 2;
  PlacesUtils.annotations.setPageAnnotation(pages[ANNO_INDEX],
                                            ANNO_NAME, ANNO_VALUE, 0,
                                            Ci.nsIAnnotationService.EXPIRE_NEVER);
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       pages[BOOKMARK_INDEX],
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "test bookmark");
  PlacesUtils.annotations.setPageAnnotation(pages[BOOKMARK_INDEX],
                                            ANNO_NAME, ANNO_VALUE, 0,
                                            Ci.nsIAnnotationService.EXPIRE_NEVER);

  PlacesUtils.bhistory.removePages(pages, pages.length);
  do_check_eq(0, PlacesUtils.bhistory.count);
  do_check_eq("", PlacesUtils.bhistory.lastPageVisited);

  // Check that the bookmark and its annotation still exist.
  do_check_true(PlacesUtils.bookmarks.getIdForItemAt(PlacesUtils.unfiledBookmarksFolderId, 0) > 0);
  do_check_eq(PlacesUtils.annotations.getPageAnnotation(pages[BOOKMARK_INDEX], ANNO_NAME),
              ANNO_VALUE);

  // Check the annotation on the non-bookmarked page does not exist anymore.
  try {
    PlacesUtils.annotations.getPageAnnotation(pages[ANNO_INDEX], ANNO_NAME);
    do_throw("did not expire expire_never anno on a not bookmarked item");
  } catch(ex) {}

  // Cleanup.
  PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  waitForClearHistory(run_next_test);
});

add_test(function test_removePagesByTimeframe()
{
  let startDate = Date.now() * 1000;
  for (let i = 0; i < 10; i++) {
    add_page(TEST_URL + i, startDate + i);
  }

  // Delete all pages except the first and the last.
  PlacesUtils.bhistory.removePagesByTimeframe(startDate + 1, startDate + 8);

  // Check that we have removed the correct pages.
  for (let i = 0; i < 10; i++) {
    do_check_eq(page_in_database(NetUtil.newURI(TEST_URL + i)) == 0,
                i > 0 && i < 9);
  }

  // Clear remaining items and check that all pages have been removed.
  PlacesUtils.bhistory.removePagesByTimeframe(startDate, startDate + 9);
  do_check_eq(0, PlacesUtils.bhistory.count);
  run_next_test();
});

add_test(function test_removePagesFromHost()
{
  add_page(TEST_URL);
  PlacesUtils.bhistory.removePagesFromHost("mozilla.com", true);
  do_check_eq(0, PlacesUtils.bhistory.count);
  run_next_test();
});

add_test(function test_removePagesFromHost_keepSubdomains()
{
  add_page(TEST_URL);
  add_page(TEST_SUBDOMAIN_URL);
  PlacesUtils.bhistory.removePagesFromHost("mozilla.com", false);
  do_check_eq(1, PlacesUtils.bhistory.count);
  run_next_test();
});

add_test(function test_removeAllPages()
{
  PlacesUtils.bhistory.removeAllPages();
  do_check_eq(0, PlacesUtils.bhistory.count);
  run_next_test();
});

function run_test()
{
  run_next_test();
}
