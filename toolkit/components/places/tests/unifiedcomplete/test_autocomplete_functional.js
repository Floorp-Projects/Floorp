/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

add_task(function* test_disabling_autocomplete() {
  do_log_info("Check disabling autocomplete disables autofill");
  Services.prefs.setBoolPref("browser.urlbar.autocomplete.enabled", false);
  yield promiseAddVisits({ uri: NetUtil.newURI("http://visit.mozilla.org"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "vis",
    autofilled: "vis",
    completed: "vis"
  });
  yield cleanup();
});

add_task(function* test_urls_order() {
  do_log_info("Add urls, check for correct order");
  let places = [{ uri: NetUtil.newURI("http://visit1.mozilla.org") },
                { uri: NetUtil.newURI("http://visit2.mozilla.org"),
                  transition: TRANSITION_TYPED }];
  yield promiseAddVisits(places);
  yield check_autocomplete({
    search: "vis",
    autofilled: "visit2.mozilla.org/",
    completed: "visit2.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_ignore_prefix() {
  do_log_info("Add urls, make sure www and http are ignored");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://www.visit1.mozilla.org"));
  yield check_autocomplete({
    search: "visit1",
    autofilled: "visit1.mozilla.org/",
    completed: "visit1.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_after_host() {
  do_log_info("Autocompleting after an existing host completes to the url");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://www.visit3.mozilla.org"));
  yield check_autocomplete({
    search: "visit3.mozilla.org/",
    autofilled: "visit3.mozilla.org/",
    completed: "visit3.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_respect_www() {
  do_log_info("Searching for www.me should yield www.me.mozilla.org/");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://www.me.mozilla.org"));
  yield check_autocomplete({
    search: "www.me",
    autofilled: "www.me.mozilla.org/",
    completed: "www.me.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_bookmark_first() {
  do_log_info("With a bookmark and history, the query result should be the bookmark");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  addBookmark({ uri: NetUtil.newURI("http://bookmark1.mozilla.org/") });
  yield promiseAddVisits(NetUtil.newURI("http://bookmark1.mozilla.org/foo"));
  yield check_autocomplete({
    search: "bookmark",
    autofilled: "bookmark1.mozilla.org/",
    completed: "bookmark1.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_full_path() {
  do_log_info("Check to make sure we get the proper results with full paths");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  let places = [{ uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious") },
                { uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey") }];
  yield promiseAddVisits(places);
  yield check_autocomplete({
    search: "smokey",
    autofilled: "smokey.mozilla.org/",
    completed: "smokey.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_complete_to_slash() {
  do_log_info("Check to make sure we autocomplete to the following '/'");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  let places = [{ uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=delicious") },
                { uri: NetUtil.newURI("http://smokey.mozilla.org/foo/bar/baz?bacon=smokey") }];
  yield promiseAddVisits(places);
  yield check_autocomplete({
    search: "smokey.mozilla.org/fo",
    autofilled: "smokey.mozilla.org/foo/",
    completed: "http://smokey.mozilla.org/foo/",
  });
  yield cleanup();
});

add_task(function* test_complete_to_slash_with_www() {
  do_log_info("Check to make sure we autocomplete to the following '/'");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  let places = [{ uri: NetUtil.newURI("http://www.smokey.mozilla.org/foo/bar/baz?bacon=delicious") },
                { uri: NetUtil.newURI("http://www.smokey.mozilla.org/foo/bar/baz?bacon=smokey") }];
  yield promiseAddVisits(places);
  yield check_autocomplete({
    search: "smokey.mozilla.org/fo",
    autofilled: "smokey.mozilla.org/foo/",
    completed: "http://www.smokey.mozilla.org/foo/",
  });
  yield cleanup();
});

add_task(function* test_complete_querystring() {
  do_log_info("Check to make sure we autocomplete after ?");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious"));
  yield check_autocomplete({
    search: "smokey.mozilla.org/foo?",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious",
  });
  yield cleanup();
});

add_task(function* test_complete_fragment() {
  do_log_info("Check to make sure we autocomplete after #");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield promiseAddVisits(NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar"));
  yield check_autocomplete({
    search: "smokey.mozilla.org/foo?bacon=delicious#bar",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious#bar",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious#bar",
  });
  yield cleanup();
});
