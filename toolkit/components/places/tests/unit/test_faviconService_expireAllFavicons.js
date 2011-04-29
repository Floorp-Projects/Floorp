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

const TEST_URI = "http://test.com/";
const TEST_ICON_URI = "http://test.com/favicon.ico";

const TEST_BOOKMARK_URI = "http://bookmarked.test.com/";
const TEST_BOOKMARK_ICON_URI = "http://bookmarked.test.com/favicon.ico";

const TOPIC_ICONS_EXPIRATION_FINISHED = "places-favicons-expired";

add_test(function() {
    do_log_info("Test that expireAllFavicons works as expected.");

    let bmURI = NetUtil.newURI(TEST_BOOKMARK_URI);
    let vsURI = NetUtil.newURI(TEST_URI);

    Services.obs.addObserver(function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(arguments.callee, aTopic);

      // Check visited page does not have an icon
      try {
        PlacesUtils.favicons.getFaviconForPage(vsURI);
        do_throw("Visited page has still a favicon!");
      } catch (ex) { /* page should not have a favicon */ }

      // Check bookmarked page does not have an icon
      try {
        PlacesUtils.favicons.getFaviconForPage(bmURI);
        do_throw("Bookmarked page has still a favicon!");
      } catch (ex) { /* page should not have a favicon */ }

      run_next_test();
    }, TOPIC_ICONS_EXPIRATION_FINISHED, false);

    // Add a page with a bookmark.
    PlacesUtils.bookmarks.insertBookmark(
      PlacesUtils.toolbarFolderId, bmURI,
      PlacesUtils.bookmarks.DEFAULT_INDEX, "Test bookmark"
    );

    // Set a favicon for the page.
    PlacesUtils.favicons.setFaviconUrlForPage(
      bmURI, NetUtil.newURI(TEST_BOOKMARK_ICON_URI)
    );
    do_check_eq(PlacesUtils.favicons.getFaviconForPage(bmURI).spec,
                TEST_BOOKMARK_ICON_URI);

    // Add a visited page.
    let visitId = PlacesUtils.history.addVisit(
      vsURI, Date.now() * 1000, null,
      PlacesUtils.history.TRANSITION_TYPED, false, 0
    );

    // Set a favicon for the page.
    PlacesUtils.favicons
               .setFaviconUrlForPage(vsURI, NetUtil.newURI(TEST_ICON_URI));
    do_check_eq(PlacesUtils.favicons.getFaviconForPage(vsURI).spec,
                TEST_ICON_URI);

    PlacesUtils.favicons.expireAllFavicons();
});

function run_test() {
  run_next_test();
}
