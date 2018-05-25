/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch (ex) {
  do_throw("Could not get history service\n");
}

add_task(async function test_hierarchical_query() {
  let bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "test folder",
      type: PlacesUtils.bookmarks.TYPE_FOLDER,
      children: [{
        title: "1 title",
        url: "http://a1.com/",
      }, {
        title: "subfolder 1",
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        children: [{
          title: "2 title",
          url: "http://a2.com/",
        }, {
          title: "subfolder 2",
          type: PlacesUtils.bookmarks.TYPE_FOLDER,
          children: [{
            title: "3 title",
            url: "http://a3.com/",
          }]
        }]
      }]
    }]
  });

  let [folderGuid, b1, sf1, b2, sf2, b3] = bookmarks.map((bookmark) => bookmark.guid);

  // bookmark query that should result in the "hierarchical" result
  // because there is one query, one folder,
  // no begin time, no end time, no domain, no uri, no search term
  // and no max results.  See GetSimpleBookmarksQueryFolder()
  // for more details.
  var options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  var query = histsvc.getNewQuery();
  query.setParents([folderGuid], 1);
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 2);
  Assert.equal(root.getChild(0).bookmarkGuid, b1);
  Assert.equal(root.getChild(1).bookmarkGuid, sf1);

  // check the contents of the subfolder
  var sf1Node = root.getChild(1);
  sf1Node = sf1Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf1Node.containerOpen = true;
  Assert.equal(sf1Node.childCount, 2);
  Assert.equal(sf1Node.getChild(0).bookmarkGuid, b2);
  Assert.equal(sf1Node.getChild(1).bookmarkGuid, sf2);

  // check the contents of the subfolder's subfolder
  var sf2Node = sf1Node.getChild(1);
  sf2Node = sf2Node.QueryInterface(Ci.nsINavHistoryContainerResultNode);
  sf2Node.containerOpen = true;
  Assert.equal(sf2Node.childCount, 1);
  Assert.equal(sf2Node.getChild(0).bookmarkGuid, b3);

  sf2Node.containerOpen = false;
  sf1Node.containerOpen = false;
  root.containerOpen = false;

  // bookmark query that should result in a flat list
  // because we specified max results
  options = histsvc.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_BOOKMARKS;
  options.maxResults = 10;
  query = histsvc.getNewQuery();
  query.setParents([folderGuid], 1);
  result = histsvc.executeQuery(query, options);
  root = result.root;
  root.containerOpen = true;
  Assert.equal(root.childCount, 3);
  Assert.equal(root.getChild(0).bookmarkGuid, b1);
  Assert.equal(root.getChild(1).bookmarkGuid, b2);
  Assert.equal(root.getChild(2).bookmarkGuid, b3);
  root.containerOpen = false;

  // XXX TODO
  // test that if we have: more than one query,
  // multiple folders, a begin time, an end time, a domain, a uri
  // or a search term, that we get the (correct) flat list results
  // (like we do when specified maxResults)
});
