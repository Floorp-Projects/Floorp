/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test autoFill for different default behaviors.
 */

add_task(function* test_default_behavior_host() {
  let uri1 = NetUtil.newURI("http://typed/");
  let uri2 = NetUtil.newURI("http://visited/");
  let uri3 = NetUtil.newURI("http://bookmarked/");
  let uri4 = NetUtil.newURI("http://tpbk/");
  let uri5 = NetUtil.newURI("http://tagged/");
  let uri6 = NetUtil.newURI("https://secure/");

  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "typed", transition: TRANSITION_TYPED },
    { uri: uri2, title: "visited" },
    { uri: uri4, title: "tpbk", transition: TRANSITION_TYPED },
    { uri: uri6, title: "secure", transition: TRANSITION_TYPED },
  ]);
  yield addBookmark( { uri: uri3, title: "bookmarked" } );
  yield addBookmark( { uri: uri4, title: "tpbk" } );
  yield addBookmark( { uri: uri5, title: "title", tags: ["foo"] } );

  yield setFaviconForPage(uri1, "chrome://global/skin/icons/info.svg");
  yield setFaviconForPage(uri3, "chrome://global/skin/icons/error-16.png");
  yield setFaviconForPage(uri6, "chrome://global/skin/icons/question-16.png");

  // RESTRICT TO HISTORY.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);

  do_print("Restrict history, common visit, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ { uri: uri2, title: "visited" } ],
    autofilled: "vi",
    completed: "vi"
  });

  do_print("Restrict history, typed visit, should autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ { uri: uri1, title: "typed", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/info.svg" } ],
    autofilled: "typed/",
    completed: "typed/"
  });

  do_print("Restrict history, secure typed visit, should autoFill with https");
  yield check_autocomplete({
    search: "secure",
    matches: [ { uri: uri6, title: "https://secure", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/question-16.png" } ],
    autofilled: "secure/",
    completed: "https://secure/"
  });

  // Don't autoFill this one cause it's not typed.
  do_print("Restrict history, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ ],
    autofilled: "bo",
    completed: "bo"
  });

  // Note we don't show this one cause it's not typed.
  do_print("Restrict history, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk", style: [ "autofill", "heuristic" ] } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  // We are not restricting on typed, so we autoFill the bookmark even if we
  // are restricted to history.  We accept that cause not doing that
  // would be a perf hit and the privacy implications are very weak.
  do_print("Restrict history, bookmark, autoFill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/error-16.png" } ],
    autofilled: "bookmarked/",
    completed: "bookmarked/"
  });

  do_print("Restrict history, common visit, autoFill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ { uri: uri2, title: "visited", style: [ "autofill", "heuristic" ] } ],
    autofilled: "visited/",
    completed: "visited/"
  });

  // RESTRICT TO TYPED.
  // This should basically ignore autoFill.typed and acts as if it would be set.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", true);

  // Typed behavior basically acts like history, but filters on typed.
  do_print("Restrict typed, common visit, autoFill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ ],
    autofilled: "vi",
    completed: "vi"
  });

  do_print("Restrict typed, typed visit, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ { uri: uri1, title: "typed", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/info.svg"} ],
    autofilled: "typed/",
    completed: "typed/"
  });

  do_print("Restrict typed, bookmark, autofill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ ],
    autofilled: "bo",
    completed: "bo"
  });

  do_print("Restrict typed, typed bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk", style: [ "autofill", "heuristic" ] } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  // RESTRICT BOOKMARKS.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);

  do_print("Restrict bookmarks, common visit, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ ],
    autofilled: "vi",
    completed: "vi"
  });

  do_print("Restrict bookmarks, typed visit, should not autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ ],
    autofilled: "ty",
    completed: "ty"
  });

  // Don't autoFill this one cause it's not typed.
  do_print("Restrict bookmarks, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked", style: [ "bookmark" ],
                 icon: "chrome://global/skin/icons/error-16.png"} ],
    autofilled: "bo",
    completed: "bo"
  });

  // Note we don't show this one cause it's not typed.
  do_print("Restrict bookmarks, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk", style: [ "autofill", "heuristic" ] } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  do_print("Restrict bookmarks, bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/error-16.png" } ],
    autofilled: "bookmarked/",
    completed: "bookmarked/"
  });

  // Don't autofill because it's a title.
  do_print("Restrict bookmarks, title, autofill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "# ta",
    matches: [ ],
    autofilled: "# ta",
    completed: "# ta"
  });

  // Don't autofill because it's a tag.
  do_print("Restrict bookmarks, tag, autofill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "+ ta",
    matches: [ { uri: uri5, title: "title", tags: [ "foo" ], style: [ "tag" ] } ],
    autofilled: "+ ta",
    completed: "+ ta"
  });

  yield cleanup();
});

