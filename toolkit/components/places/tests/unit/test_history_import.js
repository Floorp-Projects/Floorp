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
 * The Original Code is Bug 454977 code.
 *
 * The Initial Developer of the Original Code is Mozilla Corp.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77bonardo.net> (Original Author)
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

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);
var gh = hs.QueryInterface(Ci.nsIGlobalHistory2);

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @returns true if the URI is in the DB, false otherwise.
 */
function uri_in_db(aURI) {
  var options = hs.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI
  var query = hs.getNewQuery();
  query.uri = aURI;
  var result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  return (root.childCount == 1);
}

// main
function run_test() {
  // test import history
  var file = do_get_file("toolkit/components/places/tests/unit/history_import_test.dat");
  gh.importHistory(file);
  var uri1 = uri("http://www.mozilla.org/");
  do_check_true(uri_in_db(uri1));

  // Check visit count
  var options = hs.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_DATE_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  var query = hs.getNewQuery();
  query.minVisits = 4;
  query.maxVisits = 4;
  query.uri = uri1;
  var result = hs.executeQuery(query, options);
  var root = result.root;

  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, 4);

  // Check first and last visits times are correct
  var lastVisitDate = root.getChild(0).time;
  do_check_eq(lastVisitDate, 1230666859910484);
  var firstVisitDate = root.getChild(3).time;
  do_check_eq(firstVisitDate, 1230666845910353);

  // Check other visits have different times and are between first and last ones
  do_check_true(root.getChild(1).time < lastVisitDate &&
                root.getChild(1).time > firstVisitDate);
  do_check_true(root.getChild(2).time < lastVisitDate &&
                root.getChild(2).time > firstVisitDate);
  do_check_true(root.getChild(1).time != root.getChild(2).time);

  root.containerOpen = false;
}
