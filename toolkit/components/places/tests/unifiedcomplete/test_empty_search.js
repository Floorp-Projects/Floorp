/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test for bug 426864 that makes sure the empty search (drop down list) only
 * shows typed pages from history.
 */

add_task(function* test_javascript_match() {
  let uri1 = NetUtil.newURI("http://t.foo/0");
  let uri2 = NetUtil.newURI("http://t.foo/1");
  let uri3 = NetUtil.newURI("http://t.foo/2");
  let uri4 = NetUtil.newURI("http://t.foo/3");
  let uri5 = NetUtil.newURI("http://t.foo/4");
  let uri6 = NetUtil.newURI("http://t.foo/5");

  yield promiseAddVisits([ { uri: uri1, title: "title" },
                           { uri: uri2, title: "title" },
                           { uri: uri3, title: "title",
                             transition: TRANSITION_TYPED},
                           { uri: uri4, title: "title",
                             transition: TRANSITION_TYPED },
                           { uri: uri6, title: "title",
                             transition: TRANSITION_TYPED } ]);

  addBookmark({ uri: uri2,
                title: "title" });
  addBookmark({ uri: uri4,
                title: "title" });
  addBookmark({ uri: uri5,
                title: "title" });
  addBookmark({ uri: uri6,
                title: "title" });

  // Now remove page 6 from history, so it is an unvisited, typed bookmark.
  PlacesUtils.history.removePage(uri6);

  do_log_info("Match everything");
  yield check_autocomplete({
    search: "foo",
    matches: [ { uri: uri1, title: "title" },
               { uri: uri2, title: "title" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "title" },
               { uri: uri5, title: "title" },
               { uri: uri6, title: "title" } ]
  });

  do_log_info("Match only typed history");
  yield check_autocomplete({
    search: "foo ^ ~",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" } ]
  });

  do_log_info("Drop-down empty search matches only typed history");
  yield check_autocomplete({
    search: "",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" } ]
  });

  do_log_info("Drop-down empty search matches everything");
  Services.prefs.setIntPref("browser.urlbar.default.behavior.emptyRestriction", 0);
  yield check_autocomplete({
    search: "",
    matches: [ { uri: uri1, title: "title" },
               { uri: uri2, title: "title" },
               { uri: uri3, title: "title" },
               { uri: uri4, title: "title" },
               { uri: uri5, title: "title" },
               { uri: uri6, title: "title" } ]
  });

  do_log_info("Drop-down empty search matches only typed");
  Services.prefs.setIntPref("browser.urlbar.default.behavior.emptyRestriction", 32);
  yield check_autocomplete({
    search: "",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" },
               { uri: uri6, title: "title" } ]
  });

  do_log_info("Drop-down empty search matches only typed history");
  Services.prefs.clearUserPref("browser.urlbar.default.behavior.emptyRestriction");
  yield check_autocomplete({
    search: "",
    matches: [ { uri: uri3, title: "title" },
               { uri: uri4, title: "title" } ]
  });

  yield cleanup();
});
