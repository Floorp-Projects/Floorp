/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var testData = [
  { isInQuery: true,
    isDetails: true,
    title: "bmoz",
    uri: "http://foo.com/",
    isBookmark: true,
    isTag: true,
    tagArray: ["bugzilla"] },

  { isInQuery: true,
    isDetails: true,
    title: "C Moz",
    uri: "http://foo.com/changeme1.html",
    isBookmark: true,
    isTag: true,
    tagArray: ["moz", "bugzilla"] },

  { isInQuery: false,
    isDetails: true,
    title: "amo",
    uri: "http://foo2.com/",
    isBookmark: true,
    isTag: true,
    tagArray: ["moz"] },

  { isInQuery: false,
    isDetails: true,
    title: "amo",
    uri: "http://foo.com/changeme2.html",
    isBookmark: true },
];

function getIdForTag(aTagName) {
  var id = -1;
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([PlacesUtils.tagsFolderId], 1);
  var options = PlacesUtils.history.getNewQueryOptions();
  var root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;
  var cc = root.childCount;
  Assert.equal(root.childCount, 2);
  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    if (node.title == aTagName) {
      id = node.itemId;
      break;
    }
  }
  root.containerOpen = false;
  return id;
}

 /**
  * This test will test Queries that use relative search terms and URI options
  */
add_task(async function test_results_as_tag_contents_query() {
  await task_populateDB(testData);

  // Get tag id.
  let tagId = getIdForTag("bugzilla");
  Assert.ok(tagId > 0);

  var options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_TAG_CONTENTS;
  var query = PlacesUtils.history.getNewQuery();
  query.setFolders([tagId], 1);

  var root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  displayResultSet(root);
  // Cannot use compare array to results, since results ordering is hardcoded
  // and depending on lastModified (that could have VM timers issues).
  testData.forEach(function(aEntry) {
    if (aEntry.isInResult)
      Assert.ok(isInResult({uri: "http://foo.com/added.html"}, root));
  });

  // If that passes, check liveupdate
  // Add to the query set
  var change1 = { isVisit: true,
                  isDetails: true,
                  uri: "http://foo.com/added.html",
                  title: "mozadded",
                  isBookmark: true,
                  isTag: true,
                  tagArray: ["moz", "bugzilla"] };
  info("Adding item to query");
  await task_populateDB([change1]);
  info("These results should have been LIVE UPDATED with the new addition");
  displayResultSet(root);
  Assert.ok(isInResult(change1, root));

  // Add one by adding a tag, remove one by removing search term.
  info("Updating items");
  var change2 = [{ isDetails: true,
                   uri: "http://foo3.com/",
                   title: "foo"},
                 { isDetails: true,
                   uri: "http://foo.com/changeme2.html",
                   title: "zydeco",
                   isBookmark: true,
                   isTag: true,
                   tagArray: ["bugzilla", "moz"] }];
  await task_populateDB(change2);
  Assert.ok(!isInResult({uri: "http://fooz.com/"}, root));
  Assert.ok(isInResult({uri: "http://foo.com/changeme2.html"}, root));

  // Test removing a tag updates us.
  info("Deleting item");
  PlacesUtils.tagging.untagURI(uri("http://foo.com/changeme2.html"), ["bugzilla"]);
  Assert.ok(!isInResult({uri: "http://foo.com/changeme2.html"}, root));

  root.containerOpen = false;
});
