/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history services
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
    Ci.nsINavHistoryService
  );
} catch (ex) {
  do_throw("Could not get history services\n");
}

// Get tagging service
try {
  var tagssvc = Cc["@mozilla.org/browser/tagging-service;1"].getService(
    Ci.nsITaggingService
  );
} catch (ex) {
  do_throw("Could not get tagging service\n");
}

add_task(async function test_query_only_returns_bookmarks_not_tags() {
  const url = "http://foo.bar/";

  // create 2 bookmarks on the same uri
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "title 1",
    url,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "title 2",
    url,
  });
  // add some tags
  tagssvc.tagURI(uri(url), ["foo", "bar", "foobar", "foo bar"]);

  // check that a generic bookmark query returns only real bookmarks
  let options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;

  let query = histsvc.getNewQuery();
  let result = histsvc.executeQuery(query, options);
  let root = result.root;

  root.containerOpen = true;
  let cc = root.childCount;
  Assert.equal(cc, 2);
  let node1 = root.getChild(0);
  node1 = await PlacesUtils.bookmarks.fetch(node1.bookmarkGuid);
  Assert.equal(node1.parentGuid, PlacesUtils.bookmarks.menuGuid);
  let node2 = root.getChild(1);
  node2 = await PlacesUtils.bookmarks.fetch(node2.bookmarkGuid);
  Assert.equal(node2.parentGuid, PlacesUtils.bookmarks.toolbarGuid);
  root.containerOpen = false;
});
