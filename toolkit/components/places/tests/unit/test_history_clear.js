/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var mDBConn = DBConn();

add_task(async function test_history_clear() {
  await PlacesTestUtils.addVisits([
    { uri: uri("http://typed.mozilla.org/"), transition: TRANSITION_TYPED },
    { uri: uri("http://link.mozilla.org/"), transition: TRANSITION_LINK },
    {
      uri: uri("http://download.mozilla.org/"),
      transition: TRANSITION_DOWNLOAD,
    },
    {
      uri: uri("http://redir_temp.mozilla.org/"),
      transition: TRANSITION_REDIRECT_TEMPORARY,
      referrer: "http://link.mozilla.org/",
    },
    {
      uri: uri("http://redir_perm.mozilla.org/"),
      transition: TRANSITION_REDIRECT_PERMANENT,
      referrer: "http://link.mozilla.org/",
    },
  ]);

  // add a place: bookmark
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: `place:parent=${PlacesUtils.bookmarks.tagsGuid}`,
    title: "shortcut",
  });

  // Add an expire never annotation
  // Actually expire never annotations are removed as soon as a page is removed
  // from the database, so this should act as a normal visit.
  await PlacesUtils.history.update({
    url: "http://download.mozilla.org/",
    annotations: new Map([["never", "never"]]),
  });

  // Add a bookmark
  // Bookmarked page should have history cleared and frecency = -1
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://typed.mozilla.org/",
    title: "bookmark",
  });

  await PlacesTestUtils.addVisits([
    { uri: uri("http://typed.mozilla.org/"), transition: TRANSITION_BOOKMARK },
    { uri: uri("http://frecency.mozilla.org/"), transition: TRANSITION_LINK },
  ]);
  await PlacesTestUtils.promiseAsyncUpdates();

  // Clear history and wait for the onClearHistory notification.
  let promiseClearHistory = PlacesTestUtils.waitForNotification(
    "onClearHistory",
    () => true,
    "history"
  );
  await PlacesUtils.history.clear();
  await promiseClearHistory;
  await PlacesTestUtils.promiseAsyncUpdates();

  // Check that frecency for not cleared items (bookmarks) has been converted
  // to -1.
  let stmt = mDBConn.createStatement(
    "SELECT h.id FROM moz_places h WHERE h.frecency > 0 "
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  stmt = mDBConn.createStatement(
    `SELECT h.id FROM moz_places h WHERE h.frecency < 0
       AND EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1`
  );
  Assert.ok(stmt.executeStep());
  stmt.finalize();

  // Check that all visit_counts have been brought to 0
  stmt = mDBConn.createStatement(
    "SELECT id FROM moz_places WHERE visit_count <> 0 LIMIT 1"
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that history tables are empty
  stmt = mDBConn.createStatement(
    "SELECT * FROM (SELECT id FROM moz_historyvisits LIMIT 1)"
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that all moz_places entries except bookmarks and place: have been removed
  stmt = mDBConn.createStatement(
    `SELECT h.id FROM moz_places h WHERE
       url_hash NOT BETWEEN hash('place', 'prefix_lo') AND hash('place', 'prefix_hi')
       AND NOT EXISTS (SELECT id FROM moz_bookmarks WHERE fk = h.id) LIMIT 1`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that we only have favicons for retained places
  stmt = mDBConn.createStatement(
    `SELECT 1
     FROM moz_pages_w_icons
     LEFT JOIN moz_places h ON url_hash = page_url_hash AND url = page_url
     WHERE h.id ISNULL`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();
  stmt = mDBConn.createStatement(
    `SELECT 1
     FROM moz_icons WHERE id NOT IN (
       SELECT icon_id FROM moz_icons_to_pages
     )`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that we only have annotations for retained places
  stmt = mDBConn.createStatement(
    `SELECT a.id FROM moz_annos a WHERE NOT EXISTS
       (SELECT id FROM moz_places WHERE id = a.place_id) LIMIT 1`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that we only have inputhistory for retained places
  stmt = mDBConn.createStatement(
    `SELECT i.place_id FROM moz_inputhistory i WHERE NOT EXISTS
       (SELECT id FROM moz_places WHERE id = i.place_id) LIMIT 1`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();

  // Check that place:uris have frecency 0
  stmt = mDBConn.createStatement(
    `SELECT h.id FROM moz_places h
     WHERE url_hash BETWEEN hash('place', 'prefix_lo')
                        AND hash('place', 'prefix_hi')
       AND h.frecency <> 0 LIMIT 1`
  );
  Assert.ok(!stmt.executeStep());
  stmt.finalize();
});
