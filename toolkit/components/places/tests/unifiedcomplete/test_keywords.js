/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_non_keyword() {
  info("Searching for non-keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await check_autocomplete({
    search: "moz",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_keyword() {
  info("Searching for keyworded entry should not autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
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
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  await check_autocomplete({
    search: "mozi",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_less_than_keyword() {
  info("Searching for less than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
  });
  await cleanup();
});

add_task(async function test_keyword_casing() {
  info("Searching for keyworded entry is case-insensitive");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
  });
  await PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    keyword: "moz",
  });
  await check_autocomplete({
    search: "MoZ",
    autofilled: "MoZ",
    completed: "MoZ",
  });
  await cleanup();
});

add_task(async function test_less_then_equal_than_keyword_bug_1124238() {
  info("Searching for less than keyworded entry should autoFill it");
  await PlacesTestUtils.addVisits({
    uri: Services.io.newURI("http://mozilla.org/test/"),
    transition: TRANSITION_TYPED,
  });
  await PlacesTestUtils.addVisits("http://mozilla.com/");
  PlacesTestUtils.addBookmarkWithDetails({
    uri: Services.io.newURI("http://mozilla.com/"),
    keyword: "moz",
  });

  let input = await check_autocomplete({
    search: "mo",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
  });

  info(input.textValue);

  // Emulate the input of an additional character. As the input matches a
  // keyword, the completion should equal the keyword and not the URI as before.
  input.textValue = "moz";
  await check_autocomplete({
    input,
    search: "moz",
    autofilled: "moz",
    completed: "moz",
  });

  // Emulate the input of an additional character. The input doesn't match a
  // keyword anymore, it should be autofilled.
  input.textValue = "moz";
  await check_autocomplete({
    input,
    search: "mozi",
    autofilled: "mozilla.org/",
    completed: "http://mozilla.org/",
  });

  await cleanup();
});
