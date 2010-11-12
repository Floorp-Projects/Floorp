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

/**
 * Adds a test URI visit to history.
 *
 * @param aURI
 *        The URI to add a visit for.
 * @param aReferrer
 *        The referring URI for the given URI.  This can be null.
 */
function add_visit(aURI, aDayOffset, aTransition) {
  PlacesUtils.history.addVisit(aURI, (Date.now() + aDayOffset*86400000) * 1000,
                               null, aTransition, false, 0);
}

function run_test() {
  add_visit(uri("http://mirror1.mozilla.com/a"), -1, TRANSITION_LINK);
  add_visit(uri("http://mirror2.mozilla.com/b"), -2, TRANSITION_LINK);
  add_visit(uri("http://mirror3.mozilla.com/c"), -4, TRANSITION_FRAMED_LINK);
  add_visit(uri("http://mirror1.google.com/b"), -1, TRANSITION_EMBED);
  add_visit(uri("http://mirror2.google.com/a"), -2, TRANSITION_LINK);
  add_visit(uri("http://mirror1.apache.org/b"), -3, TRANSITION_LINK);
  add_visit(uri("http://mirror2.apache.org/a"), -4, TRANSITION_FRAMED_LINK);

  let queries = [
    PlacesUtils.history.getNewQuery(),
    PlacesUtils.history.getNewQuery()
  ];
  queries[0].domain = "mozilla.com";
  queries[1].domain = "google.com";

  let root = PlacesUtils.history.executeQueries(
    queries, queries.length, PlacesUtils.history.getNewQueryOptions()
  ).root;
  root.containerOpen = true;
  let childCount = root.childCount;
  root.containerOpen = false;

  do_check_eq(childCount, 3);
}
