/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 426864 that makes sure the empty search (drop down list) only
 * shows typed pages from history.
 */

add_task(async function test_javascript_match() {
  let uri1 = NetUtil.newURI("http://t.foo/0");
  let uri2 = NetUtil.newURI("http://t.foo/1");
  let uri3 = NetUtil.newURI("http://t.foo/2");
  let uri4 = NetUtil.newURI("http://t.foo/3");
  let uri5 = NetUtil.newURI("http://t.foo/4");
  let uri6 = NetUtil.newURI("http://t.foo/5");
  let uri7 = NetUtil.newURI("http://t.foo/6");

  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title" },
    { uri: uri2, title: "title" },
    { uri: uri3, title: "title", transition: TRANSITION_TYPED},
    { uri: uri4, title: "title", transition: TRANSITION_TYPED },
    { uri: uri6, title: "title", transition: TRANSITION_TYPED },
    { uri: uri7, title: "title" }
  ]);

  await addBookmark({ uri: uri2,
                      title: "title" });
  await addBookmark({ uri: uri4,
                      title: "title" });
  await addBookmark({ uri: uri5,
                      title: "title" });
  await addBookmark({ uri: uri6,
                      title: "title" });

  addOpenPages(uri7, 1);

  // Now remove page 6 from history, so it is an unvisited bookmark.
  await PlacesUtils.history.remove(uri6);

  info("Match everything");
  await check_autocomplete({
    search: "foo",
    searchParam: "enable-actions",
    matches: [ makeSearchMatch("foo", { heuristic: true }),
               { uri: uri1, title: "title" },
               { uri: uri2, title: "title", style: ["bookmark"] },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "title", style: ["bookmark"] },
               { uri: uri5, title: "title", style: ["bookmark"] },
               { uri: uri6, title: "title", style: ["bookmark"] },
               makeSwitchToTabMatch("http://t.foo/6", { title: "title" }),
             ]
  });

  // Note the next few tests do *not* get a search result as enable-actions
  // isn't specified.
  info("Match only typed history");
  await check_autocomplete({
    search: "foo ^ ~",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" } ]
  });

  info("Drop-down empty search matches only typed history");
  await check_autocomplete({
    search: "",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" } ]
  });

  info("Drop-down empty search matches only bookmarks");
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "",
    matches: [ { uri: uri2, title: "title", style: ["bookmark"] },
               { uri: uri4, title: "title", style: ["bookmark"] },
               { uri: uri5, title: "title", style: ["bookmark"] },
               { uri: uri6, title: "title", style: ["bookmark"] } ]
  });

  info("Drop-down empty search matches only open tabs");
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  await check_autocomplete({
    search: "",
    searchParam: "enable-actions",
    matches: [
               makeSwitchToTabMatch("http://t.foo/6", { title: "title" }),
             ]
  });

  Services.prefs.clearUserPref("browser.urlbar.suggest.history");
  Services.prefs.clearUserPref("browser.urlbar.suggest.bookmark");

  await cleanup();
});
