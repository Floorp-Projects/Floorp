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
 * The Original Code is Places unit test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net> (Original Author)
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

/*
 * TEST DESCRIPTION:
 *
 * This test checks that setting a sort on a RESULTS_AS_DATE_QUERY query,
 * children of inside containers are sorted accordingly.
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

// adds a test URI visit to the database, and checks for a valid visitId
function add_visit(aURI, aTime) {
  var visitId = hs.addVisit(uri(aURI),
                            aTime * 1000,
                            null, // no referrer
                            hs.TRANSITION_TYPED,
                            false, // not redirect
                            0);
  do_check_true(visitId > 0);
  return visitId;
}

/**
 * Tests that sorting date query by none will sort by title asc.
 */
add_test(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_NONE;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;

  var cc = dayContainer.childCount;
  do_check_eq(cc, pages.length);
  for (var i = 0; i < cc; i++) {
    var node = dayContainer.getChild(i);
    do_check_eq(pages[i], node.uri);
  }

  dayContainer.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Tests that sorting date query by date will sort accordingly.
 */
add_test(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;

  var cc = dayContainer.childCount;
  do_check_eq(cc, pages.length);
  for (var i = 0; i < cc; i++) {
    var node = dayContainer.getChild(i);
    do_check_eq(pages[pages.length - i - 1], node.uri);
  }

  dayContainer.containerOpen = false;
  root.containerOpen = false;
});

/**
 * Tests that sorting date site query by date will still sort by title asc.
 */
add_test(function() {
  var options = hs.getNewQueryOptions();
  options.resultType = options.RESULTS_AS_DATE_SITE_QUERY;
  // This should sort by title asc.
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var dayContainer = root.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  dayContainer.containerOpen = true;
  var siteContainer = dayContainer.getChild(0).QueryInterface(Ci.nsINavHistoryContainerResultNode);
  do_check_eq(siteContainer.title, "a.mozilla.org");
  siteContainer.containerOpen = true;

  var cc = siteContainer.childCount;
  do_check_eq(cc, pages.length / 2);
  for (var i = 0; i < cc / 2; i++) {
    var node = siteContainer.getChild(i);
    do_check_eq(pages[i], node.uri);
  }

  siteContainer.containerOpen = false;
  dayContainer.containerOpen = false;
  root.containerOpen = false;
});

// Will be inserted in this order, so last one will be the newest visit.
var pages = [
  "http://a.mozilla.org/1/",
  "http://a.mozilla.org/2/",
  "http://a.mozilla.org/3/",
  "http://a.mozilla.org/4/",
  "http://b.mozilla.org/5/",
  "http://b.mozilla.org/6/",
  "http://b.mozilla.org/7/",
  "http://b.mozilla.org/8/",
];

// main
function run_test() {
  var noon = new Date();
  noon.setHours(12);

  // Add visits.
  pages.forEach(function(aPage) {
      add_visit(aPage, noon - (pages.length - pages.indexOf(aPage)) * 1000);
    });

  // Kick off tests.
  while (gTests.length)
    (gTests.shift())();
}
