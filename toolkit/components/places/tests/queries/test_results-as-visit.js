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
 * The Original Code is Places Test code.
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
var testData = [];
var now = Date.now() * 1000;
function createTestData() {
  function generateVisits(aPage) {
    for (var i = 0; i < aPage.visitCount; i++) {
      testData.push({ isInQuery: aPage.inQuery,
                      isVisit: true,
                      title: aPage.title,
                      uri: aPage.uri,
                      lastVisit: now++,
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
 function run_test() {
   createTestData();
   populateDB(testData);
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

   LOG("Number of items in result set: " + root.childCount);
   for(var i=0; i < root.childCount; ++i) {
     LOG("result: " + root.getChild(i).uri + " Title: " + root.getChild(i).title);
   }

   // Check our inital result set
   compareArrayToResult(testData, root);

   // If that passes, check liveupdate
   // Add to the query set
   LOG("Adding item to query")
   var tmp = [];
   for (var i=0; i < 2; i++) {
     tmp.push({ isVisit: true,
                uri: "http://foo.com/added.html",
                title: "ab moz" });
   }
   populateDB(tmp);
   for (var i=0; i < 2; i++)
     do_check_eq(root.getChild(i).title, "ab moz");

   // Update an existing URI
   LOG("Updating Item");
   var change2 = [{ isVisit: true,
                    title: "moz",
                    uri: "http://foo.mail.com/changeme2.html" }];
   populateDB(change2);
   do_check_true(isInResult(change2, root));

   // Update some in batch mode - add one and take one out of query set,
   // and simply change one so that it still applies to the query
   LOG("Updating Items in batch");
   var updateBatch = {
     runBatched: function (aUserData) {
       var batchchange = [{ isVisit: true,
                            lastVisit: now++,
                            uri: "http://foo.mail.com/changeme1.html",
                            title: "foo"},
                          { isVisit: true,
                            lastVisit: now++,
                            uri: "http://foo.mail.com/changeme3.html",
                            title: "moz",
                            isTag: true,
                            tagArray: ["foo", "moz"] }];
       populateDB(batchchange);
     }
   };
   PlacesUtils.history.runInBatchMode(updateBatch, null);
   do_check_false(isInResult({uri: "http://foo.mail.com/changeme1.html"}, root));
   do_check_true(isInResult({uri: "http://foo.mail.com/changeme3.html"}, root));

   // And now, delete one
   LOG("Delete item outside of batch");
   var change4 = [{ isVisit: true,
                    lastVisit: now++,
                    uri: "http://moilla.com/",
                    title: "mo,z" }];
   populateDB(change4);
   do_check_false(isInResult(change4, root));
}
