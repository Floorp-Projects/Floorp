/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function assert_date_eq(a, b) {
  if (typeof(a) != "number") {
    a = PlacesUtils.toPRTime(a);
  }
  if (typeof(b) != "number") {
    b = PlacesUtils.toPRTime(b);
  }
  Assert.equal(a, b, "The dates should match");
}

 /**
  * Test that inserting a new bookmark will set lastModified to the same
  * values as dateAdded.
  */
add_task(async function test_bookmarkLastModified() {
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://www.mozilla.org/",
    title: "itemTitle"
  });

  let guid = bookmark.guid;

  // Check the bookmark from the database.
  bookmark = await PlacesUtils.bookmarks.fetch(guid);

  let dateAdded = PlacesUtils.toPRTime(bookmark.dateAdded);
  assert_date_eq(dateAdded, bookmark.lastModified);

  // Change lastModified, then change dateAdded.  LastModified should be set
  // to the new dateAdded.
  // This could randomly fail on virtual machines due to timing issues, so
  // we manually increase the time value.  See bug 500640 for details.
  await PlacesUtils.bookmarks.update({
    guid,
    lastModified: PlacesUtils.toDate(dateAdded + 1000)
  });

  bookmark = await PlacesUtils.bookmarks.fetch(guid);

  assert_date_eq(bookmark.lastModified, dateAdded + 1000);
  Assert.ok(bookmark.dateAdded < bookmark.lastModified,
    "Date added should be earlier than last modified.");

  await PlacesUtils.bookmarks.update({
    guid,
    dateAdded: PlacesUtils.toDate(dateAdded + 2000)
  });

  bookmark = await PlacesUtils.bookmarks.fetch(guid);

  assert_date_eq(bookmark.dateAdded, dateAdded + 2000);
  assert_date_eq(bookmark.dateAdded, bookmark.lastModified);

  // If dateAdded is set to older than lastModified, then we shouldn't
  // update lastModified to keep sync happy.
  let origLastModified = bookmark.lastModified;

  await PlacesUtils.bookmarks.update({
    guid,
    dateAdded: PlacesUtils.toDate(dateAdded - 10000)
  });

  bookmark = await PlacesUtils.bookmarks.fetch(guid);

  assert_date_eq(bookmark.dateAdded, dateAdded - 10000);
  assert_date_eq(bookmark.lastModified, origLastModified);

  await PlacesUtils.bookmarks.remove(guid);
});
