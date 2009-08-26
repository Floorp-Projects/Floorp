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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Clint Talbert <ctalbert@mozilla.com>
 *  Marco Bonardo <mak77@bonardo.net>
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
    tagArray: ["moz","bugzilla"] },

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
  var query = histsvc.getNewQuery();
  query.setFolders([bmsvc.tagsFolder], 1);
  var options = histsvc.getNewQueryOptions();
  var root = histsvc.executeQuery(query, options).root;
  root.containerOpen = true;  
  var cc = root.childCount;
  do_check_eq(root.childCount, 2);
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
function run_test() {
  populateDB(testData);

  // Get tag id.
  let tagId = getIdForTag("bugzilla");
  do_check_true(tagId > 0);

  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_TAG_CONTENTS;
  var query = histsvc.getNewQuery();
  query.setFolders([tagId], 1);

  var root = histsvc.executeQuery(query, options).root;
  root.containerOpen = true;

  displayResultSet(root);
  // Cannot use compare array to results, since results ordering is hardcoded
  // and depending on lastModified (that could have VM timers issues).
  testData.forEach(function(aEntry) {
    if (aEntry.isInResult)
      do_check_true(isInResult({uri: "http://foo.com/added.html"}, root));
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
  LOG("Adding item to query")
  populateDB([change1]);
  LOG("These results should have been LIVE UPDATED with the new addition");
  displayResultSet(root);
  do_check_true(isInResult(change1, root));

  // Update some in batch mode - add one by adding a tag, remove one by removing
  // search term.
  LOG("Updating Items in batch");
  var updateBatch = {
    runBatched: function (aUserData) {
      batchchange = [{ isDetails: true,
                       uri: "http://foo3.com/",
                       title: "foo"},
                     { isDetails: true,
                       uri: "http://foo.com/changeme2.html",
                       title: "zydeco",
                       isBookmark:true,
                       isTag: true,
                       tagArray: ["bugzilla", "moz"] }];
      populateDB(batchchange);
    }
  };
  histsvc.runInBatchMode(updateBatch, null);
  do_check_false(isInResult({uri: "http://fooz.com/"}, root));
  do_check_true(isInResult({uri: "http://foo.com/changeme2.html"}, root));

  // Test removing a tag updates us.
  LOG("Delete item outside of batch");
  tagssvc.untagURI(uri("http://foo.com/changeme2.html"), ["bugzilla"]);
  do_check_false(isInResult({uri: "http://foo.com/changeme2.html"}, root));

  root.containerOpen = false;
}
