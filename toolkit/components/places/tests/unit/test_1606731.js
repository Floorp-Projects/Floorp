/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let hs = PlacesUtils.history;

/* Test for diacritic-insensitive history search */

add_task(async function test_execute() {
  const TEST_URL = "http://example.net/El_%C3%81rea_51";
  const SEARCH_TERM = "area";
  await PlacesTestUtils.addVisits(uri(TEST_URL));
  let query = hs.getNewQuery();
  query.searchTerms = SEARCH_TERM;
  let options = hs.getNewQueryOptions();
  let result = hs.executeQuery(query, options);
  result.root.containerOpen = true;
  Assert.ok(result.root.childCount == 1);
});
