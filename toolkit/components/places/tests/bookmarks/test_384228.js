/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * test querying for bookmarks in multiple folders.
 */
add_task(function* search_bookmark_in_folder() {
  let testFolder1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bug 384228 test folder 1"
  });
  Assert.equal(testFolder1.index, 0);

  let testFolder2 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bug 384228 test folder 2"
  });
  Assert.equal(testFolder2.index, 1);

  let testFolder3 = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bug 384228 test folder 3"
  });
  Assert.equal(testFolder3.index, 2);

  let b1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder1.guid,
    url: "http://foo.tld/",
    title: "title b1 (folder 1)"
  });
  Assert.equal(b1.index, 0);

  let b2 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder1.guid,
    url: "http://foo.tld/",
    title: "title b2 (folder 1)"
  });
  Assert.equal(b2.index, 1);

  let b3 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder2.guid,
    url: "http://foo.tld/",
    title: "title b3 (folder 2)"
  });
  Assert.equal(b3.index, 0);

  let b4 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder3.guid,
    url: "http://foo.tld/",
    title: "title b4 (folder 3)"
  });
  Assert.equal(b4.index, 0);

  // also test recursive search
  let testFolder1_1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder1.guid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bug 384228 test folder 1.1"
  });
  Assert.equal(testFolder1_1.index, 2);

  let b5 = yield PlacesUtils.bookmarks.insert({
    parentGuid: testFolder1_1.guid,
    url: "http://foo.tld/",
    title: "title b5 (folder 1.1)"
  });
  Assert.equal(b5.index, 0);


  // query folder 1, folder 2 and get 4 bookmarks
  let folderIds = [];
  folderIds.push(yield PlacesUtils.promiseItemId(testFolder1.guid));
  folderIds.push(yield PlacesUtils.promiseItemId(testFolder2.guid));

  let hs = PlacesUtils.history;
  let options = hs.getNewQueryOptions();
  let query = hs.getNewQuery();
  query.searchTerms = "title";
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  query.setFolders(folderIds, folderIds.length);
  let rootNode = hs.executeQuery(query, options).root;
  rootNode.containerOpen = true;

  // should not match item from folder 3
  Assert.equal(rootNode.childCount, 4);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b1.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b3.guid);
  Assert.equal(rootNode.getChild(3).bookmarkGuid, b5.guid);

  rootNode.containerOpen = false;
});
