/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
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

/*
 * This test ensures favicons are correctly expired by expireAllFavicons API,
 * also when cache is cleared.
 */
var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var bh = hs.QueryInterface(Ci.nsIBrowserHistory);
var bs = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
         getService(Ci.nsINavBookmarksService);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var icons = Cc["@mozilla.org/browser/favicon-service;1"].
            getService(Ci.nsIFaviconService);
var cs = Cc["@mozilla.org/network/cache-service;1"].
         getService(Ci.nsICacheService);

const TEST_URI = "http://test.com/";
const TEST_ICON_URI = "http://test.com/favicon.ico";

const TEST_BOOKMARK_URI = "http://bookmarked.test.com/";
const TEST_BOOKMARK_ICON_URI = "http://bookmarked.test.com/favicon.ico";

const kExpirationFinished = "places-favicons-expired";

// TESTS
var tests = [
  function() {
    dump("\n\nTest that expireAllFavicons works as expected.\n");
    setup();
    icons.expireAllFavicons();
  },
];

function setup() {

  // Cleanup.
  remove_all_bookmarks();
  bh.removeAllPages();

  // Add a page with a bookmark.
  bs.insertBookmark(bs.toolbarFolder, uri(TEST_BOOKMARK_URI),
                    bs.DEFAULT_INDEX, "visited");
  // Set a favicon for the page.
  icons.setFaviconUrlForPage(uri(TEST_BOOKMARK_URI),
                             uri(TEST_BOOKMARK_ICON_URI));
  // Sanity check the favicon
  try {
    do_check_eq(icons.getFaviconForPage(uri(TEST_BOOKMARK_URI)).spec,
                TEST_BOOKMARK_ICON_URI);
  } catch(ex) {
    do_throw(ex.message);
  }

  // Add a visited page.
  visitId = hs.addVisit(uri(TEST_URI), Date.now() * 1000, null,
                        hs.TRANSITION_TYPED, false, 0);
  // Set a favicon for the page.
  icons.setFaviconUrlForPage(uri(TEST_URI), uri(TEST_ICON_URI));
  // Sanity check the favicon
  try {
    do_check_eq(icons.getFaviconForPage(uri(TEST_URI)).spec, TEST_ICON_URI);
  } catch(ex) {
    do_throw(ex.message);
  }
}

var observer = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == kExpirationFinished) {
      // Check visited page does not have an icon
      try {
        icons.getFaviconForPage(uri(TEST_URI));
        do_throw("Visited page has still a favicon!");
      } catch (ex) { /* page should not have a favicon */ }

      // Check bookmarked page does not have an icon
      try {
        icons.getFaviconForPage(uri(TEST_BOOKMARK_URI));
        do_throw("Bookmarked page has still a favicon!");
      } catch (ex) { /* page should not have a favicon */ }
    
      // Move to next test.
      if (tests.length)
        (tests.shift())();
      else {
        os.removeObserver(this, kExpirationFinished);
        do_test_finished();
      }
    }
  }
}
os.addObserver(observer, kExpirationFinished, false);

function run_test()
{
  do_test_pending();
  (tests.shift())();
}
