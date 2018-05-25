/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests inheritance of certain query options like:
 * excludeItems, excludeQueries, expandQueries.
 */

"use strict";

add_task(async function() {
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "folder",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [
          { title: "query",
            url: "place:queryType=" + Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS,
          },
          { title: "bm",
            url: "http://example.com",
          },
          {
            type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
          },
        ]
      },
      { title: "bm",
        url: "http://example.com",
      },
      {
        type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
      },
    ]
  });

  await test_query({}, 3, 3, 2);
  await test_query({ expandQueries: false }, 3, 3, 0);
  await test_query({ excludeItems: true }, 1, 1, 0);
  await test_query({ excludeItems: true, expandQueries: false }, 1, 1, 0);
  await test_query({ excludeItems: true, excludeQueries: true }, 1, 0, 0);
});

async function test_query(opts, expectedRootCc, expectedFolderCc, expectedQueryCc) {
  let query = PlacesUtils.history.getNewQuery();
  query.setParents([PlacesUtils.bookmarks.unfiledGuid], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  for (const [o, v] of Object.entries(opts)) {
    info(`Setting ${o} to ${v}`);
    options[o] = v;
  }
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, expectedRootCc, "Checking root child count");
  if (root.childCount > 0) {
    let folder = root.getChild(0);
    Assert.equal(folder.title, "folder", "Found the expected folder");

    // Check the folder uri doesn't reflect the root options, since those
    // options are inherited and not part of this node declaration.
    checkURIOptions(folder.uri);

    PlacesUtils.asContainer(folder).containerOpen = true;
    Assert.equal(folder.childCount, expectedFolderCc, "Checking folder child count");
    if (folder.childCount) {
      let placeQuery = folder.getChild(0);
      PlacesUtils.asQuery(placeQuery).containerOpen = true;
      Assert.equal(placeQuery.childCount, expectedQueryCc, "Checking query child count");
      placeQuery.containerOpen = false;
    }
    folder.containerOpen = false;
  }
  let f = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    type: PlacesUtils.bookmarks.TYPE_FOLDER
  });
  checkURIOptions(root.getChild(root.childCount - 1).uri);
  await PlacesUtils.bookmarks.remove(f);

  root.containerOpen = false;
}

function checkURIOptions(uri) {
  info("Checking options for uri " + uri);
  let folderOptions = { };
  PlacesUtils.history.queryStringToQuery(uri, {}, folderOptions);
  folderOptions = folderOptions.value;
  Assert.equal(folderOptions.excludeItems, false, "ExcludeItems should not be changed");
  Assert.equal(folderOptions.excludeQueries, false, "ExcludeQueries should not be changed");
  Assert.equal(folderOptions.expandQueries, true, "ExpandQueries should not be changed");
}
