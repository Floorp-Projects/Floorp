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
        ]
      },
      { title: "bm",
        url: "http://example.com",
      },
    ]
  });

  await test_query({}, 2, 2, 3);
  await test_query({ expandQueries: false }, 2, 2, 0);
  await test_query({ excludeItems: true }, 1, 1, 0);
  await test_query({ excludeItems: true, expandQueries: false }, 1, 1, 0);
  await test_query({ excludeItems: true, excludeQueries: true }, 1, 0, 0);
});


async function test_query(opts, expectedRootCc, expectedFolderCc, expectedQueryCc) {
  let query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.unfiledBookmarksFolderId], 1);
  let options = PlacesUtils.history.getNewQueryOptions();
  for (const [o, v] of Object.entries(opts)) {
    info(`setting ${o} to ${v}`);
    options[o] = v;
  }
  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  Assert.equal(root.childCount, expectedRootCc, "Checking root child count");
  if (root.childCount > 0) {
    let folder = root.getChild(0);
    Assert.equal(folder.title, "folder", "Found the expected folder");
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
  root.containerOpen = false;
}
