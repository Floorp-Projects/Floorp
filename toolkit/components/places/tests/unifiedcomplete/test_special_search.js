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

add_task(function* test_special_searches() {
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
  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "title", transition: TRANSITION_TYPED },
    { uri: uri2, title: "foo.bar" },
    { uri: uri3, title: "title" },
    { uri: uri4, title: "foo.bar", transition: TRANSITION_TYPED },
    { uri: uri6, title: "foo.bar" },
    { uri: uri11, title: "title", transition: TRANSITION_TYPED }
  ]);
  yield addBookmark( { uri: uri5, title: "title" } );
  yield addBookmark( { uri: uri6, title: "foo.bar" } );
  yield addBookmark( { uri: uri7, title: "title" } );
  yield addBookmark( { uri: uri8, title: "foo.bar" } );
  yield addBookmark( { uri: uri9, title: "title", tags: [ "foo.bar" ] } );
  yield addBookmark( { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] } );
  yield addBookmark( { uri: uri11, title: "title", tags: [ "foo.bar" ] } );
  yield addBookmark( { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } );

  // Test restricting searches
  do_print("History restrict");
  yield check_autocomplete({
    search: "^",
    matches: [ { uri: uri1, title: "title" },
               { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("Star restrict");
  yield check_autocomplete({
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

  do_print("Tag restrict");
  yield check_autocomplete({
    search: "+",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test specials as any word position
  do_print("Special as first word");
  yield check_autocomplete({
    search: "^ foo bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("Special as middle word");
  yield check_autocomplete({
    search: "foo ^ bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("Special as last word");
  yield check_autocomplete({
    search: "foo bar ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test restricting and matching searches with a term
  do_print("foo ^ -> history");
  yield check_autocomplete({
    search: "foo ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo | -> history (change pref)");
  changeRestrict("history", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo * -> is star");
  resetRestrict("history");
  yield check_autocomplete({
    search: "foo *",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo | -> is star (change pref)");
  changeRestrict("bookmark", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo # -> in title");
  resetRestrict("bookmark");
  yield check_autocomplete({
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

  do_print("foo | -> in title (change pref)");
  changeRestrict("title", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo @ -> in url");
  resetRestrict("title");
  yield check_autocomplete({
    search: "foo @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo | -> in url (change pref)");
  changeRestrict("url", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo + -> is tag");
  resetRestrict("url");
  yield check_autocomplete({
    search: "foo +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo | -> is tag (change pref)");
  changeRestrict("tag", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo ~ -> is typed");
  resetRestrict("tag");
  yield check_autocomplete({
    search: "foo ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo | -> is typed (change pref)");
  changeRestrict("typed", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Test various pairs of special searches
  do_print("foo ^ * -> history, is star");
  resetRestrict("typed");
  yield check_autocomplete({
    search: "foo ^ *",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo ^ # -> history, in title");
  yield check_autocomplete({
    search: "foo ^ #",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo ^ @ -> history, in url");
  yield check_autocomplete({
    search: "foo ^ @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo ^ + -> history, is tag");
  yield check_autocomplete({
    search: "foo ^ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo ^ ~ -> history, is typed");
  yield check_autocomplete({
    search: "foo ^ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo * # -> is star, in title");
  yield check_autocomplete({
    search: "foo * #",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo * @ -> is star, in url");
  yield check_autocomplete({
    search: "foo * @",
    matches: [ { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo * + -> same as +");
  yield check_autocomplete({
    search: "foo * +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo * ~ -> is star, is typed");
  yield check_autocomplete({
    search: "foo * ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo # @ -> in title, in url");
  yield check_autocomplete({
    search: "foo # @",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo # + -> in title, is tag");
  yield check_autocomplete({
    search: "foo # +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo # ~ -> in title, is typed");
  yield check_autocomplete({
    search: "foo # ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo @ + -> in url, is tag");
  yield check_autocomplete({
    search: "foo @ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo @ ~ -> in url, is typed");
  yield check_autocomplete({
    search: "foo @ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  do_print("foo + ~ -> is tag, is typed");
  yield check_autocomplete({
    search: "foo + ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "tag" ] } ]
  });

  // Disable autoFill for the next tests, see test_autoFill_default_behavior.js
  // for specific tests.
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  // Test default usage by setting certain browser.urlbar.suggest.* prefs
  do_print("foo -> default history");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: ["foo.bar"], style: [ "tag" ] } ]
  });

  do_print("foo -> default history, is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  yield check_autocomplete({
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

  do_print("foo -> default history, is star, is typed");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo -> is star");
  setSuggestPrefsToFalse();
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] } ]
  });

  do_print("foo -> is star, is typed");
  setSuggestPrefsToFalse();
  // only typed should be ignored
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri6, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri7, title: "title", style: [ "bookmark" ] },
               { uri: uri8, title: "foo.bar", style: [ "bookmark" ] },
               { uri: uri9, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ], style: [ "bookmark-tag" ] }  ]
  });

  yield cleanup();
});
