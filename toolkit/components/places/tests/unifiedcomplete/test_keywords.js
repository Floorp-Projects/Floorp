/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_non_keyword() {
  info("Searching for non-keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/") });
  await check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_keyword() {
  info("Searching for keyworded entry should not autoFill it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  await check_autocomplete({
    search: "moz",
    autofilled: "moz",
    completed: "moz",
  });
  await cleanup();
});

add_task(async function test_more_than_keyword() {
  info("Searching for more than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  await check_autocomplete({
    search: "mozi",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/"
  });
  await cleanup();
});

add_task(async function test_less_than_keyword() {
  info("Searching for less than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_keyword_casing() {
  info("Searching for keyworded entry is case-insensitive");
  await PlacesTestUtils.addVisits({
    uri: NetUtil.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED
  });
  await addBookmark({ uri: NetUtil.newURI("http://mozilla.org/test/"), keyword: "moz" });
  await check_autocomplete({
    search: "MoZ",
    autofilled: "MoZ",
    completed: "MoZ"
  });
  await cleanup();
});
