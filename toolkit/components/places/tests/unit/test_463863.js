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
 * appear but TRANSITION_EMBED and TRANSITION_FRAMED_LINK ones.
 */

function add_visit(aURI, aType) {
  PlacesUtils.history.addVisit(uri(aURI), Date.now() * 1000, null, aType,
                            false, 0);
}

let transitions = [
  TRANSITION_LINK
, TRANSITION_TYPED
, TRANSITION_BOOKMARK
, TRANSITION_EMBED
, TRANSITION_FRAMED_LINK
, TRANSITION_REDIRECT_PERMANENT
, TRANSITION_REDIRECT_TEMPORARY
, TRANSITION_DOWNLOAD
];

function runQuery(aResultType) {
  let options = PlacesUtils.history.getNewQueryOptions();
  options.resultType = aResultType;
  let root = PlacesUtils.history.executeQuery(PlacesUtils.history.getNewQuery(),
                                              options).root;
  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, transitions.length - 2);

  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    // Check that all transition types but EMBED and FRAMED appear in results
    do_check_neq(node.uri.substr(6,1), TRANSITION_EMBED);
    do_check_neq(node.uri.substr(6,1), TRANSITION_FRAMED_LINK);
  }
  root.containerOpen = false;
}

function run_test() {
  // add visits, one for each transition type
  transitions.forEach(function(transition) {
    add_visit("http://" + transition +".mozilla.org/", transition);
  });

  runQuery(Ci.nsINavHistoryQueryOptions.RESULTS_AS_VISIT);
  runQuery(Ci.nsINavHistoryQueryOptions.RESULTS_AS_URI);
}
