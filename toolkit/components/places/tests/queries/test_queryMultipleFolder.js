/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var folderIds = [];
var folderGuids = [];
var bookmarkGuids = [];

add_task(async function setup() {
  // adding bookmarks in the folders
  for (let i = 0; i < 3; ++i) {
    let folder = await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.menuGuid,
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      title: `Folder${i}`
    });
    folderGuids.push(folder.guid);

    for (let j = 0; j < 7; ++j) {
      let bm = await PlacesUtils.bookmarks.insert({
        parentGuid: folderGuids[i],
        url: `http://Bookmark${i}_${j}.com`,
        title: ""
      });
      bookmarkGuids.push(bm.guid);
    }
  }
});

add_task(async function test_queryMultipleFolders_ids() {
  // using queryStringToQuery
  let query = {}, options = {};
  let maxResults = 20;
  let queryString = `place:${folderGuids.map(guid => "parent=" + guid).join("&")}&sort=5&maxResults=${maxResults}`;
  PlacesUtils.history.queryStringToQuery(queryString, query, options);
  let rootNode = PlacesUtils.history.executeQuery(query.value, options.value).root;
  rootNode.containerOpen = true;
  let resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkGuids[i], node.bookmarkGuid, node.uri);
  }
  rootNode.containerOpen = false;

  // using getNewQuery and getNewQueryOptions
  query = PlacesUtils.history.getNewQuery();
  options = PlacesUtils.history.getNewQueryOptions();
  query.setParents(folderGuids, folderGuids.length);
  options.sortingMode = options.SORT_BY_URI_ASCENDING;
  options.maxResults = maxResults;
  rootNode = PlacesUtils.history.executeQuery(query, options).root;
  rootNode.containerOpen = true;
  resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkGuids[i], node.bookmarkGuid, node.uri);
  }
  rootNode.containerOpen = false;
});

add_task(async function test_queryMultipleFolders_guids() {
  // using queryStringToQuery
  let query = {}, options = {};
  let maxResults = 20;
  let queryString = `place:${folderGuids.map((guid) => "parent=" + guid).join("&")}&sort=5&maxResults=${maxResults}`;
  PlacesUtils.history.queryStringToQuery(queryString, query, options);
  let rootNode = PlacesUtils.history.executeQuery(query.value, options.value).root;
  rootNode.containerOpen = true;
  let resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkGuids[i], node.bookmarkGuid, node.uri);
  }
  rootNode.containerOpen = false;

  // using getNewQuery and getNewQueryOptions
  query = PlacesUtils.history.getNewQuery();
  options = PlacesUtils.history.getNewQueryOptions();
  query.setParents(folderGuids, folderGuids.length);
  options.sortingMode = options.SORT_BY_URI_ASCENDING;
  options.maxResults = maxResults;
  rootNode = PlacesUtils.history.executeQuery(query, options).root;
  rootNode.containerOpen = true;
  resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkGuids[i], node.bookmarkGuid, node.uri);
  }
  rootNode.containerOpen = false;
});
