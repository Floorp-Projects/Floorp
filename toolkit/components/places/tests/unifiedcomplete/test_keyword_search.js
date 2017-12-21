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

add_task(async function test_keyword_searc() {
  let uri1 = NetUtil.newURI("http://abc/?search=%s");
  let uri2 = NetUtil.newURI("http://abc/?search=ThisPageIsInHistory");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "Generic page title" },
    { uri: uri2, title: "Generic page title" }
  ]);
  await addBookmark({ uri: uri1, title: "Bookmark title", keyword: "key"});

  info("Plain keyword query");
  await check_autocomplete({
    search: "key term",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=term"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Plain keyword UC");
  await check_autocomplete({
    search: "key TERM",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=TERM"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Multi-word keyword query");
  await check_autocomplete({
    search: "key multi word",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=multi%20word"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Keyword query with +");
  await check_autocomplete({
    search: "key blocking+",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=blocking%2B"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Unescaped term in query");
  await check_autocomplete({
    search: "key ユニコード",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=ユニコード"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Keyword that happens to match a page");
  await check_autocomplete({
    search: "key ThisPageIsInHistory",
    matches: [ { uri: NetUtil.newURI("http://abc/?search=ThisPageIsInHistory"), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Keyword without query (without space)");
  await check_autocomplete({
    search: "key",
    matches: [ { uri: NetUtil.newURI("http://abc/?search="), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  info("Keyword without query (with space)");
  await check_autocomplete({
    search: "key ",
    matches: [ { uri: NetUtil.newURI("http://abc/?search="), title: "abc", style: ["keyword", "heuristic"] } ]
  });

  await cleanup();
});
