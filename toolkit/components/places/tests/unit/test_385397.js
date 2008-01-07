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
 *  Darin Fisher <darin@meer.net>
 *  Dietrich Ayala <dietrich@mozilla.com>
 *  Dan Mills <thunder@mozilla.com>
 *  Seth Spitzer <sspitzer@mozilla.com>
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

// adds a test URI visit to the database, and checks for a valid place ID
function add_visit(aURI, aWhen, aType) {
  var placeID = histsvc.addVisit(aURI,
                                 aWhen,
                                 null, // no referrer
                                 aType,
                                 false, // not redirect
                                 0);
  do_check_true(placeID > 0);
  return placeID;
}

const TOTAL_SITES = 20;

// main
function run_test() {
  var now = Date.now();
  // add visits
  for (var i=0; i < TOTAL_SITES; i++) {
    var site = "http://www.test-" + i + ".com/";
    var testURI = uri(site);
    var testImageURI = uri(site + "blank.gif");
    var when = now + (i * TOTAL_SITES);
    add_visit(testURI, when, histsvc.TRANSITION_TYPED);
    add_visit(testImageURI, when + 1, histsvc.TRANSITION_EMBED);
    add_visit(testURI, when + 2, histsvc.TRANSITION_LINK);
  }

  // verify our visits AS_VISIT, ordered by date descending
  // including hidden
  // we should get 60 visits:
  // http://www.test-19.com/
  // http://www.test-19.com/blank.gif
  // http://www.test-19.com/
  // ...
  // http://www.test-0.com/
  // http://www.test-0.com/blank.gif
  // http://www.test-0.com/
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, 3 * TOTAL_SITES); 
  for (var i=0; i < TOTAL_SITES; i++) {
    var node = root.getChild(i*3);
    var site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
    node = root.getChild(i*3+1);
    do_check_eq(node.uri, site + "blank.gif");
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
    node = root.getChild(i*3+2);
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
  }
  root.containerOpen = false;

  // verify our visits AS_VISIT, ordered by date descending
  // we should get 40 visits:
  // http://www.test-19.com/
  // http://www.test-19.com/
  // ...
  // http://www.test-0.com/
  // http://www.test-0.com/
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  // 2 * TOTAL_SITES because we count the TYPED and LINK, but not EMBED
  do_check_eq(cc, 2 * TOTAL_SITES); 
  for (var i=0; i < TOTAL_SITES; i++) {
    var node = root.getChild(i*2);
    var site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
    node = root.getChild(i*2+1);
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_VISIT);
  }
  root.containerOpen = false;

  // test our optimized query for the places menu
  // place:type=0&sort=4&maxResults=10
  // verify our visits AS_URI, ordered by date descending
  // we should get 10 visits:
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.maxResults = 10;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, options.maxResults);
  for (var i=0; i < cc; i++) {
    var node = root.getChild(i);
    var site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_URI);
  }
  root.containerOpen = false;

  // test without a maxResults, which executes a different query
  // but the first 10 results should be the same.
  // verify our visits AS_URI, ordered by date descending
  // we should get 20 visits, but the first 10 should be
  // http://www.test-19.com/
  // ...
  // http://www.test-10.com/
  var options = histsvc.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_URI;
  var query = histsvc.getNewQuery();
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, TOTAL_SITES);
  for (var i=0; i < 10; i++) {
    var node = root.getChild(i);
    var site = "http://www.test-" + (TOTAL_SITES - 1 - i) + ".com/";
    do_check_eq(node.uri, site);
    do_check_eq(node.type, options.RESULTS_AS_URI);
  }
  root.containerOpen = false;
}
