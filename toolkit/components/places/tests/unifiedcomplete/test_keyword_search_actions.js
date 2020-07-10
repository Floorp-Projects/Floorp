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

add_task(async function test_keyword_search() {
  let uri1 = "http://abc/?search=%s";
  let uri2 = "http://abc/?search=ThisPageIsInHistory";
  let uri3 = "http://abc/?search=%s&raw=%S";
  let uri4 = "http://abc/?search=%s&raw=%S&mozcharset=ISO-8859-1";
  let uri5 = "http://def/?search=%s";
  let uri6 = "http://ghi/?search=%s&raw=%S";
  await PlacesTestUtils.addVisits([
    { uri: uri1 },
    { uri: uri2 },
    { uri: uri3 },
    { uri: uri6 },
  ]);
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "Keyword",
    keyword: "key",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri1,
    title: "Post",
    keyword: "post",
    postData: "post_search=%s",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri3,
    title: "Encoded",
    keyword: "encoded",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri4,
    title: "Charset",
    keyword: "charset",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "Noparam",
    keyword: "noparam",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri2,
    title: "Noparam-Post",
    keyword: "post_noparam",
    postData: "noparam=1",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri5,
    title: "Keyword",
    keyword: "key2",
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: uri6,
    title: "Charset-history",
    keyword: "charset_history",
  });

  await PlacesUtils.history.update({
    url: uri6,
    annotations: new Map([[PlacesUtils.CHARSET_ANNO, "ISO-8859-1"]]),
  });

  info("Plain keyword query");
  await check_autocomplete({
    search: "key term",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=term",
          keyword: "key",
          input: "key term",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Plain keyword UC");
  await check_autocomplete({
    search: "key TERM",
    matches: [
      {
        uri: "http://abc/?search=TERM",
        title: "abc",
        style: ["keyword", "heuristic"],
      },
    ],
  });

  info("Multi-word keyword query");
  await check_autocomplete({
    search: "key multi word",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=multi%20word",
          keyword: "key",
          input: "key multi word",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Keyword query with +");
  await check_autocomplete({
    search: "key blocking+",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=blocking%2B",
          keyword: "key",
          input: "key blocking+",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Unescaped term in query");
  // ... but note that UnifiedComplete calls encodeURIComponent() on the query
  // string when it builds the URL, so the expected result will have the
  // ユニコード substring encoded in the URL.
  await check_autocomplete({
    search: "key ユニコード",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=" + encodeURIComponent("ユニコード"),
          keyword: "key",
          input: "key ユニコード",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Keyword that happens to match a page");
  await check_autocomplete({
    search: "key ThisPageIsInHistory",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=ThisPageIsInHistory",
          keyword: "key",
          input: "key ThisPageIsInHistory",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Keyword with partial page match");
  await check_autocomplete({
    search: "key ThisPage",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=ThisPage",
          keyword: "key",
          input: "key ThisPage",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
      // Only the most recent bookmark for the URL:
      {
        value: "http://abc/?search=ThisPageIsInHistory",
        title: "Noparam-Post",
        style: ["bookmark"],
      },
    ],
  });

  // For the keyword with no query terms (with or without space after), the
  // domain is different from the other tests because otherwise all the other
  // test bookmarks and history entries would be matches.
  info("Keyword without query (without space)");
  await check_autocomplete({
    search: "key2",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://def/?search=",
          keyword: "key2",
          input: "key2",
        }),
        style: ["action", "keyword", "heuristic"],
      },
      { uri: uri5, title: "Keyword", style: ["bookmark"] },
    ],
  });

  info("Keyword without query (with space)");
  await check_autocomplete({
    search: "key2 ",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://def/?search=",
          keyword: "key2",
          input: "key2 ",
        }),
        style: ["action", "keyword", "heuristic"],
      },
      { uri: uri5, title: "Keyword", style: ["bookmark"] },
    ],
  });

  info("POST Keyword");
  await check_autocomplete({
    search: "post foo",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=foo",
          keyword: "post",
          input: "post foo",
          postData: "post_search=foo",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Bug 420328: no-param keyword with a param");
  await check_autocomplete({
    search: "noparam foo",
    searchParam: "enable-actions",
    matches: [makeSearchMatch("noparam foo", { heuristic: true })],
  });
  await check_autocomplete({
    search: "post_noparam foo",
    searchParam: "enable-actions",
    matches: [makeSearchMatch("post_noparam foo", { heuristic: true })],
  });

  info("escaping with default UTF-8 charset");
  await check_autocomplete({
    search: "encoded foé",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=fo%C3%A9&raw=foé",
          keyword: "encoded",
          input: "encoded foé",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("escaping with forced ISO-8859-1 charset");
  await check_autocomplete({
    search: "charset foé",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=fo%E9&raw=foé",
          keyword: "charset",
          input: "charset foé",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("escaping with ISO-8859-1 charset annotated in history");
  await check_autocomplete({
    search: "charset_history foé",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://ghi/?search=fo%E9&raw=foé",
          keyword: "charset_history",
          input: "charset_history foé",
        }),
        title: "ghi",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Bug 359809: escaping +, / and @ with default UTF-8 charset");
  await check_autocomplete({
    search: "encoded +/@",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=%2B%2F%40&raw=+/@",
          keyword: "encoded",
          input: "encoded +/@",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Bug 359809: escaping +, / and @ with forced ISO-8859-1 charset");
  await check_autocomplete({
    search: "charset +/@",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=%2B%2F%40&raw=+/@",
          keyword: "charset",
          input: "charset +/@",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  info("Bug 1228111 - Keyword with a space in front");
  await check_autocomplete({
    search: " key test",
    searchParam: "enable-actions",
    matches: [
      {
        uri: makeActionURI("keyword", {
          url: "http://abc/?search=test",
          keyword: "key",
          input: " key test",
        }),
        title: "abc",
        style: ["action", "keyword", "heuristic"],
      },
    ],
  });

  await cleanup();
});
