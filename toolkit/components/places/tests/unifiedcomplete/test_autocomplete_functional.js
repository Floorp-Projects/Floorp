/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Functional tests for inline autocomplete

add_task(async function test_urls_order() {
  info("Add urls, check for correct order");
  let places = [
    { uri: NetUtil.newURI("http://visit1.mozilla.org") },
    { uri: NetUtil.newURI("http://visit2.mozilla.org") },
  ];
  await PlacesTestUtils.addVisits(places);
  await check_autocomplete({
    search: "vis",
    autofilled: "visit2.mozilla.org/",
    completed: "http://visit2.mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_bookmark_first() {
  info("With a bookmark and history, the query result should be the bookmark");
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: NetUtil.newURI("http://bookmark1.mozilla.org/"),
  });
  await PlacesTestUtils.addVisits(
    NetUtil.newURI("http://bookmark1.mozilla.org/foo")
  );
  await check_autocomplete({
    search: "bookmark",
    autofilled: "bookmark1.mozilla.org/",
    completed: "http://bookmark1.mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_complete_querystring() {
  info("Check to make sure we autocomplete after ?");
  await PlacesTestUtils.addVisits(
    NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious")
  );
  await check_autocomplete({
    search: "smokey.mozilla.org/foo?",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious",
  });
  await cleanup();
});

add_task(async function test_complete_fragment() {
  info("Check to make sure we autocomplete after #");
  await PlacesTestUtils.addVisits(
    NetUtil.newURI("http://smokey.mozilla.org/foo?bacon=delicious#bar")
  );
  await check_autocomplete({
    search: "smokey.mozilla.org/foo?bacon=delicious#bar",
    autofilled: "smokey.mozilla.org/foo?bacon=delicious#bar",
    completed: "http://smokey.mozilla.org/foo?bacon=delicious#bar",
  });
  await cleanup();
});
