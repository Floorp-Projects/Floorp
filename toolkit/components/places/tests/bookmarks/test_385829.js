/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* search_bookmark_by_lastModified_dateDated() {
  // test search on folder with various sorts and max results
  // see bug #385829 for more details
  let folder = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: "bug 385829 test"
  });

  let now = new Date();
  // ensure some unique values for date added and last modified
  // for date added:    b1 < b2 < b3 < b4
  // for last modified: b1 > b2 > b3 > b4
  let b1 = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: "http://a1.com/",
    title: "1 title",
    dateAdded: new Date(now.getTime() + 1000)
  });
  let b2 = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: "http://a2.com/",
    title: "2 title",
    dateAdded: new Date(now.getTime() + 2000)
  });
  let b3 = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: "http://a3.com/",
    title: "3 title",
    dateAdded: new Date(now.getTime() + 3000)
  });
  let b4 = yield PlacesUtils.bookmarks.insert({
    parentGuid: folder.guid,
    url: "http://a4.com/",
    title: "4 title",
    dateAdded: new Date(now.getTime() + 4000)
  });

  // make sure lastModified is larger than dateAdded
  let modifiedTime = new Date(now.getTime() + 5000);
  yield PlacesUtils.bookmarks.update({
    guid: b1.guid,
    lastModified: new Date(modifiedTime.getTime() + 4000)
  });
  yield PlacesUtils.bookmarks.update({
    guid: b2.guid,
    lastModified: new Date(modifiedTime.getTime() + 3000)
  });
  yield PlacesUtils.bookmarks.update({
    guid: b3.guid,
    lastModified: new Date(modifiedTime.getTime() + 2000)
  });
  yield PlacesUtils.bookmarks.update({
    guid: b4.guid,
    lastModified: new Date(modifiedTime.getTime() + 1000)
  });

  let hs = PlacesUtils.history;
  let options = hs.getNewQueryOptions();
  let query = hs.getNewQuery();
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 3;
  let folderIds = [];
  folderIds.push(yield PlacesUtils.promiseItemId(folder.guid));
  query.setFolders(folderIds, 1);

  let result = hs.executeQuery(query, options);
  let rootNode = result.root;
  rootNode.containerOpen = true;

  // test SORT_BY_DATEADDED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b1.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b3.guid);
  Assert.ok(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  Assert.ok(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_DATEADDED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b3.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b1.guid);
  Assert.ok(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  Assert.ok(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);

  // test SORT_BY_LASTMODIFIED_ASCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b3.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b1.guid);
  Assert.ok(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  Assert.ok(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);

  // test SORT_BY_LASTMODIFIED_DESCENDING (live update)
  result.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;

  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b1.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b3.guid);
  Assert.ok(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  Assert.ok(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_ASCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_ASCENDING;
  result = hs.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b1.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b3.guid);
  Assert.ok(rootNode.getChild(0).dateAdded <
                rootNode.getChild(1).dateAdded);
  Assert.ok(rootNode.getChild(1).dateAdded <
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_DATEADDED_DESCENDING
  options.sortingMode = options.SORT_BY_DATEADDED_DESCENDING;
  result = hs.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b4.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b3.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b2.guid);
  Assert.ok(rootNode.getChild(0).dateAdded >
                rootNode.getChild(1).dateAdded);
  Assert.ok(rootNode.getChild(1).dateAdded >
                rootNode.getChild(2).dateAdded);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_ASCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_ASCENDING;
  result = hs.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b4.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b3.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b2.guid);
  Assert.ok(rootNode.getChild(0).lastModified <
                rootNode.getChild(1).lastModified);
  Assert.ok(rootNode.getChild(1).lastModified <
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;

  // test SORT_BY_LASTMODIFIED_DESCENDING
  options.sortingMode = options.SORT_BY_LASTMODIFIED_DESCENDING;
  result = hs.executeQuery(query, options);
  rootNode = result.root;
  rootNode.containerOpen = true;
  Assert.equal(rootNode.childCount, 3);
  Assert.equal(rootNode.getChild(0).bookmarkGuid, b1.guid);
  Assert.equal(rootNode.getChild(1).bookmarkGuid, b2.guid);
  Assert.equal(rootNode.getChild(2).bookmarkGuid, b3.guid);
  Assert.ok(rootNode.getChild(0).lastModified >
                rootNode.getChild(1).lastModified);
  Assert.ok(rootNode.getChild(1).lastModified >
                rootNode.getChild(2).lastModified);
  rootNode.containerOpen = false;
});
