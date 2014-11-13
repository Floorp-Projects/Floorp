/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  run_next_test();
}

add_task(function* test_queryMultipleFolders() {
  // adding bookmarks in the folders
  let folderIds = [];
  let bookmarkIds = [];
  for (let i = 0; i < 3; ++i) {
    folderIds.push(PlacesUtils.bookmarks.createFolder(PlacesUtils.bookmarksMenuFolderId,
                                                      "Folder"+ i, PlacesUtils.bookmarks.DEFAULT_INDEX));
    for(let j = 0; j < 7; ++j) {
      bookmarkIds.push(PlacesUtils.bookmarks.insertBookmark(folderIds[i],
                                                            uri("http://Bookmark" + i + "_" + j + ".com"),
                                                            PlacesUtils.bookmarks.DEFAULT_INDEX, ""));
    }
  }

  // using queryStringToQueries
  let query = {};
  let options = {};
  let maxResults = 20;
  let queryString = "place:" + folderIds.map((id) => {
    return "folder=" + id;
  }).join('&') + "&sort=5&maxResults=" + maxResults;
  PlacesUtils.history.queryStringToQueries(queryString, query, {}, options);
  let rootNode = PlacesUtils.history.executeQuery(query.value[0],options.value).root;
  rootNode.containerOpen = true;
  let resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkIds[i], node.itemId, node.uri);
  }
  rootNode.containerOpen = false;

  // using getNewQuery and getNewQueryOptions
  query = PlacesUtils.history.getNewQuery();
  options = PlacesUtils.history.getNewQueryOptions();
  query.setFolders(folderIds, folderIds.length);
  options.sortingMode = options.SORT_BY_URI_ASCENDING;
  options.maxResults = maxResults;
  rootNode = PlacesUtils.history.executeQuery(query, options).root;
  rootNode.containerOpen = true;
  resultLength = rootNode.childCount;
  Assert.equal(resultLength, maxResults);
  for (let i = 0; i < resultLength; ++i) {
    let node = rootNode.getChild(i);
    Assert.equal(bookmarkIds[i], node.itemId, node.uri);
  }
  rootNode.containerOpen = false;
});