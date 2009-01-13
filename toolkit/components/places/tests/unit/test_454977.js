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

// Get services
try {
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);
  var mDBConn = hs.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
} catch(ex) {
  do_throw("Could not get services\n");
}

// Cache actual visit_count value, filled by add_visit, used by check_results
var visit_count = 0;

function add_visit(aURI, aVisitDate, aVisitType) {
  var isRedirect = aVisitType == hs.TRANSITION_REDIRECT_PERMANENT ||
                   aVisitType == hs.TRANSITION_REDIRECT_TEMPORARY;
  var visitId = hs.addVisit(aURI, aVisitDate, null,
                            aVisitType, isRedirect, 0);
  do_check_true(visitId > 0);
  // Increase visit_count if applicable
  if (aVisitType != 0 &&
      aVisitType != hs.TRANSITION_EMBED &&
      aVisitType != hs.TRANSITION_DOWNLOAD)
    visit_count ++;
  // Get the place id
  var sql = "SELECT place_id FROM moz_historyvisits_view WHERE id = ?1";
  var stmt = mDBConn.createStatement(sql);
  stmt.bindInt64Parameter(0, visitId);
  do_check_true(stmt.executeStep());
  var placeId = stmt.getInt64(0);
  do_check_true(placeId > 0);
  return placeId;
}

/**
 * Checks for results consistency, using visit_count as constraint
 * @param   aExpectedCount
 *          Number of history results we are expecting (excluded hidden ones)
 * @param   aExpectedCountWithHidden
 *          Number of history results we are expecting (included hidden ones)
 */
function check_results(aExpectedCount, aExpectedCountWithHidden) {
  var query = hs.getNewQuery();
  // used to check visit_count
  query.minVisits = visit_count;
  query.maxVisits = visit_count;
  var options = hs.getNewQueryOptions();
  options.queryType = Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY;
  result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  // Children without hidden ones
  do_check_eq(root.childCount, aExpectedCount);
  root.containerOpen = false;

  // Execute again with includeHidden = true
  // This will ensure visit_count is correct
  options.includeHidden = true;
  result = hs.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  // Children with hidden ones
  do_check_eq(root.childCount, aExpectedCountWithHidden);
  root.containerOpen = false;
}

// main
function run_test() {
  var testURI = uri("http://test.mozilla.org/");

  // Add a visit that force hidden
  var placeId = add_visit(testURI, Date.now()*1000, hs.TRANSITION_EMBED);
  check_results(0, 1);

  // Add a visit that force unhide and check place id
  // - We expect that the place gets hidden = 0 while retaining the same
  //   place_id and a correct visit_count.
  do_check_eq(add_visit(testURI, Date.now()*1000, hs.TRANSITION_TYPED), placeId);
  check_results(1, 1);

  // Add a visit, check that hidden is not overwritten and check place id
  // - We expect that the place has still hidden = 0, while retaining the same
  //   place_id and a correct visit_count.
  do_check_eq(add_visit(testURI, Date.now()*1000, hs.TRANSITION_EMBED), placeId);
  check_results(1, 1);
}
