/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var testData = [];
var timeInMicroseconds = PlacesUtils.toPRTime(Date.now() - 10000);

function newTimeInMicroseconds() {
  timeInMicroseconds = timeInMicroseconds + 1000;
  return timeInMicroseconds;
}

function createTestData() {
  function generateVisits(aPage) {
    for (var i = 0; i < aPage.visitCount; i++) {
      testData.push({ isInQuery: aPage.inQuery,
                      isVisit: true,
                      title: aPage.title,
                      uri: aPage.uri,
                      lastVisit: newTimeInMicroseconds(),
                      isTag: aPage.tags && aPage.tags.length > 0,
                      tagArray: aPage.tags });
    }
  }

  var pages = [
    { uri: "http://foo.com/", title: "amo", tags: ["moz"], visitCount: 3, inQuery: true },
    { uri: "http://moilla.com/", title: "bMoz", tags: ["bugzilla"], visitCount: 5, inQuery: true },
    { uri: "http://foo.mail.com/changeme1.html", title: "c Moz", visitCount: 7, inQuery: true },
    { uri: "http://foo.mail.com/changeme2.html", tags: ["moz"], title: "", visitCount: 1, inQuery: false },
    { uri: "http://foo.mail.com/changeme3.html", title: "zydeco", visitCount: 5, inQuery: false },
  ];
  pages.forEach(generateVisits);
}

/**
 * This test will test Queries that use relative search terms and URI options
 */
add_task(async function test_results_as_visit() {
   createTestData();
   await task_populateDB(testData);
   var query = PlacesUtils.history.getNewQuery();
   query.searchTerms = "moz";
   query.minVisits = 2;

   // Options
   var options = PlacesUtils.history.getNewQueryOptions();
   options.sortingMode = options.SORT_BY_VISITCOUNT_ASCENDING;
   options.resultType = options.RESULTS_AS_VISIT;

   // Results
   var result = PlacesUtils.history.executeQuery(query, options);
   var root = result.root;
   root.containerOpen = true;

   info("Number of items in result set: " + root.childCount);
   for (let i = 0; i < root.childCount; ++i) {
     info("result: " + root.getChild(i).uri + " Title: " + root.getChild(i).title);
   }

   // Check our inital result set
   compareArrayToResult(testData, root);

   // If that passes, check liveupdate
   // Add to the query set
   info("Adding item to query");
   var tmp = [];
   for (let i = 0; i < 2; i++) {
     tmp.push({ isVisit: true,
                uri: "http://foo.com/added.html",
                title: "ab moz" });
   }
   await task_populateDB(tmp);
   for (let i = 0; i < 2; i++)
     Assert.equal(root.getChild(i).title, "ab moz");

   // Update an existing URI
   info("Updating Item");
   var change2 = [{ isVisit: true,
                    title: "moz",
                    uri: "http://foo.mail.com/changeme2.html" }];
   await task_populateDB(change2);
   Assert.ok(isInResult(change2, root));

   // Update some visits - add one and take one out of query set, and simply
   // change one so that it still applies to the query.
   info("Updating More Items");
   var change3 = [{ isVisit: true,
                    lastVisit: newTimeInMicroseconds(),
                    uri: "http://foo.mail.com/changeme1.html",
                    title: "foo"},
                  { isVisit: true,
                    lastVisit: newTimeInMicroseconds(),
                    uri: "http://foo.mail.com/changeme3.html",
                    title: "moz",
                    isTag: true,
                    tagArray: ["foo", "moz"] }];
   await task_populateDB(change3);
   Assert.ok(!isInResult({uri: "http://foo.mail.com/changeme1.html"}, root));
   Assert.ok(isInResult({uri: "http://foo.mail.com/changeme3.html"}, root));

   // And now, delete one
   info("Delete item outside of batch");
   var change4 = [{ isVisit: true,
                    lastVisit: newTimeInMicroseconds(),
                    uri: "http://moilla.com/",
                    title: "mo,z" }];
   await task_populateDB(change4);
   Assert.ok(!isInResult(change4, root));

   root.containerOpen = false;
});
