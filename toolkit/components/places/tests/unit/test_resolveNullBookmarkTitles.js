/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  run_next_test();
}

add_test(function test_resolveNullBookmarkTitles() {
  let uri1 = uri("http://foo.tld/");
  let uri2 = uri("https://bar.tld/");

  PlacesTestUtils.addVisits([
    { uri: uri1, title: "foo title" },
    { uri: uri2, title: "bar title" }
  ]).then(function() {
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
