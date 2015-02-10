/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
  * Test bookmarksService.getBookmarkedURIFor(aURI);
  */

let hs = PlacesUtils.history;
let bs = PlacesUtils.bookmarks;

function run_test() {
  run_next_test();
}

add_task(function test_getBookmarkedURIFor() {
  let now = Date.now() * 1000;
  const sourceURI = uri("http://test.mozilla.org/");
  // Add a visit and a bookmark.
  yield PlacesTestUtils.addVisits({ uri: sourceURI, visitDate: now });
  do_check_eq(bs.getBookmarkedURIFor(sourceURI), null);

  let sourceItemId = bs.insertBookmark(bs.unfiledBookmarksFolder,
                                       sourceURI,
                                       bs.DEFAULT_INDEX,
                                       "bookmark");
  do_check_true(bs.getBookmarkedURIFor(sourceURI).equals(sourceURI));

  // Add a redirected visit.
  const permaURI = uri("http://perma.mozilla.org/");
  yield PlacesTestUtils.addVisits({
    uri: permaURI,
    transition: TRANSITION_REDIRECT_PERMANENT,
    visitDate: now++,
    referrer: sourceURI
  });
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
  yield PlacesTestUtils.addVisits({
    uri: tempURI,
    transition: TRANSITION_REDIRECT_TEMPORARY,
    visitDate: now++,
    referrer: permaURI
  });

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
});
