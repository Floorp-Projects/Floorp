/* -*- tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Bug 1017502 - Add a foreign_count column to moz_places
This tests, tests the triggers that adjust the foreign_count when a bookmark is
added or removed and also the maintenance task to fix wrong counts.
*/

const T_URI = NetUtil.newURI("https://www.mozilla.org/firefox/nightly/firstrun/");

async function getForeignCountForURL(conn, url) {
  await PlacesTestUtils.promiseAsyncUpdates();
  url = url instanceof Ci.nsIURI ? url.spec : url;
  let rows = await conn.executeCached(
    `SELECT foreign_count FROM moz_places WHERE url_hash = hash(:t_url)
                                            AND url = :t_url`, { t_url: url });
  return rows[0].getResultByName("foreign_count");
}

function run_test() {
  run_next_test();
}

add_task(async function add_remove_change_bookmark_test() {
  let conn = await PlacesUtils.promiseDBConnection();

  // Simulate a visit to the url
  await PlacesTestUtils.addVisits(T_URI);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 0);

  // Add 1st bookmark which should increment foreign_count by 1
  let id1 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                    T_URI, PlacesUtils.bookmarks.DEFAULT_INDEX, "First Run");
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 1);

  // Add 2nd bookmark
  let id2 = PlacesUtils.bookmarks.insertBookmark(PlacesUtils.bookmarksMenuFolderId,
                      T_URI, PlacesUtils.bookmarks.DEFAULT_INDEX, "First Run");
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 2);

  // Remove 2nd bookmark which should decrement foreign_count by 1
  PlacesUtils.bookmarks.removeItem(id2);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 1);

  // Change first bookmark's URI
  const URI2 = NetUtil.newURI("http://www.mozilla.org");
  PlacesUtils.bookmarks.changeBookmarkURI(id1, URI2);
  // Check foreign count for original URI
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 0);
  // Check foreign count for new URI
  Assert.equal((await getForeignCountForURL(conn, URI2)), 1);

  // Cleanup - Remove changed bookmark
  let id = PlacesUtils.bookmarks.getBookmarkIdsForURI(URI2);
  PlacesUtils.bookmarks.removeItem(id);
  Assert.equal((await getForeignCountForURL(conn, URI2)), 0);

});

add_task(async function maintenance_foreign_count_test() {
  let conn = await PlacesUtils.promiseDBConnection();

  // Simulate a visit to the url
  await PlacesTestUtils.addVisits(T_URI);

  // Adjust the foreign_count for the added entry to an incorrect value
  await new Promise(resolve => {
    let stmt = DBConn().createAsyncStatement(
      `UPDATE moz_places SET foreign_count = 10 WHERE url_hash = hash(:t_url)
                                                  AND url = :t_url `);
    stmt.params.t_url = T_URI.spec;
    stmt.executeAsync({
      handleCompletion() {
        resolve();
      }
    });
    stmt.finalize();
  });
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 10);

  // Run maintenance
  Components.utils.import("resource://gre/modules/PlacesDBUtils.jsm");
  await PlacesDBUtils.maintenanceOnIdle();

  // Check if the foreign_count has been adjusted to the correct value
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 0);
});

add_task(async function add_remove_tags_test() {
  let conn = await PlacesUtils.promiseDBConnection();

  await PlacesTestUtils.addVisits(T_URI);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 0);

  // Check foreign count incremented by 1 for a single tag
  PlacesUtils.tagging.tagURI(T_URI, ["test tag"]);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 1);

  // Check foreign count is incremented by 2 for two tags
  PlacesUtils.tagging.tagURI(T_URI, ["one", "two"]);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 3);

  // Check foreign count is set to 0 when all tags are removed
  PlacesUtils.tagging.untagURI(T_URI, ["test tag", "one", "two"]);
  Assert.equal((await getForeignCountForURL(conn, T_URI)), 0);
});
