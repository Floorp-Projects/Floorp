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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

 /**
  * Test bookmarksService.getBookmarkedURIFor(aURI);
  */

let hs = PlacesUtils.history;
let bs = PlacesUtils.bookmarks;

/**
 * Adds a fake redirect between two visits.
 */
function addFakeRedirect(aSourceVisitId, aDestVisitId, aRedirectType) {
  let stmt = DBConn().createStatement(
    "UPDATE moz_historyvisits " +
    "SET from_visit = :source, visit_type = :type " +
    "WHERE id = :dest");
  stmt.params.source = aSourceVisitId;
  stmt.params.dest = aDestVisitId;
  stmt.params.type = aRedirectType;
  try {
    stmt.executeStep();
  }
  finally {
    stmt.finalize();
  }
}

function run_test() {
  let now = Date.now() * 1000;
  const sourceURI = uri("http://test.mozilla.org/");
  // Add a visit and a bookmark.
  let sourceVisitId = hs.addVisit(sourceURI,
                                  now,
                                  null,
                                  hs.TRANSITION_TYPED,
                                  false,
                                  0);
  do_check_eq(bs.getBookmarkedURIFor(sourceURI), null);

  let sourceItemId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                       sourceURI,
                                       bs.DEFAULT_INDEX,
                                       "bookmark");
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));

  // Add a redirected visit.
  const permaURI = uri("http://perma.mozilla.org/");
  hs.addVisit(permaURI,
              now++,
              sourceURI,
              hs.TRANSITION_REDIRECT_PERMANENT,
              true,
              0);
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));
  do_check_true(bs.getBookmarkedURIFor(permaURI).equals(sourceURI));
  // Add a bookmark to the destination.
  let permaItemId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                      permaURI,
                                      bs.DEFAULT_INDEX,
                                      "bookmark");
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));
  do_check_true(bs.getBookmarkedURIFor(permaURI).equals(permaURI));
  // Now remove the bookmark on the destination.
  bs.removeItem(permaItemId);
  // We should see the source as bookmark.
  do_check_true(bs.getBookmarkedURIFor(permaURI).equals(sourceURI));

  // Add another redirected visit.
  const tempURI = uri("http://perma.mozilla.org/");
  hs.addVisit(tempURI,
              now++,
              permaURI,
              hs.TRANSITION_REDIRECT_TEMPORARY,
              true,
              0);
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));
  do_check_true(bs.getBookmarkedURIFor(tempURI).equals(sourceURI));
  // Add a bookmark to the destination.
  let tempItemId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                     tempURI,
                                     bs.DEFAULT_INDEX,
                                     "bookmark");
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));
  do_check_true(bs.getBookmarkedURIFor(tempURI).equals(tempURI));

  // Now remove the bookmark on the destination.
  bs.removeItem(tempItemId);
  // We should see the source as bookmark.
  do_check_true(bs.getBookmarkedURIFor(tempURI).equals(sourceURI));
  // Remove the source bookmark as well.
  bs.removeItem(sourceItemId);
  do_check_eq(bs.getBookmarkedURIFor(tempURI), null);

  // Try to pass in a never seen URI, should return null and a new entry should
  // not be added to the database.
  do_check_eq(bs.getBookmarkedURIFor(uri("http://does.not.exist/")), null);
  do_check_false(page_in_database("http://does.not.exist/"));
}
