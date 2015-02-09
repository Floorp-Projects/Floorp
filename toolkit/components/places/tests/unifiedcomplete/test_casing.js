/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_casing_1() {
  do_print("Searching for cased entry 1");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "MOZ",
    autofilled: "MOZilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_casing_2() {
  do_print("Searching for cased entry 2");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/T",
    completed: "mozilla.org/T"
  });
  yield cleanup();
});

add_task(function* test_casing_3() {
  do_print("Searching for cased entry 3");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  yield cleanup();
});

add_task(function* test_casing_4() {
  do_print("Searching for cased entry 4");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "mOzilla.org/t",
    autofilled: "mOzilla.org/t",
    completed: "mOzilla.org/t"
  });
  yield cleanup();
});

add_task(function* test_casing_5() {
  do_print("Searching for cased entry 5");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "mOzilla.org/T",
    autofilled: "mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_casing() {
  do_print("Searching for untrimmed cased entry");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://mOz",
    autofilled: "http://mOzilla.org/",
    completed: "http://mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_www_casing() {
  do_print("Searching for untrimmed cased entry with www");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://www.mOz",
    autofilled: "http://www.mOzilla.org/",
    completed: "http://www.mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_path_casing() {
  do_print("Searching for untrimmed cased entry with path");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://mOzilla.org/t",
    autofilled: "http://mOzilla.org/t",
    completed: "http://mOzilla.org/t"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_path_casing_2() {
  do_print("Searching for untrimmed cased entry with path 2");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://mOzilla.org/T",
    autofilled: "http://mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_path_www_casing() {
  do_print("Searching for untrimmed cased entry with www and path");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://www.mOzilla.org/t",
    autofilled: "http://www.mOzilla.org/t",
    completed: "http://www.mOzilla.org/t"
  });
  yield cleanup();
});

add_task(function* test_untrimmed_path_www_casing_2() {
  do_print("Searching for untrimmed cased entry with www and path 2");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  yield check_autocomplete({
    search: "http://www.mOzilla.org/T",
    autofilled: "http://www.mOzilla.org/Test/",
    completed: "http://www.mozilla.org/Test/"
  });
  yield cleanup();
});
