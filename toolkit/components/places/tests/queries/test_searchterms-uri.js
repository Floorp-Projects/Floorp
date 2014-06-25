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

    // Test subdomain included with isRedirect=true, different transtype
    {isInQuery: true, isVisit: true, isDetails: true, title: "amozzie",
     isRedirect: true, uri: "http://foo.com/redirect", lastVisit: old,
     referrer: "http://myreferrer.com", transType: PlacesUtils.history.TRANSITION_LINK},

    // Test www. style URI is included, with a tag
    {isInQuery: true, isVisit: true, isDetails: true, isTag: true,
     uri: "http://foo.com/yiihah", tagArray: ["moz"], lastVisit: yesterday,
     title: "foo"},

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

add_task(function test_searchterms_uri()
{
  yield task_populateDB(testData);
   var query = PlacesUtils.history.getNewQuery();
   query.searchTerms = "moz";
   query.uri = uri("http://foo.com");
   query.uriIsPrefix = true;

   // Options
   var options = PlacesUtils.history.getNewQueryOptions();
   options.sortingMode = options.SORT_BY_DATE_ASCENDING;
   options.resultType = options.RESULTS_AS_URI;
   
   // Results
   var result = PlacesUtils.history.executeQuery(query, options);
   var root = result.root;
   root.containerOpen = true;

   LOG("Number of items in result set: " + root.childCount);
   for(var i=0; i < root.childCount; ++i) {
     LOG("result: " + root.getChild(i).uri + " Title: " + root.getChild(i).title);
   }

   // Check our inital result set
   compareArrayToResult(testData, root);

   // If that passes, check liveupdate
   // Add to the query set
   LOG("Adding item to query")
   var change1 = [{isVisit: true, isDetails: true, uri: "http://foo.com/added.htm",
                   title: "moz", transType: PlacesUtils.history.TRANSITION_LINK}];
   yield task_populateDB(change1);
   do_check_true(isInResult(change1, root));

   // Update an existing URI
   LOG("Updating Item");
   var change2 = [{isDetails: true, uri: "http://foo.com/changeme1.htm",
                   title: "moz" }];
   yield task_populateDB(change2);
   do_check_true(isInResult(change2, root));

   // Add one and take one out of query set, and simply change one so that it
   // still applies to the query.
   LOG("Updating More Items");
   var change3 = [{isDetails: true, uri:"http://foo.com/changeme2.htm",
                   title: "moz"},
                  {isDetails: true, uri: "http://foo.com/yiihah",
                   title: "moz now updated"},
                  {isDetails: true, uri: "http://foo.com/redirect",
                   title: "gone"}];
   yield task_populateDB(change3);
   do_check_true(isInResult({uri: "http://foo.com/changeme2.htm"}, root));
   do_check_true(isInResult({uri: "http://foo.com/yiihah"}, root));
   do_check_false(isInResult({uri: "http://foo.com/redirect"}, root));

   // And now, delete one
   LOG("Deleting items");
   var change4 = [{isDetails: true, uri: "http://foo.com/",
                   title: "mo,z"}];
   yield task_populateDB(change4);
   do_check_false(isInResult(change4, root));

   root.containerOpen = false;
});
