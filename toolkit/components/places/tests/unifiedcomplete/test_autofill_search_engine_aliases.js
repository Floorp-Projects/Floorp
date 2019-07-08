/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests autofilling search engine token ("@") aliases.

"use strict";

const TEST_ENGINE_NAME = "test autofill aliases";
const TEST_ENGINE_ALIAS = "@autofilltest";

add_task(async function init() {
  // Add an engine with an "@" alias.
  await Services.search.addEngineWithDetails(TEST_ENGINE_NAME, {
    alias: TEST_ENGINE_ALIAS,
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async () => {
    let engine = Services.search.getEngineByName(TEST_ENGINE_NAME);
    Assert.ok(engine);
    await Services.search.removeEngine(engine);
  });
});

// Searching for @autofi should autofill to @autofilltest.
add_task(async function basic() {
  // Add a history visit that should normally match but for the fact that the
  // search uses an @ alias.  When an @ alias is autofilled, there should be no
  // other matches except the autofill heuristic match.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    title: TEST_ENGINE_ALIAS,
  });

  let autofilledValue = TEST_ENGINE_ALIAS + " ";
  let completedURL = PlacesUtils.mozActionURI("searchengine", {
    engineName: TEST_ENGINE_NAME,
    alias: TEST_ENGINE_ALIAS,
    input: autofilledValue,
    searchQuery: "",
  });
  await check_autocomplete({
    search: TEST_ENGINE_ALIAS.substr(
      0,
      Math.round(TEST_ENGINE_ALIAS.length / 2)
    ),
    autofilled: autofilledValue,
    completed: completedURL,
    matches: [
      {
        value: autofilledValue,
        comment: TEST_ENGINE_NAME,
        uri: completedURL,
        style: ["autofill", "action", "searchengine", "heuristic"],
      },
    ],
  });
  await cleanup();
});

// Searching for @AUTOFI should autofill to @AUTOFIlltest, preserving the case
// in the search string.
add_task(async function preserveCase() {
  // Add a history visit that should normally match but for the fact that the
  // search uses an @ alias.  When an @ alias is autofilled, there should be no
  // other matches except the autofill heuristic match.
  await PlacesTestUtils.addVisits({
    uri: "http://example.com/",
    title: TEST_ENGINE_ALIAS,
  });

  let search = TEST_ENGINE_ALIAS.toUpperCase().substr(
    0,
    Math.round(TEST_ENGINE_ALIAS.length / 2)
  );
  let alias = search + TEST_ENGINE_ALIAS.substr(search.length);
  let autofilledValue = alias + " ";
  let completedURL = PlacesUtils.mozActionURI("searchengine", {
    engineName: TEST_ENGINE_NAME,
    alias,
    input: autofilledValue,
    searchQuery: "",
  });
  await check_autocomplete({
    search,
    autofilled: autofilledValue,
    completed: completedURL,
    matches: [
      {
        value: autofilledValue,
        comment: TEST_ENGINE_NAME,
        uri: completedURL,
        style: ["autofill", "action", "searchengine", "heuristic"],
      },
    ],
  });
  await cleanup();
});
