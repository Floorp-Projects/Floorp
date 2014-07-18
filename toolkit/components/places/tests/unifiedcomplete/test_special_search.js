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
  yield promiseAddVisits([ { uri: uri1, title: "title", transition: TRANSITION_TYPED },
                           { uri: uri2, title: "foo.bar" },
                           { uri: uri3, title: "title" },
                           { uri: uri4, title: "foo.bar", transition: TRANSITION_TYPED },
                           { uri: uri6, title: "foo.bar" },
                           { uri: uri11, title: "title", transition: TRANSITION_TYPED } ]);
  addBookmark( { uri: uri5, title: "title" } );
  addBookmark( { uri: uri6, title: "foo.bar" } );
  addBookmark( { uri: uri7, title: "title" } );
  addBookmark( { uri: uri8, title: "foo.bar" } );
  addBookmark( { uri: uri9, title: "title", tags: [ "foo.bar" ] } );
  addBookmark( { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] } );
  addBookmark( { uri: uri11, title: "title", tags: [ "foo.bar" ] } );
  addBookmark( { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } );

  // Test restricting searches
  do_log_info("History restrict");
  yield check_autocomplete({
    search: "^",
    matches: [ { uri: uri1, title: "title" },
               { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("Star restrict");
  yield check_autocomplete({
    search: "*",
    matches: [ { uri: uri5, title: "title" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar"] },
               { uri: uri12, title: "foo.bar", tags: ["foo.bar"] } ]
  });

  do_log_info("Tag restrict");
  yield check_autocomplete({
    search: "+",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  // Test specials as any word position
  do_log_info("Special as first word");
  yield check_autocomplete({
    search: "^ foo bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("Special as middle word");
  yield check_autocomplete({
    search: "foo ^ bar",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("Special as last word");
  yield check_autocomplete({
    search: "foo bar ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  // Test restricting and matching searches with a term
  do_log_info("foo ^ -> history");
  yield check_autocomplete({
    search: "foo ^",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo | -> history (change pref)");
  changeRestrict("history", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo * -> is star");
  resetRestrict("history");
  yield check_autocomplete({
    search: "foo *",
    matches: [ { uri: uri6, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo | -> is star (change pref)");
  changeRestrict("bookmark", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri6, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo # -> in title");
  resetRestrict("bookmark");
  yield check_autocomplete({
    search: "foo #",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo | -> in title (change pref)");
  changeRestrict("title", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo @ -> in url");
  resetRestrict("title");
  yield check_autocomplete({
    search: "foo @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo | -> in url (change pref)");
  changeRestrict("url", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo + -> is tag");
  resetRestrict("url");
  yield check_autocomplete({
    search: "foo +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo | -> is tag (change pref)");
  changeRestrict("tag", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo ~ -> is typed");
  resetRestrict("tag");
  yield check_autocomplete({
    search: "foo ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo | -> is typed (change pref)");
  changeRestrict("typed", "|");
  yield check_autocomplete({
    search: "foo |",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  // Test various pairs of special searches
  do_log_info("foo ^ * -> history, is star");
  resetRestrict("typed");
  yield check_autocomplete({
    search: "foo ^ *",
    matches: [ { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo ^ # -> history, in title");
  yield check_autocomplete({
    search: "foo ^ #",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo ^ @ -> history, in url");
  yield check_autocomplete({
    search: "foo ^ @",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo ^ + -> history, is tag");
  yield check_autocomplete({
    search: "foo ^ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo ^ ~ -> history, is typed");
  yield check_autocomplete({
    search: "foo ^ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo * # -> is star, in title");
  yield check_autocomplete({
    search: "foo * #",
    matches: [ { uri: uri6, title: "foo.bar" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo * @ -> is star, in url");
  yield check_autocomplete({
    search: "foo * @",
    matches: [ { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo * + -> same as +");
  yield check_autocomplete({
    search: "foo * +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo * ~ -> is star, is typed");
  yield check_autocomplete({
    search: "foo * ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo # @ -> in title, in url");
  yield check_autocomplete({
    search: "foo # @",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo # + -> in title, is tag");
  yield check_autocomplete({
    search: "foo # +",
    matches: [ { uri: uri9, title: "title", tags: [ "foo.bar" ] },
               { uri: uri10, title: "foo.bar", tags: [ "foo.bar" ] },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo # ~ -> in title, is typed");
  yield check_autocomplete({
    search: "foo # ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo @ + -> in url, is tag");
  yield check_autocomplete({
    search: "foo @ +",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo @ ~ -> in url, is typed");
  yield check_autocomplete({
    search: "foo @ ~",
    matches: [ { uri: uri4, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo + ~ -> is tag, is typed");
  yield check_autocomplete({
    search: "foo + ~",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  // Test default usage by setting certain bits of default.behavior to 1
  do_log_info("foo -> default history");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 1);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri2, title: "foo.bar" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title" } ]
  });

  do_log_info("foo -> default history, is star");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 3);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri6, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo -> default history, is star, is typed");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 35);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo -> default history, is star, in url");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 19);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri11, title: "title", tags: [ "foo.bar" ] } ]
  });

  // Change the default to be less restrictive to make sure we find more
  do_log_info("foo -> default is star, in url");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 18);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  do_log_info("foo -> default in url");
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 16);
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "foo.bar" },
               { uri: uri7, title: "title" },
               { uri: uri8, title: "foo.bar" },
               { uri: uri11, title: "title", tags: [ "foo.bar" ] },
               { uri: uri12, title: "foo.bar", tags: [ "foo.bar" ] } ]
  });

  yield cleanup();
});
