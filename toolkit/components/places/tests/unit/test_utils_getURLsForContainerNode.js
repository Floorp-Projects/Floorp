/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Check for correct functionality of PlacesUtils.getURLsForContainerNode and
  * PlacesUtils.hasChildURIs (those helpers share almost all of their code)
  */

var PU = PlacesUtils;
var hs = PU.history;


add_task(async function test_getURLsForContainerNode_folder() {
  do_print("*** TEST: folder");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      // This is the folder we will check for children.
      title: "folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      // Create a folder and a query node inside it, these should not be considered
      // uri nodes.
      children: [{
        title: "inside folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [{
          url: "place:sort=1",
          title: "inside query",
        }]
      }]
    }],
  });

  var query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = hs.getNewQueryOptions();

  do_print("Check folder without uri nodes");
  check_uri_nodes(query, options, 0);

  do_print("Check folder with uri nodes");
  // Add an uri node, this should be considered.
  await PlacesUtils.bookmarks.insert({
    parentGuid: bookmarks[0].guid,
    url: "http://www.mozilla.org/",
    title: "bookmark"
  });
  check_uri_nodes(query, options, 1);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_getURLsForContainerNode_folder_excludeItems() {
  do_print("*** TEST: folder in an excludeItems root");
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [{
      // This is the folder we will check for children.
      title: "folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      // Create a folder and a query node inside it, these should not be considered
      // uri nodes.
      children: [{
        title: "inside folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [{
          url: "place:sort=1",
          title: "inside query",
        }]
      }]
    }],
  });

  var query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = hs.getNewQueryOptions();
  options.excludeItems = true;

  do_print("Check folder without uri nodes");
  check_uri_nodes(query, options, 0);

  do_print("Check folder with uri nodes");
  // Add an uri node, this should be considered.
  await PlacesUtils.bookmarks.insert({
    parentGuid: bookmarks[0].guid,
    url: "http://www.mozilla.org/",
    title: "bookmark"
  });
  check_uri_nodes(query, options, 1);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_getURLsForContainerNode_query() {
  do_print("*** TEST: query");
  // This is the query we will check for children.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "inside query",
    url: "place:folder=BOOKMARKS_MENU&sort=1",
  });

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "inside folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        url: "place:sort=1",
        title: "inside query",
      }]
    }],
  });

  var query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = hs.getNewQueryOptions();

  do_print("Check query without uri nodes");
  check_uri_nodes(query, options, 0);

  do_print("Check query with uri nodes");
  // Add an uri node, this should be considered.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://www.mozilla.org/",
    title: "bookmark"
  });
  check_uri_nodes(query, options, 1);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_getURLsForContainerNode_query_excludeItems() {
  do_print("*** TEST: excludeItems Query");
  // This is the query we will check for children.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "inside query",
    url: "place:folder=BOOKMARKS_MENU&sort=1",
  });

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "inside folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        url: "place:sort=1",
        title: "inside query",
      }]
    }],
  });

  var query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = hs.getNewQueryOptions();
  options.excludeItems = true;

  do_print("Check folder without uri nodes");
  check_uri_nodes(query, options, 0);

  do_print("Check folder with uri nodes");
  // Add an uri node, this should be considered.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://www.mozilla.org/",
    title: "bookmark"
  });
  check_uri_nodes(query, options, 1);

  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function test_getURLsForContainerNode_query_excludeQueries() {
  do_print("*** TEST: !expandQueries Query");
  // This is the query we will check for children.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: "inside query",
    url: "place:folder=BOOKMARKS_MENU&sort=1",
  });

  // Create a folder and a query node inside it, these should not be considered
  // uri nodes.
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.menuGuid,
    children: [{
      title: "inside folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        url: "place:sort=1",
        title: "inside query",
      }]
    }],
  });

  var query = hs.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);
  var options = hs.getNewQueryOptions();
  options.expandQueries = false;

  do_print("Check folder without uri nodes");
  check_uri_nodes(query, options, 0);

  do_print("Check folder with uri nodes");
  // Add an uri node, this should be considered.
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://www.mozilla.org/",
    title: "bookmark"
  });
  check_uri_nodes(query, options, 1);
});

/**
 * Executes a query and checks number of uri nodes in the first container in
 * query's results.  To correctly test a container ensure that the query will
 * return only your container in the first level.
 *
 * @param  aQuery
 *         nsINavHistoryQuery object defining the query
 * @param  aOptions
 *         nsINavHistoryQueryOptions object defining the query's options
 * @param  aExpectedURINodes
 *         number of expected uri nodes
 */
function check_uri_nodes(aQuery, aOptions, aExpectedURINodes) {
  var result = hs.executeQuery(aQuery, aOptions);
  var root = result.root;
  root.containerOpen = true;
  var node = root.getChild(0);
  Assert.equal(PU.hasChildURIs(node), aExpectedURINodes > 0);
  Assert.equal(PU.getURLsForContainerNode(node).length, aExpectedURINodes);
  root.containerOpen = false;
}
