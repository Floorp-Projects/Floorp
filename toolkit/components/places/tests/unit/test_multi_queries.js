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
function add_visit(aURI, aDayOffset, aEmbedded) {
  var placeID = histsvc.addVisit(aURI,
                                 (Date.now() + aDayOffset*86400000) * 1000,
                                 null,
                                 aEmbedded?histsvc.TRANSITION_EMBED
                                          :histsvc.TRANSITION_TYPED,
                                 false, // not redirect
                                 0);
  do_check_true(placeID > 0);
  return placeID;
}

// main
function run_test() {

  var testURI = uri("http://mirror1.mozilla.com/a");
  add_visit(testURI, -1);
  testURI = uri("http://mirror2.mozilla.com/b");
  add_visit(testURI, -2);
  testURI = uri("http://mirror1.google.com/b");
  add_visit(testURI, -1, true);
  testURI = uri("http://mirror2.google.com/a");
  add_visit(testURI, -2);
  testURI = uri("http://mirror1.apache.org/b");
  add_visit(testURI, -3);
  testURI = uri("http://mirror2.apache.org/a");
  add_visit(testURI, -4, true);

  var options = histsvc.getNewQueryOptions();
  var queries = [];
  queries.push(histsvc.getNewQuery());
  queries.push(histsvc.getNewQuery());

  queries[0].domain = "mozilla.com";
  queries[1].domain = "google.com";

  var result = histsvc.executeQueries(queries, queries.length, options);
  var root = result.root;
  root.containerOpen = true;
  do_check_eq(root.childCount, 3);
  root.containerOpen = false;
}
