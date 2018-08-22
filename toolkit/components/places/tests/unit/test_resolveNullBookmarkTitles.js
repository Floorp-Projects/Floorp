/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  let uri1 = uri("http://foo.tld/");
  let uri2 = uri("https://bar.tld/");

  await PlacesTestUtils.addVisits([
    { uri: uri1, title: "foo title" },
    { uri: uri2, title: "bar title" },
  ]);
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: uri1,
    title: null,
  });
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: uri2,
    title: null,
  });

  PlacesUtils.tagging.tagURI(uri1, ["tag 1"]);
  PlacesUtils.tagging.tagURI(uri2, ["tag 2"]);
});

add_task(async function testTagQuery() {
  let options = PlacesUtils.history.getNewQueryOptions();
  let query = PlacesUtils.history.getNewQuery();
  query.tags = ["tag 1"];
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 1);
  Assert.equal(root.getChild(0).title, "");
  root.containerOpen = false;
});
