/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL =
  { u: "http://www.google.com/search?q=testing%3Bthis&ie=utf-8&oe=utf-8&aq=t&rls=org.mozilla:en-US:unofficial&client=firefox-a",
    s: "goog" };

add_task(async function() {
  print("Testing url: " + TEST_URL.u);
  await PlacesTestUtils.addVisits(uri(TEST_URL.u));

  let query = PlacesUtils.history.getNewQuery();
  query.searchTerms = TEST_URL.s;
  let options = PlacesUtils.history.getNewQueryOptions();

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  Assert.equal(cc, 1);

  print("Checking url is in the query.");
  let node = root.getChild(0);
  print("Found " + node.uri);

  Assert.ok(await PlacesTestUtils.isPageInDB(TEST_URL.u));

  root.containerOpen = false;
  await PlacesUtils.history.remove(node.uri);

  Assert.equal(false, await PlacesTestUtils.isPageInDB(TEST_URL.u));
});
