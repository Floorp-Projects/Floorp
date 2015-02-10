/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* test_non_keyword() {
  do_print("Searching for non-keyworded entry should autoFill it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/") });
  yield check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_keyword() {
  do_print("Searching for keyworded entry should not autoFill it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  yield check_autocomplete({
    search: "moz",
    autofilled: "moz",
    completed: "moz",
  });
  yield cleanup();
});

add_task(function* test_more_than_keyword() {
  do_print("Searching for more than keyworded entry should autoFill it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  yield check_autocomplete({
    search: "mozi",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  yield cleanup();
});

add_task(function* test_less_than_keyword() {
  do_print("Searching for less than keyworded entry should autoFill it");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  yield check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/",
  });
  yield cleanup();
});

add_task(function* test_keyword_casing() {
  do_print("Searching for keyworded entry is case-insensitive");
  yield PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  yield check_autocomplete({
    search: "MoZ",
    autofilled: "MoZ",
    completed: "MoZ"
  });
  yield cleanup();
});
