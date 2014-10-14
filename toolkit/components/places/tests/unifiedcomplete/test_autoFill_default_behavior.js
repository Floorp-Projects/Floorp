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

  yield promiseAddVisits([
    { uri: uri1, title: "typed", transition: TRANSITION_TYPED },
    { uri: uri2, title: "visited" },
    { uri: uri4, title: "tpbk", transition: TRANSITION_TYPED },
  ]);
  addBookmark( { uri: uri3, title: "bookmarked" } );
  addBookmark( { uri: uri4, title: "tpbk" } );

  // RESTRICT TO HISTORY.
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 1);

  do_log_info("Restrict history, common visit, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ { uri: uri2, title: "visited" } ],
    autofilled: "vi",
    completed: "vi"
  });

  do_log_info("Restrict history, typed visit, should autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ { uri: uri1, title: "typed" } ],
    autofilled: "typed/",
    completed: "typed/"
  });

  // Don't autoFill this one cause it's not typed.
  do_log_info("Restrict history, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ ],
    autofilled: "bo",
    completed: "bo"
  });

  // Note we don't show this one cause it's not typed.
  do_log_info("Restrict history, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk" } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  // We are not restricting on typed, so we autoFill the bookmark even if we
  // are restricted to history.  We accept that cause not doing that
  // would be a perf hit and the privacy implications are very weak.
  do_log_info("Restrict history, bookmark, autoFill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked" } ],
    autofilled: "bookmarked/",
    completed: "bookmarked/"
  });

  do_log_info("Restrict history, common visit, autoFill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ { uri: uri2, title: "visited" } ],
    autofilled: "visited/",
    completed: "visited/"
  });

  // RESTRICT TO TYPED.
  // This should basically ignore autoFill.typed and acts as if it would be set.
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 32);

  // Typed behavior basically acts like history, but filters on typed.
  do_log_info("Restrict typed, common visit, autoFill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ ],
    autofilled: "vi",
    completed: "vi"
  });

  do_log_info("Restrict typed, typed visit, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ { uri: uri1, title: "typed" } ],
    autofilled: "typed/",
    completed: "typed/"
  });

  do_log_info("Restrict typed, bookmark, autofill.typed = false, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ ],
    autofilled: "bo",
    completed: "bo"
  });

  do_log_info("Restrict typed, typed bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk" } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  // RESTRICT BOOKMARKS.
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 2);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);

  do_log_info("Restrict bookmarks, common visit, should not autoFill");
  yield check_autocomplete({
    search: "vi",
    matches: [ ],
    autofilled: "vi",
    completed: "vi"
  });

  do_log_info("Restrict bookmarks, typed visit, should not autoFill");
  yield check_autocomplete({
    search: "ty",
    matches: [ ],
    autofilled: "ty",
    completed: "ty"
  });

  // Don't autoFill this one cause it's not typed.
  do_log_info("Restrict bookmarks, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked" } ],
    autofilled: "bo",
    completed: "bo"
  });

  // Note we don't show this one cause it's not typed.
  do_log_info("Restrict bookmarks, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tp",
    matches: [ { uri: uri4, title: "tpbk" } ],
    autofilled: "tpbk/",
    completed: "tpbk/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  do_log_info("Restrict bookmarks, bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bo",
    matches: [ { uri: uri3, title: "bookmarked" } ],
    autofilled: "bookmarked/",
    completed: "bookmarked/"
  });

  yield cleanup();
});

add_task(function* test_default_behavior_url() {
  let uri1 = NetUtil.newURI("http://typed/ty/");
  let uri2 = NetUtil.newURI("http://visited/vi/");
  let uri3 = NetUtil.newURI("http://bookmarked/bo/");
  let uri4 = NetUtil.newURI("http://tpbk/tp/");

  yield promiseAddVisits([
    { uri: uri1, title: "typed", transition: TRANSITION_TYPED },
    { uri: uri2, title: "visited" },
    { uri: uri4, title: "tpbk", transition: TRANSITION_TYPED },
  ]);
  addBookmark( { uri: uri3, title: "bookmarked" } );
  addBookmark( { uri: uri4, title: "tpbk" } );

  // RESTRICT TO HISTORY.
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 1);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", true);
  Services.prefs.setBoolPref("browser.urlbar.autoFill.searchEngines", false);

  do_log_info("URL: Restrict history, common visit, should not autoFill");
  yield check_autocomplete({
    search: "visited/v",
    matches: [ { uri: uri2, title: "visited" } ],
    autofilled: "visited/v",
    completed: "visited/v"
  });

  do_log_info("URL: Restrict history, typed visit, should autoFill");
  yield check_autocomplete({
    search: "typed/t",
    matches: [ { uri: uri1, title: "typed/ty/" } ],
    autofilled: "typed/ty/",
    completed: "http://typed/ty/"
  });

  // Don't autoFill this one cause it's not typed.
  do_log_info("URL: Restrict history, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ ],
    autofilled: "bookmarked/b",
    completed: "bookmarked/b"
  });

  // Note we don't show this one cause it's not typed.
  do_log_info("URL: Restrict history, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tpbk/t",
    matches: [ { uri: uri4, title: "tpbk/tp/" } ],
    autofilled: "tpbk/tp/",
    completed: "http://tpbk/tp/"
  });

  // RESTRICT BOOKMARKS.
  Services.prefs.setIntPref("browser.urlbar.default.behavior", 2);

  do_log_info("URL: Restrict bookmarks, common visit, should not autoFill");
  yield check_autocomplete({
    search: "visited/v",
    matches: [ ],
    autofilled: "visited/v",
    completed: "visited/v"
  });

  do_log_info("URL: Restrict bookmarks, typed visit, should not autoFill");
  yield check_autocomplete({
    search: "typed/t",
    matches: [ ],
    autofilled: "typed/t",
    completed: "typed/t"
  });

  // Don't autoFill this one cause it's not typed.
  do_log_info("URL: Restrict bookmarks, bookmark, should not autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ { uri: uri3, title: "bookmarked" } ],
    autofilled: "bookmarked/b",
    completed: "bookmarked/b"
  });

  // Note we don't show this one cause it's not typed.
  do_log_info("URL: Restrict bookmarks, typed bookmark, should autoFill");
  yield check_autocomplete({
    search: "tpbk/t",
    matches: [ { uri: uri4, title: "tpbk/tp/" } ],
    autofilled: "tpbk/tp/",
    completed: "http://tpbk/tp/"
  });

  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);

  do_log_info("URL: Restrict bookmarks, bookmark, autofill.typed = false, should autoFill");
  yield check_autocomplete({
    search: "bookmarked/b",
    matches: [ { uri: uri3, title: "bookmarked/bo/" } ],
    autofilled: "bookmarked/bo/",
    completed: "http://bookmarked/bo/"
  });

  yield cleanup();
});
