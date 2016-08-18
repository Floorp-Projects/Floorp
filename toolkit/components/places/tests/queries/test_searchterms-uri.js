/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

  // The test data for our database, note that the ordering of the results that
  // will be returned by the query (the isInQuery: true objects) is IMPORTANT.
  // see compareArrayToResult in head_queries.js for more info.
  var testData = [
    // Test flat domain with annotation, search term in sentence
    {isInQuery: true, isVisit: true, isDetails: true, isPageAnnotation: true,
     uri: "http://foo.com/", annoName: "moz/test", annoVal: "val",
     lastVisit: lastweek, title: "you know, moz is cool"},

    // Test https protocol
    {isInQuery: false, isVisit: true, isDetails: true, title: "moz",
     uri: "https://foo.com/", lastVisit: today},

    // Begin the invalid queries: wrong search term
    {isInQuery: false, isVisit:true, isDetails: true, title: "m o z",
     uri: "http://foo.com/wrongsearch.php", lastVisit: today},

     // Test subdomain inclued, search term at end
     {isInQuery: false, isVisit: true, isDetails: true,
      uri: "http://mail.foo.com/yiihah", title: "blahmoz", lastVisit: daybefore},

     // Test ftp protocol - vary the title length, embed search term
     {isInQuery: false, isVisit: true, isDetails: true,
      uri: "ftp://foo.com/ftp", lastVisit: lastweek,
      title: "hugelongconfmozlagurationofwordswithasearchtermsinit whoo-hoo"},

    // Test what we do with escaping in titles
    {isInQuery: false, isVisit:true, isDetails: true, title: "m%0o%0z",
     uri: "http://foo.com/changeme1.htm", lastVisit: yesterday},

    // Test another invalid title - for updating later
    {isInQuery: false, isVisit:true, isDetails: true, title: "m,oz",
     uri: "http://foo.com/changeme2.htm", lastVisit: tomorrow}];

/**
 * This test will test Queries that use relative search terms and URI options
 */
function run_test()
{
  run_next_test();
}

add_task(function* test_searchterms_uri()
{
  yield task_populateDB(testData);
   var query = PlacesUtils.history.getNewQuery();
   query.searchTerms = "moz";
   query.uri = uri("http://foo.com");

   // Options
   var options = PlacesUtils.history.getNewQueryOptions();
   options.sortingMode = options.SORT_BY_DATE_ASCENDING;
   options.resultType = options.RESULTS_AS_URI;

   // Results
   var result = PlacesUtils.history.executeQuery(query, options);
   var root = result.root;
   root.containerOpen = true;

   do_print("Number of items in result set: " + root.childCount);
   for (var i=0; i < root.childCount; ++i) {
     do_print("result: " + root.getChild(i).uri + " Title: " + root.getChild(i).title);
   }

   // Check our inital result set
   compareArrayToResult(testData, root);

   // live update.
   do_print("change title");
   var change1 = [{isDetails: true, uri:"http://foo.com/",
                   title: "mo"}, ];
   yield task_populateDB(change1);

   do_check_false(isInResult({uri: "http://foo.com/"}, root));
   var change2 = [{isDetails: true, uri:"http://foo.com/",
                   title: "moz"}, ];
   yield task_populateDB(change2);
   do_check_true(isInResult({uri: "http://foo.com/"}, root));

   root.containerOpen = false;
});