add_task(function* test_default_behavior_url() {
  let uri1 = NetUtil.newURI("http://typed/ty/");
  let uri2 = NetUtil.newURI("http://visited/vi/");
  let uri3 = NetUtil.newURI("http://bookmarked/bo/");
  let uri4 = NetUtil.newURI("http://tpbk/tp/");

  yield PlacesTestUtils.addVisits([
    { uri: uri1, title: "typed", transition: TRANSITION_TYPED },
    { uri: uri2, title: "visited" },
    { uri: uri4, title: "tpbk", transition: TRANSITION_TYPED },
  ]);
  yield addBookmark( { uri: uri3, title: "bookmarked" } );
  yield addBookmark( { uri: uri4, title: "tpbk" } );

  yield setFaviconForPage(uri1, "chrome://global/skin/icons/info.svg");
  yield setFaviconForPage(uri3, "chrome://global/skin/icons/error-16.png");

  // RESTRICT TO HISTORY.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.history.onlyTyped", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", false);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);

  do_print("URL: Restrict history, common visit, should not autoFill");
  yield check_autocomplete({
    search: "visited/v",
    matches: [ { uri: uri2, title: "visited" } ],
    autofilled: "visited/v",
    completed: "visited/v"
  });

  do_print("URL: Restrict history, typed visit, should autoFill");
  yield check_autocomplete({
    search: "typed/t",
    matches: [ { uri: uri1, title: "typed/ty/", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/info.svg"} ],
    autofilled: "typed/ty/",
    completed: "http://typed/ty/"
  });

  // Don't autoFill this one cause it's not typed.
  do_print("URL: Restrict history, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ ],
    autofilled: "bookmarked/b",
    completed: "bookmarked/b"
  });

  // Note we don't show this one cause it's not typed.
  do_print("URL: Restrict history, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tpbk/t",
    matches: [ { uri: uri4, title: "tpbk/tp/", style: [ "autofill", "heuristic" ] } ],
    autofilled: "tpbk/tp/",
    completed: "http://tpbk/tp/"
  });

  // RESTRICT BOOKMARKS.
  Services.prefs.setBoolPref("browser.urlbar.suggest.history", false);
  Services.prefs.setBoolPref("browser.urlbar.suggest.bookmark", true);

  do_print("URL: Restrict bookmarks, common visit, should not autoFill");
  yield check_autocomplete({
    search: "visited/v",
    matches: [ ],
    autofilled: "visited/v",
    completed: "visited/v"
  });

  do_print("URL: Restrict bookmarks, typed visit, should not autoFill");
  yield check_autocomplete({
    search: "typed/t",
    matches: [ ],
    autofilled: "typed/t",
    completed: "typed/t"
  });

  // Don't autoFill this one cause it's not typed.
  do_print("URL: Restrict bookmarks, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ { uri: uri3, title: "bookmarked", style: [ "bookmark" ],
                 icon: "chrome://global/skin/icons/error-16.png" } ],
    autofilled: "bookmarked/b",
    completed: "bookmarked/b"
  });

  // Note we don't show this one cause it's not typed.
  do_print("URL: Restrict bookmarks, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tpbk/t",
    matches: [ { uri: uri4, title: "tpbk/tp/", style: [ "autofill", "heuristic" ] } ],
    autofilled: "tpbk/tp/",
    completed: "http://tpbk/tp/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  do_print("URL: Restrict bookmarks, bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ { uri: uri3, title: "bookmarked/bo/", style: [ "autofill", "heuristic" ],
                 icon: "chrome://global/skin/icons/error-16.png" } ],
    autofilled: "bookmarked/bo/",
    completed: "http://bookmarked/bo/"
  });

  yield cleanup();
});
