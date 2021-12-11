/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_query_title_update() {
  try {
    var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(
      Ci.nsINavHistoryService
    );
  } catch (ex) {
    do_throw("Unable to initialize Places services");
  }

  // create a query bookmark
  let bmQuery = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "test query",
    url: "place:",
  });

  // query for that query
  var options = histsvc.getNewQueryOptions();
  let query = histsvc.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.toolbarGuid]);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var queryNode = root.getChild(0);
  Assert.equal(queryNode.title, "test query");

  // change the title
  await PlacesUtils.bookmarks.update({
    guid: bmQuery.guid,
    title: "foo",
  });

  // confirm the node was updated
  Assert.equal(queryNode.title, "foo");

  root.containerOpen = false;
});
