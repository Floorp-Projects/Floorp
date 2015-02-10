/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// First do searches with typed behavior forced to false, so later tests will
// ensure autocomplete is able to dinamically switch behavior.

add_task(function* test_domain() {
  do_print("Searching for domain should autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  yield check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed:  "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_url() {
  do_print("Searching for url should autoFill it");
  Services.prefs.setBoolPref("browser.urlbar.autoFill.typed", false);
  yield PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  yield check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/link/",
    completed: "http://mozilla.org/link/"
  });
  yield cleanup();
});

// Now do searches with typed behavior forced to true.

add_task(function* test_untyped_domain() {
  do_print("Searching for non-typed domain should not autoFill it");
  yield PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  yield check_autocomplete({
    search: "moz",
    autofilled: "moz",
    completed: "moz"
  });
  yield cleanup();
});

add_task(function* test_typed_domain() {
  do_print("Searching for typed domain should autoFill it");
  yield PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/typed/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untyped_url() {
  do_print("Searching for non-typed url should not autoFill it");
  yield PlacesTestUtils.addVisits(NetUtil.newURI("http://mozilla.org/link/"));
  yield check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/li",
    completed: "mozilla.org/li"
  });
  yield cleanup();
});

add_task(function* test_typed_url() {
  do_print("Searching for typed url should autoFill it");
  yield PlacesTestUtils.addVisits({ uri: NetUtil.newURI("http://mozilla.org/link/"),
                           transition: TRANSITION_TYPED });
  yield check_autocomplete({
    search: "mozilla.org/li",
    autofilled: "mozilla.org/link/",
    completed: "http://mozilla.org/link/"
  });
  yield cleanup();
});
