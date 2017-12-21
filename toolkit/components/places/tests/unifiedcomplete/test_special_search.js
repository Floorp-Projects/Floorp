/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 395161 that allows special searches that restrict results to
 * history/bookmark/tagged items and title/url matches.
 *
 * Test 485122 by making sure results don't have tags when restricting result
 * to just history either by default behavior or dynamic query restrict.
 */

function setSuggestPrefsToFalse() {
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
}

add_task(async function test_special_searches() {
  let uri1 = NetUtil.newURI("http://url/");
  let uri2 = NetUtil.newURI("http://url/2");
  let uri3 = NetUtil.newURI("http://foo.bar/");
  let uri4 = NetUtil.newURI("http://foo.bar/2");
  let uri5 = NetUtil.newURI("http://url/star");
  let uri6 = NetUtil.newURI("http://url/star/2");
  let uri7 = NetUtil.newURI("http://foo.bar/star");
  let uri8 = NetUtil.newURI("http://foo.bar/star/2");
  let uri9 = NetUtil.newURI("http://url/tag");
  let uri10 = NetUtil.newURI("http://url/tag/2");
  let uri11 = NetUtil.newURI("http://foo.bar/tag");
  let uri12 = NetUtil.newURI("http://foo.bar/tag/2");
  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "title", transition: TRANSITION_TYPED },
    { uri: uri2, title: "foo.bar" },
    { uri: uri3, title: "title" },
    { uri: uri4, title: "foo.bar", transition: TRANSITION_TYPED },
    { uri: uri6, title: "foo.bar" },
    { uri: uri11, title: "title", transition: TRANSITION_TYPED }
  ]);
  await addBookmark( { uri: uri5, title: "title" } );
  await addBookmark( { uri: uri6, title: "foo.bar" } );
  await addBookmark( { uri: uri7, title: "title" } );
  await addBookmark( { uri: uri8, title: "foo.bar" } );
  await addBookmark( { uri: uri9, title: "title", tags: [ "foo.bar" ] } );
  await addBookmark( { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] } );
  await addBookmark( { uri: uri11, title: "title", tags: [ "foo.bar" ] } );
  await addBookmark( { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } );

  // Test restricting searches
  info("History restrict");
  await check_autocomplete({
    search: "^",
    matches: [ { uri: uri1, title: "title" },
               { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("Star restrict");
  await check_autocomplete({
    search: "*",
    matches: [ { uri: uri5, title: "title", style: [ "bookmark" ] },
               { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar"], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("Tag restrict");
  await check_autocomplete({
    search: "+",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test specials as any word position
  info("Special as first word");
  await check_autocomplete({
    search: "^ foo bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("Special as middle word");
  await check_autocomplete({
    search: "foo ^ bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("Special as last word");
  await check_autocomplete({
    search: "foo bar ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test restricting and matching searches with a term
  info("foo ^ -> history");
  await check_autocomplete({
    search: "foo ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo * -> is star");
  await check_autocomplete({
    search: "foo *",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo # -> in title");
  await check_autocomplete({
    search: "foo #",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo @ -> in url");
  await check_autocomplete({
    search: "foo @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo + -> is tag");
  await check_autocomplete({
    search: "foo +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo ~ -> is typed");
  await check_autocomplete({
    search: "foo ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test various pairs of special searches
  info("foo ^ * -> history, is star");
  await check_autocomplete({
    search: "foo ^ *",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo ^ # -> history, in title");
  await check_autocomplete({
    search: "foo ^ #",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo ^ @ -> history, in url");
  await check_autocomplete({
    search: "foo ^ @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo ^ + -> history, is tag");
  await check_autocomplete({
    search: "foo ^ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo ^ ~ -> history, is typed");
  await check_autocomplete({
    search: "foo ^ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo * # -> is star, in title");
  await check_autocomplete({
    search: "foo * #",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo * @ -> is star, in url");
  await check_autocomplete({
    search: "foo * @",
    matches: [ { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo * + -> same as +");
  await check_autocomplete({
    search: "foo * +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo * ~ -> is star, is typed");
  await check_autocomplete({
    search: "foo * ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo # @ -> in title, in url");
  await check_autocomplete({
    search: "foo # @",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo # + -> in title, is tag");
  await check_autocomplete({
    search: "foo # +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo # ~ -> in title, is typed");
  await check_autocomplete({
    search: "foo # ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo @ + -> in url, is tag");
  await check_autocomplete({
    search: "foo @ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo @ ~ -> in url, is typed");
  await check_autocomplete({
    search: "foo @ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  info("foo + ~ -> is tag, is typed");
  await check_autocomplete({
    search: "foo + ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Disable autoFill for the next tests, see test_autoFill_default_behavior.js
  // for specific tests.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  // Test default usage by setting certain browser.urlbar.suggest.* prefs
  info("foo -> default history");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  await check_autocomplete({
    search: "foo",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: ["foo.bar"], style: [ "tag" ] } ]
  });

  info("foo -> default history, is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar"], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo -> default history, is star, is typed");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo -> is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  info("foo -> is star, is typed");
  setSuggestPrefsToFalse();
  // only typed should be ignored
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  await check_autocomplete({
    search: "foo",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] }  ]
  });

  await cleanup();
});
