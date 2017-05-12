/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_casing_1() {
  do_print("Searching for cased entry 1");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "MOZ",
    autofilled: "MOZilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_casing_2() {
  do_print("Searching for cased entry 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/T",
    completed: "mozilla.org/T"
  });
  await cleanup();
});

add_task(async function test_casing_3() {
  do_print("Searching for cased entry 3");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mozilla.org/T",
    autofilled: "mozilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_casing_4() {
  do_print("Searching for cased entry 4");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mOzilla.org/t",
    autofilled: "mOzilla.org/t",
    completed: "mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_casing_5() {
  do_print("Searching for cased entry 5");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "mOzilla.org/T",
    autofilled: "mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_casing() {
  do_print("Searching for untrimmed cased entry");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://mOz",
    autofilled: "http://mOzilla.org/",
    completed: "http://mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_www_casing() {
  do_print("Searching for untrimmed cased entry with www");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://www.mOz",
    autofilled: "http://www.mOzilla.org/",
    completed: "http://www.mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_casing() {
  do_print("Searching for untrimmed cased entry with path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://mOzilla.org/t",
    autofilled: "http://mOzilla.org/t",
    completed: "http://mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_casing_2() {
  do_print("Searching for untrimmed cased entry with path 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://mOzilla.org/T",
    autofilled: "http://mOzilla.org/Test/",
    completed: "http://mozilla.org/Test/"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_www_casing() {
  do_print("Searching for untrimmed cased entry with www and path");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://www.mOzilla.org/t",
    autofilled: "http://www.mOzilla.org/t",
    completed: "http://www.mOzilla.org/t"
  });
  await cleanup();
});

add_task(async function test_untrimmed_path_www_casing_2() {
  do_print("Searching for untrimmed cased entry with www and path 2");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://www.mozilla.org/Test/"),
    transition: TRANSITION_TYPED
  });
  await check_autocomplete({
    search: "http://www.mOzilla.org/T",
    autofilled: "http://www.mOzilla.org/Test/",
    completed: "http://www.mozilla.org/Test/"
  });
  await cleanup();
});
