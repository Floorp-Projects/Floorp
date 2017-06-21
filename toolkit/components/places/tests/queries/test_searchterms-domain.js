/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 // The test data for our database, note that the ordering of the results that
 // will be returned by the query (the isInQuery: true objects) is IMPORTANT.
 // see compareArrayToResult in head_queries.js for more info.
 var testData = [
   // Test ftp protocol - vary the title length, embed search term
   {isInQuery: true, isVisit: true, isDetails: true,
    uri: "ftp://foo.com/ftp", lastVisit: lastweek,
    title: "hugelongconfmozlagurationofwordswithasearchtermsinit whoo-hoo"},

   // Test flat domain with annotation, search term in sentence
   {isInQuery: true, isVisit: true, isDetails: true, isPageAnnotation: true,
    uri: "http://foo.com/", annoName: "moz/test", annoVal: "val",
    lastVisit: lastweek, title: "you know, moz is cool"},

   // Test subdomain included with isRedirect=true, different transtype
   {isInQuery: true, isVisit: true, isDetails: true, title: "amozzie",
    isRedirect: true, uri: "http://mail.foo.com/redirect", lastVisit: old,
    referrer: "http://myreferrer.com", transType: PlacesUtils.history.TRANSITION_LINK},

   // Test subdomain inclued, search term at end
   {isInQuery: true, isVisit: true, isDetails: true,
    uri: "http://mail.foo.com/yiihah", title: "blahmoz", lastVisit: daybefore},

   // Test www. style URI is included, with a tag
   {isInQuery: true, isVisit: true, isDetails: true, isTag: true,
    uri: "http://www.foo.com/yiihah", tagArray: ["moz"],
    lastVisit: yesterday, title: "foo"},

   // Test https protocol
   {isInQuery: true, isVisit: true, isDetails: true, title: "moz",
    uri: "https://foo.com/", lastVisit: today},

   // Begin the invalid queries: wrong search term
   {isInQuery: false, isVisit: true, isDetails: true, title: "m o z",
    uri: "http://foo.com/tooearly.php", lastVisit: today},

   // Test bad URI
   {isInQuery: false, isVisit: true, isDetails: true, title: "moz",
    uri: "http://sffoo.com/justwrong.htm", lastVisit: yesterday},

   // Test what we do with escaping in titles
   {isInQuery: false, isVisit: true, isDetails: true, title: "m%0o%0z",
    uri: "http://foo.com/changeme1.htm", lastVisit: yesterday},

   // Test another invalid title - for updating later
   {isInQuery: false, isVisit: true, isDetails: true, title: "m,oz",
    uri: "http://foo.com/changeme2.htm", lastVisit: yesterday}];

/**
 * This test will test Queries that use relative search terms and domain options
 */
add_task(async function test_searchterms_domain() {
  await task_populateDB(testData);
  var query = PlacesUtils.history.getNewQuery();
  query.searchTerms = "moz";
  query.domain = "foo.com";
  query.domainIsHost = false;

  // Options
  var options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_ASCENDING;
  options.resultType = options.RESULTS_AS_URI;

  // Results
  var result = PlacesUtils.history.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;

  do_print("Number of items in result set: " + root.childCount);
  for (var i = 0; i < root.childCount; ++i) {
    do_print("result: " + root.getChild(i).uri + " Title: " + root.getChild(i).title);
  }

  // Check our inital result set
  compareArrayToResult(testData, root);

  // If that passes, check liveupdate
  // Add to the query set
  do_print("Adding item to query");
  var change1 = [{isVisit: true, isDetails: true, uri: "http://foo.com/added.htm",
                  title: "moz", transType: PlacesUtils.history.TRANSITION_LINK}];
  await task_populateDB(change1);
  do_check_true(isInResult(change1, root));

  // Update an existing URI
  do_print("Updating Item");
  var change2 = [{isDetails: true, uri: "http://foo.com/changeme1.htm",
                  title: "moz" }];
  await task_populateDB(change2);
  do_check_true(isInResult(change2, root));

  // Add one and take one out of query set, and simply change one so that it
  // still applies to the query.
  do_print("Updating More Items");
  var change3 = [{isDetails: true, uri: "http://foo.com/changeme2.htm",
                  title: "moz"},
                 {isDetails: true, uri: "http://mail.foo.com/yiihah",
                  title: "moz now updated"},
                 {isDetails: true, uri: "ftp://foo.com/ftp", title: "gone"}];
  await task_populateDB(change3);
  do_check_true(isInResult({uri: "http://foo.com/changeme2.htm"}, root));
  do_check_true(isInResult({uri: "http://mail.foo.com/yiihah"}, root));
  do_check_false(isInResult({uri: "ftp://foo.com/ftp"}, root));

  // And now, delete one
  do_print("Deleting items");
  var change4 = [{isDetails: true, uri: "https://foo.com/",
                  title: "mo,z"}];
  await task_populateDB(change4);
  do_check_false(isInResult(change4, root));

  root.containerOpen = false;
});
