/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that bookmarking an hidden page unhides it.

"use strict";

add_task(async function test_hidden() {
  const url = "http://moz.com/";
  await PlacesTestUtils.addVisits({
    url,
    transition: TRANSITION_FRAMED_LINK,
  });
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "hidden", { url }),
    1
  );
  await PlacesUtils.bookmarks.insert({
    url,
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
  });
  Assert.equal(
    await PlacesTestUtils.getDatabaseValue("moz_places", "hidden", { url }),
    0
  );
});
