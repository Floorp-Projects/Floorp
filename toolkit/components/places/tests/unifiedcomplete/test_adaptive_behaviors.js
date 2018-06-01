/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test for Bug 527311
// Addressbar suggests adaptive results regardless of the requested behavior.

const TEST_URL = "http://adapt.mozilla.org/";
const SEARCH_STRING = "adapt";
const SUGGEST_TYPES = ["history", "bookmark", "openpage"];

add_task(async function test_adaptive_search_specific() {
  Services.prefs.setBoolPref("browser.urlbar.autoFill", false);

  // Add a bookmark to our url.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test_book",
    url: TEST_URL,
  });
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  // We want to search only history.
  for (let type of SUGGEST_TYPES) {
    type == "history" ? Services.prefs.setBoolPref("browser.urlbar.suggest." + type, true)
                      : Services.prefs.setBoolPref("browser.urlbar.suggest." + type, false);
  }

  // Add an adaptive entry.
  await addAdaptiveFeedback(TEST_URL, SEARCH_STRING);

  await check_autocomplete({
    search: SEARCH_STRING,
    matches: [],
  });
});
