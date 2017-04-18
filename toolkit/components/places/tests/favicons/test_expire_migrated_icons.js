/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests that favicons migrated from a previous profile, having a 0
 * expiration, will be properly expired when fetching new ones.
 */

add_task(function* test_storing_a_normal_16x16_icon() {
  const PAGE_URL = "http://places.test";
  yield PlacesTestUtils.addVisits(PAGE_URL);
  yield setFaviconForPage(PAGE_URL, SMALLPNG_DATA_URI);

  // Now set expiration to 0 and change the payload.
  do_print("Set expiration to 0 and replace favicon data");
  yield PlacesUtils.withConnectionWrapper("Change favicons payload", db => {
    return db.execute(`UPDATE moz_icons SET expire_ms = 0, data = "test"`);
  });

  let {data, mimeType} = yield getFaviconDataForPage(PAGE_URL);
  Assert.equal(mimeType, "image/png");
  Assert.deepEqual(data, "test".split("").map(c => c.charCodeAt(0)));

  do_print("Refresh favicon");
  yield setFaviconForPage(PAGE_URL, SMALLPNG_DATA_URI, false);
  yield compareFavicons("page-icon:" + PAGE_URL, SMALLPNG_DATA_URI);
});
