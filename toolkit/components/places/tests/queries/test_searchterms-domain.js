/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places Test Code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Clint Talbert <ctalbert@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
   {isInQuery: false, isVisit:true, isDetails: true, title: "m o z",
    uri: "http://foo.com/tooearly.php", lastVisit: today},

   // Test bad URI
   {isInQuery: false, isVisit:true, isDetails: true, title: "moz",
    uri: "http://sffoo.com/justwrong.htm", lastVisit: tomorrow},

   // Test what we do with escaping in titles
   {isInQuery: false, isVisit:true, isDetails: true, title: "m%0o%0z",
    uri: "http://foo.com/changeme1.htm", lastVisit: yesterday},

   // Test another invalid title - for updating later
   {isInQuery: false, isVisit:true, isDetails: true, title: "m,oz",
    uri: "http://foo.com/changeme2.htm", lastVisit: tomorrow}];

/**
 * This test will test Queries that use relative search terms and domain options
 */
function run_test() {
  populateDB(testData);
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
  populateDB(change1);
  do_check_true(isInResult(change1, root));

  // Update an existing URI
  LOG("Updating Item");
  var change2 = [{isDetails: true, uri: "http://foo.com/changeme1.htm",
                  title: "moz" }];
  populateDB(change2);
  do_check_true(isInResult(change2, root));
                  
  // Update some in batch mode - add one and take one out of query set,
  // and simply change one so that it still applies to the query
  LOG("Updating Items in batch");
  var updateBatch = {
    runBatched: function (aUserData) {
      var batchchange = [{isDetails: true, uri:"http://foo.com/changeme2.htm",
                          title: "moz"},
                         {isDetails: true, uri: "http://mail.foo.com/yiihah",
                          title: "moz now updated"},
                         {isDetails: true, uri: "ftp://foo.com/ftp", title: "gone"}];
      populateDB(batchchange);
    }
  };
  PlacesUtils.history.runInBatchMode(updateBatch, null);
  do_check_true(isInResult({uri: "http://foo.com/changeme2.htm"}, root));
  do_check_true(isInResult({uri: "http://mail.foo.com/yiihah"}, root));
  do_check_false(isInResult({uri: "ftp://foo.com/ftp"}, root));

  // And now, delete one
  LOG("Delete item outside of batch");
  var change4 = [{isDetails: true, uri: "https://foo.com/",
                  title: "mo,z"}];
  populateDB(change4);
  do_check_false(isInResult(change4, root));

  root.containerOpen = false;
}
