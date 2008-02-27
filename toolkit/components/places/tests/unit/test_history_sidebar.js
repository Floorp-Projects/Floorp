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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Ondrej Brablc <ondrej@allpeers.com>
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

// Get history service
try {
  var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].getService(Ci.nsINavHistoryService);
} catch(ex) {
  do_throw("Could not get history service\n");
} 

/**
 * Adds a test URI visit to the database, and checks for a valid place ID.
 *
 * @param aURI
 *        The URI to add a visit for.
 * @param aReferrer
 *        The referring URI for the given URI.  This can be null.
 * @returns the place id for aURI.
 */
function add_visit(aURI, aDayOffset) {
  var placeID = histsvc.addVisit(aURI,
                                 (Date.now() + aDayOffset*86400000) * 1000,
                                 null,
                                 histsvc.TRANSITION_TYPED, // user typed in URL bar
                                 false, // not redirect
                                 0);
  do_check_true(placeID > 0);
  return placeID;
}

// Can I rely on en-US locales during units tests?
var dayLabels = 
[ 
  "Today", 
  "Yesterday", 
  "2 days ago", 
  "3 days ago",
  "4 days ago",
  "5 days ago",
  "6 days ago",
  "Older than 6 days"
];

// Fills history and checks if date labels are correct for partially filled history
function fill_history() {
  const checkOlderOffset = 4;

  // add visits for the older days
  for (var i=checkOlderOffset; i<dayLabels.length; i++)
  {
    var testURI = uri("http://mirror"+i+".mozilla.com/b");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".mozilla.com/a");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".google.com/b");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".google.com/a");
    add_visit(testURI, -i);
  }

  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, dayLabels.length - checkOlderOffset);

  for (var i=checkOlderOffset; i<dayLabels.length; i++)
  {
    var node = root.getChild(i-checkOlderOffset);
    do_check_eq(node.title, dayLabels[i]);
  }


  // When I close the root container here, it would generate a warning
  // on next call to addVisit.
  root.containerOpen = false;
  
  // add visits for the most recent days
  for (var i=0; i<checkOlderOffset; i++)
  {
    var testURI = uri("http://mirror"+i+".mozilla.com/d");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".mozilla.com/c");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".google.com/d");
    add_visit(testURI, -i);
    var testURI = uri("http://mirror"+i+".google.com/c");
    add_visit(testURI, -i);
  }

  root.containerOpen = false;
}

function test_RESULTS_AS_DATE_SITE_QUERY() {

  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, dayLabels.length);

  // Now we check whether we have all the labels
  for (var i=0; i<dayLabels.length; i++)
  {
    var node = root.getChild(i);
    do_check_eq(node.title, dayLabels[i]);
  }

  // Check one of the days
  var dayNode = root.getChild(4).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  do_check_eq(dayNode.childCount, 2);

  // Items should be sorted by host
  var site1 = dayNode.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(site1.title, "mirror4.google.com");

  var site2 = dayNode.getChild(1).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(site2.title, "mirror4.mozilla.com");

  site1.containerOpen = true;
  do_check_eq(site1.childCount, 2);

  // Inside of host sites are sorted by title
  var site1visit = site1.getChild(0);
  do_check_eq(site1visit.uri, "http://mirror4.google.com/a");

  site1.containerOpen = false;
  dayNode.containerOpen = false;
  root.containerOpen = false;

}

function test_RESULTS_AS_DATE_QUERY() {

  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, dayLabels.length);

  // Now we check whether we have all the labels
  for (var i=0; i<dayLabels.length; i++)
  {
    var node = root.getChild(i);
    do_check_eq(node.title, dayLabels[i]);
  }

  // Check one of the days
  var dayNode = root.getChild(3).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayNode.containerOpen = true;
  do_check_eq(dayNode.childCount, 4);
  do_check_eq(dayNode.title, "3 days ago");

  // Items should be sorted title
  var visit1 = dayNode.getChild(0);
  do_check_eq(visit1.uri, "http://mirror3.google.com/c");

  var visit2 = dayNode.getChild(3);
  do_check_eq(visit2.uri, "http://mirror3.mozilla.com/d");

  dayNode.containerOpen = false;
  root.containerOpen = false;
}

function test_RESULTS_AS_SITE_QUERY() {

  var options = histsvc.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_SITE_QUERY;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, dayLabels.length*2);

  // We include this here, se that maintainer knows what is the expected result
  var expectedResult = 
  [
    "mirror0.google.com",
    "mirror0.mozilla.com",
    "mirror1.google.com",
    "mirror1.mozilla.com",
    "mirror2.google.com",
    "mirror2.mozilla.com",
    "mirror3.google.com",
    "mirror3.mozilla.com",
    "mirror4.google.com",
    "mirror4.mozilla.com",
    "mirror5.google.com",   // We check for this site
    "mirror5.mozilla.com",
    "mirror6.google.com",
    "mirror6.mozilla.com",
    "mirror7.google.com",
    "mirror7.mozilla.com"
  ];

  // Items should be sorted by host
  var siteNode = root.getChild(dayLabels.length+2).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(siteNode.title,  expectedResult[dayLabels.length+2] );

  siteNode.containerOpen = true;
  do_check_eq(siteNode.childCount, 2);

  // Inside of host sites are sorted by title
  var visit = siteNode.getChild(0);
  do_check_eq(visit.uri, "http://mirror5.google.com/a");

  siteNode.containerOpen = false;
  root.containerOpen = false;
}

// main
function run_test() {

  fill_history();
  test_RESULTS_AS_DATE_SITE_QUERY();
  test_RESULTS_AS_DATE_QUERY();
  test_RESULTS_AS_SITE_QUERY();

  // The remaining views are
  //   RESULTS_AS_URI + SORT_BY_VISITCOUNT_DESCENDING 
  //   ->  test_399266.js
  //   RESULTS_AS_URI + SORT_BY_DATE_DESCENDING
  //   ->  test_385397.js
}
