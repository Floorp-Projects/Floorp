/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
