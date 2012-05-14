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
 * The Original Code is Places Tagging Service unit test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Asaf Romano <mano@mozilla.com> (Original Author)
 *   Seth Spitzer <sspitzer@mozilla.com>
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

function run_test() {
  run_next_test();
}

add_test(function test_resolveNullBookmarkTitles() {
  let uri1 = uri("http://foo.tld/");
  let uri2 = uri("https://bar.tld/");

  addVisits([
    { uri: uri1, title: "foo title" },
    { uri: uri2, title: "bar title" }
  ], function () {
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                         uri1,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         null);
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                                         uri2,
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         null);

    PlacesUtils.tagging.tagURI(uri1, ["tag 1"]);
    PlacesUtils.tagging.tagURI(uri2, ["tag 2"]);

    let options = PlacesUtils.history.getNewQueryOptions();
    options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
    options.resultType = options.RESULTS_AS_TAG_CONTENTS;

    let query = PlacesUtils.history.getNewQuery();
    // if we don't set a tag folder, RESULTS_AS_TAG_CONTENTS will return all
    // tagged URIs
    let root = PlacesUtils.history.executeQuery(query, options).root;
    root.containerOpen = true;
    do_check_eq(root.childCount, 2);
    // actually RESULTS_AS_TAG_CONTENTS return results ordered by place_id DESC
    // so they are reversed
    do_check_eq(root.getChild(0).title, "bar title");
    do_check_eq(root.getChild(1).title, "foo title");
    root.containerOpen = false;

    run_next_test();
  });
});
