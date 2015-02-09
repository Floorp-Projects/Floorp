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

add_task(function* test_keyword_search() {
  let uri1 = NetUtil.newURI("http://abc/?search=%s");
  let uri2 = NetUtil.newURI("http://abc/?search=ThisPageIsInHistory");
  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "Generic page title" },
    { uri: uri2, title: "Generic page title" }
  ]);
  addBookmark({ uri: uri1, title: "Keyword title", keyword: "key"});

  do_print("Plain keyword query");
  yield check_autocomplete({
    search: "key term",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=term", input: "key term"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Multi-word keyword query");
  yield check_autocomplete({
    search: "key multi word",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=multi+word", input: "key multi word"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Keyword query with +");
  yield check_autocomplete({
    search: "key blocking+",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=blocking%2B", input: "key blocking+"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Unescaped term in query");
  yield check_autocomplete({
    search: "key ユニコード",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=ユニコード", input: "key ユニコード"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Keyword that happens to match a page");
  yield check_autocomplete({
    search: "key ThisPageIsInHistory",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=ThisPageIsInHistory", input: "key ThisPageIsInHistory"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Keyword without query (without space)");
  yield check_autocomplete({
    search: "key",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=", input: "key"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  do_print("Keyword without query (with space)");
  yield check_autocomplete({
    search: "key ",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=", input: "key "}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  // This adds a second keyword so anything after this will match 2 keywords
  let uri3 = NetUtil.newURI("http://xyz/?foo=%s");
  yield PlacesTestUtils.addVisits([ { uri: uri3, title: "Generic page title" } ]);
  addBookmark({ uri: uri3, title: "Keyword title", keyword: "key"});

  do_print("Two keywords matched");
  yield check_autocomplete({
    search: "key twoKey",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=twoKey", input: "key twoKey"}), title: "Keyword title", style: [ "action", "keyword" ] },
               { uri: makeActionURI("keyword", {url: "http://xyz/?foo=twoKey", input: "key twoKey"}), title: "Keyword title", style: [ "action", "keyword" ] } ]
  });

  yield cleanup();
});
