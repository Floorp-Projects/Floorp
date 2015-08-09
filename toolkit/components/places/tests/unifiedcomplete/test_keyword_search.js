/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 392143 that puts keyword results into the autocomplete. Makes
 * sure that multiple parameter queries get spaces converted to +, + converted
 * to %2B, non-ascii become escaped, and pages in history that match the
 * keyword uses the page's title.
 *
 * Also test for bug 249468 by making sure multiple keyword bookmarks with the
 * same keyword appear in the list.
 */

add_task(function* test_keyword_searc() {
  let uri1 = NetUtil.newURI("http://abc/?search=%s");
  let uri2 = NetUtil.newURI("http://abc/?search=ThisPageIsInHistory");
  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "Generic page title" },
    { uri: uri2, title: "Generic page title" }
  ]);
  yield addBookmark({ uri: uri1, title: "Bookmark title", keyword: "key"});

  do_print("Plain keyword query");
  yield check_autocomplete({
    search: "key term",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=term"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Multi-word keyword query");
  yield check_autocomplete({
    search: "key multi word",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=multi+word"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Keyword query with +");
  yield check_autocomplete({
    search: "key blocking+",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=blocking%2B"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Unescaped term in query");
  yield check_autocomplete({
    search: "key ユニコード",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=ユニコード"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Keyword that happens to match a page");
  yield check_autocomplete({
    search: "key ThisPageIsInHistory",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=ThisPageIsInHistory"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Keyword without query (without space)");
  yield check_autocomplete({
    search: "key",
    matches: [ { uri: NetUtil.newURI("http://abc/?search="), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Keyword without query (with space)");
  yield check_autocomplete({
    search: "key ",
    matches: [ { uri: NetUtil.newURI("http://abc/?search="), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  yield cleanup();
});
