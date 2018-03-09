/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gTabRestrictChar = "%";

add_task(async function test_tab_matches() {
  let uri1 = NetUtil.newURI("http://abc.com/");
  let uri2 = NetUtil.newURI("http://xyz.net/");
  let uri3 = NetUtil.newURI("about:mozilla");
  let uri4 = NetUtil.newURI("data:text/html,test");
  let uri5 = NetUtil.newURI("http://foobar.org");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "ABC rocks" },
    { uri: uri2, title: "xyz.net - we're better than ABC" },
    { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ" }
  ]);
  addOpenPages(uri1, 1);
  // Pages that cannot be registered in history.
  addOpenPages(uri3, 1);
  addOpenPages(uri4, 1);

  info("two results, normal result is a tab match");
  await check_autocomplete({
    search: "abc.com",
    searchParam: "enable-actions",
    matches: [ makeVisitMatch("abc.com", "http://abc.com/", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               makeSearchMatch("abc.com", { heuristic: false }) ]
  });

  info("three results, one tab match");
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] },
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("three results, both normal results are tab matches");
  addOpenPages(uri2, 1);
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               makeSwitchToTabMatch("http://xyz.net/", { title: "xyz.net - we're better than ABC" }),
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("a container tab is not visible in 'switch to tab'");
  addOpenPages(uri5, 1, /* userContextId: */ 3);
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               makeSwitchToTabMatch("http://xyz.net/", { title: "xyz.net - we're better than ABC" }),
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("a container tab should not see 'switch to tab' for other container tabs");
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions user-context-id:3",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               makeSwitchToTabMatch("http://foobar.org/", { title: "foobar.org - much better than ABC, definitely better than XYZ" }),
               { uri: uri1, title: "ABC rocks", style: [ "favicon" ] },
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] } ]
  });

  info("a different container tab should not see any 'switch to tab'");
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions user-context-id:2",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               { uri: uri1, title: "ABC rocks", style: [ "favicon" ] },
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] },
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("three results, both normal results are tab matches, one has multiple tabs");
  addOpenPages(uri2, 5);
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               makeSwitchToTabMatch("http://xyz.net/", { title: "xyz.net - we're better than ABC" }),
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("three results, no tab matches (disable-private-actions)");
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions disable-private-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               { uri: uri1, title: "ABC rocks", style: [ "favicon" ] },
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] },
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("two results (actions disabled)");
  await check_autocomplete({
    search: "abc",
    searchParam: "",
    matches: [ { uri: uri1, title: "ABC rocks", style: [ "favicon" ] },
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] },
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("three results, no tab matches");
  removeOpenPages(uri1, 1);
  removeOpenPages(uri2, 6);
  await check_autocomplete({
    search: "abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("abc", { heuristic: true }),
               { uri: uri1, title: "ABC rocks", style: [ "favicon" ] },
               { uri: uri2, title: "xyz.net - we're better than ABC", style: [ "favicon" ] },
               { uri: uri5, title: "foobar.org - much better than ABC, definitely better than XYZ", style: [ "favicon" ] } ]
  });

  info("tab match search with restriction character");
  addOpenPages(uri1, 1);
  await check_autocomplete({
    search: gTabRestrictChar + " abc",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch(gTabRestrictChar + " abc", { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }) ]
  });

  info("tab match with not-addable pages");
  await check_autocomplete({
    search: "mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("mozilla", { heuristic: true }),
               makeSwitchToTabMatch("about:mozilla") ]
  });

  info("tab match with not-addable pages and restriction character");
  await check_autocomplete({
    search: gTabRestrictChar + " mozilla",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch(gTabRestrictChar + " mozilla", { heuristic: true }),
               makeSwitchToTabMatch("about:mozilla") ]
  });

  info("tab match with not-addable pages and only restriction character");
  await check_autocomplete({
    search: gTabRestrictChar,
    searchParam: "enable-actions",
    matches: [ makeSearchMatch(gTabRestrictChar, { heuristic: true }),
               makeSwitchToTabMatch("http://abc.com/", { title: "ABC rocks" }),
               makeSwitchToTabMatch("about:mozilla"),
               makeSwitchToTabMatch("data:text/html,test") ]
  });

  await cleanup();
});
