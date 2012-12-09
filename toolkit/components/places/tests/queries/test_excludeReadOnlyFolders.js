/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The test data for our database, note that the ordering of the results that
// will be returned by the query (the isInQuery: true objects) is IMPORTANT.
// see compareArrayToResult in head_queries.js for more info.
var testData = [
  // Normal folder
  { isInQuery: true, isFolder: true, title: "Folder 1",
    parentFolder: PlacesUtils.toolbarFolderId },

  // Read only folder
  { isInQuery: false, isFolder: true, title: "Folder 2 RO",
    parentFolder: PlacesUtils.toolbarFolderId, readOnly: true }
];

function run_test()
{
  run_next_test();
}

add_task(function test_excludeReadOnlyFolders()
{
  yield task_populateDB(testData);

  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.toolbarFolderId], 1);

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.excludeQueries = true;
  options.excludeReadOnlyFolders = true;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  displayResultSet(root);
  // The readonly folder should not be in our result set.
  do_check_eq(1, root.childCount);
  do_check_eq("Folder 1", root.getChild(0).title);

  root.containerOpen = false;
});
