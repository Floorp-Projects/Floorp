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
 *   Marco Bonardo <mak77@supereva.it> (Original Author)
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

function add_visit(aURI, aType) {
  PlacesUtils.history.addVisit(uri(aURI), Date.now() * 1000, null, aType,
                                 false, 0);
}

function run_test() {
  let count_visited_URIs = ["http://www.test-link.com/",
                            "http://www.test-typed.com/",
                            "http://www.test-bookmark.com/",
                            "http://www.test-redirect-permanent.com/",
                            "http://www.test-redirect-temporary.com/"];

  let notcount_visited_URIs = ["http://www.test-embed.com/",
                               "http://www.test-download.com/",
                               "http://www.test-framed.com/"];

  // add visits, one for each transition type
  add_visit("http://www.test-link.com/", TRANSITION_LINK);
  add_visit("http://www.test-typed.com/", TRANSITION_TYPED);
  add_visit("http://www.test-bookmark.com/", TRANSITION_BOOKMARK);
  add_visit("http://www.test-embed.com/", TRANSITION_EMBED);
  add_visit("http://www.test-framed.com/", TRANSITION_FRAMED_LINK);
  add_visit("http://www.test-redirect-permanent.com/", TRANSITION_REDIRECT_PERMANENT);
  add_visit("http://www.test-redirect-temporary.com/", TRANSITION_REDIRECT_TEMPORARY);
  add_visit("http://www.test-download.com/", TRANSITION_DOWNLOAD);

  // check that all links are marked as visited
  count_visited_URIs.forEach(function (visited_uri) {
    do_check_eq(PlacesUtils.bhistory.isVisited(uri(visited_uri)), true);
  });
  notcount_visited_URIs.forEach(function (visited_uri) {
    do_check_eq(PlacesUtils.bhistory.isVisited(uri(visited_uri)), true);
  });

  // check that visit_count does not take in count embed and downloads
  // maxVisits query are directly binded to visit_count
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = options.SORT_BY_VISITCOUNT_DESCENDING;
  options.resultType = options.RESULTS_AS_VISIT;
  options.includeHidden = true;
  let query = PlacesUtils.history.getNewQuery();
  query.minVisits = 1;
  let root = PlacesUtils.history.executeQuery(query, options).root;

  root.containerOpen = true;
  let cc = root.childCount;
  do_check_eq(cc, count_visited_URIs.length);

  for (let i = 0; i < cc; i++) {
    let node = root.getChild(i);
    do_check_neq(count_visited_URIs.indexOf(node.uri), -1);
  }
  root.containerOpen = false;
}
