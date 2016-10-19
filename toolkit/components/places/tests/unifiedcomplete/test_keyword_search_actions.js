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
  let uri3 = NetUtil.newURI("http://abc/?search=%s&raw=%S");
  let uri4 = NetUtil.newURI("http://abc/?search=%s&raw=%S&mozcharset=ISO-8859-1");
  yield PlacesTestUtils.addVisits([{ uri: uri1 },
                                   { uri: uri2 },
                                   { uri: uri3 }]);
  yield addBookmark({ uri: uri1, title: "Keyword", keyword: "key"});
  yield addBookmark({ uri: uri1, title: "Post", keyword: "post", postData: "post_search=%s"});
  yield addBookmark({ uri: uri3, title: "Encoded", keyword: "encoded"});
  yield addBookmark({ uri: uri4, title: "Charset", keyword: "charset"});
  yield addBookmark({ uri: uri2, title: "Noparam", keyword: "noparam"});
  yield addBookmark({ uri: uri2, title: "Noparam-Post", keyword: "post_noparam", postData: "noparam=1"});

  do_print("Plain keyword query");
  yield check_autocomplete({
    search: "key term",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=term", input: "key term"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Plain keyword UC");
  yield check_autocomplete({
    search: "key TERM",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=TERM"),
                 title: "abc", style: ["keyword", "heuristic"] } ]
  });

  do_print("Multi-word keyword query");
  yield check_autocomplete({
    search: "key multi word",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=multi%20word", input: "key multi word"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Keyword query with +");
  yield check_autocomplete({
    search: "key blocking+",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=blocking%2B", input: "key blocking+"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Unescaped term in query");
  // ... but note that UnifiedComplete calls encodeURIComponent() on the query
  // string when it builds the URL, so the expected result will have the
  // ユニコード substring encoded in the URL.
  yield check_autocomplete({
    search: "key ユニコード",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=" + encodeURIComponent("ユニコード"), input: "key ユニコード"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Keyword that happens to match a page");
  yield check_autocomplete({
    search: "key ThisPageIsInHistory",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=ThisPageIsInHistory", input: "key ThisPageIsInHistory"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Keyword without query (without space)");
  yield check_autocomplete({
    search: "key",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=", input: "key"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Keyword without query (with space)");
  yield check_autocomplete({
    search: "key ",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=", input: "key "}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("POST Keyword");
  yield check_autocomplete({
    search: "post foo",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=foo", input: "post foo", postData: "post_search=foo"}),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Bug 420328: no-param keyword with a param");
  yield check_autocomplete({
    search: "noparam foo",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("noparam foo", { heuristic: true }) ]
  });
  yield check_autocomplete({
    search: "post_noparam foo",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("post_noparam foo", { heuristic: true }) ]
  });

  do_print("escaping with default UTF-8 charset");
  yield check_autocomplete({
    search: "encoded foé",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=fo%C3%A9&raw=foé", input: "encoded foé" }),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("escaping with forced ISO-8859-1 charset");
  yield check_autocomplete({
    search: "charset foé",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=fo%E9&raw=foé", input: "charset foé" }),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Bug 359809: escaping +, / and @ with default UTF-8 charset");
  yield check_autocomplete({
    search: "encoded +/@",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=%2B%2F%40&raw=+/@", input: "encoded +/@" }),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  do_print("Bug 359809: escaping +, / and @ with forced ISO-8859-1 charset");
  yield check_autocomplete({
    search: "charset +/@",
    searchParam: "enable-actions",
    matches: [ { uri: makeActionURI("keyword", {url: "http://abc/?search=%2B%2F%40&raw=+/@", input: "charset +/@" }),
                 title: "abc", style: [ "action", "keyword", "heuristic" ] } ]
  });

  yield cleanup();
});
