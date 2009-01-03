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
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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
 * This test checks that in a basic history query all transition types visits
 * appear but TRANSITION_EMBED ones.
 */

var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
         getService(Ci.nsINavHistoryService);

// adds a test URI visit to the database, and checks for a valid visitId
function add_visit(aURI, aType) {
  var visitId = hs.addVisit(uri(aURI),
                            Date.now() * 1000,
                            null, // no referrer
                            aType,
                            false, // not redirect
                            0);
  do_check_true(visitId > 0);
  return visitId;
}

var  transitions = [ hs.TRANSITION_LINK,
                     hs.TRANSITION_TYPED,
                     hs.TRANSITION_BOOKMARK,
                     hs.TRANSITION_EMBED,
                     hs.TRANSITION_REDIRECT_PERMANENT,
                     hs.TRANSITION_REDIRECT_TEMPORARY,
                     hs.TRANSITION_DOWNLOAD ];

function runQuery(aResultType) {
  var options = hs.getNewQueryOptions();
  options.resultType = aResultType;
  var query = hs.getNewQuery();
  var result = hs.executeQuery(query, options);
  var root = result.root;

  root.containerOpen = true;
  var cc = root.childCount;
  do_check_eq(cc, transitions.length-1);

  for (var i = 0; i < cc; i++) {
    var node = root.getChild(i);
    // Check that all transition types but TRANSITION_EMBED appear in results
    do_check_neq(node.uri.substr(6,1), hs.TRANSITION_EMBED);
  }
  root.containerOpen = false;
}

// main
function run_test() {
  // add visits, one for each transition type
  transitions.forEach(
    function(transition) {
      add_visit("http://" + transition +".mozilla.org/", transition)
    });

  runQuery(Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT);
  runQuery(Ci.nsINavHistoryQueryOptions.RESULTS_AS_URI);
}
