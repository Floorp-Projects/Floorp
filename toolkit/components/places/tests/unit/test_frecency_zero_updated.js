/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests a zero frecency is correctly updated when inserting new valid visits.

add_task(async function() {
  const TEST_URI = NetUtil.newURI("http://example.com/");
  let bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URI,
    title: "A title",
  });
  await PlacesTestUtils.promiseAsyncUpdates();
  Assert.ok(frecencyForUrl(TEST_URI) > 0);

  // Removing the bookmark should leave an orphan page with zero frecency.
  // Note this would usually be expired later by expiration.
  await PlacesUtils.bookmarks.remove(bookmark.guid);
  await PlacesTestUtils.promiseAsyncUpdates();
  Assert.equal(frecencyForUrl(TEST_URI), 0);

  // Now add a valid visit to the page, frecency should increase.
  await PlacesTestUtils.addVisits({ uri: TEST_URI });
  Assert.ok(frecencyForUrl(TEST_URI) > 0);
});
