/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests that adding new icons for a page expired old ones.
 */

add_task(async function test_expire_associated() {
  const TEST_URL = "http://mozilla.com/";
  await PlacesTestUtils.addVisits(TEST_URL);
  const TEST_URL2 = "http://test.mozilla.com/";
  await PlacesTestUtils.addVisits(TEST_URL2);

  let favicons = [
    {
      name: "favicon-normal16.png",
      mimeType: "image/png",
      expire: PlacesUtils.toPRTime(new Date(1)), // Very old.
    },
    {
      name: "favicon-normal32.png",
      mimeType: "image/png",
      expire: 0,
    },
    {
      name: "favicon-big64.png",
      mimeType: "image/png",
      expire: 0,
    },
  ];

  for (let icon of favicons) {
    let data = readFileData(do_get_file(icon.name));
    PlacesUtils.favicons.replaceFaviconData(
      NetUtil.newURI(TEST_URL + icon.name),
      data,
      icon.mimeType,
      icon.expire
    );
    await setFaviconForPage(TEST_URL, TEST_URL + icon.name);
    if (icon.expire != 0) {
      PlacesUtils.favicons.replaceFaviconData(
        NetUtil.newURI(TEST_URL + icon.name),
        data,
        icon.mimeType,
        icon.expire
      );
      await setFaviconForPage(TEST_URL2, TEST_URL + icon.name);
    }
  }

  // Only the second and the third icons should have survived.
  Assert.equal(
    await getFaviconUrlForPage(TEST_URL, 16),
    TEST_URL + favicons[1].name,
    "Should retrieve the 32px icon, not the 16px one."
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URL, 64),
    TEST_URL + favicons[2].name,
    "Should retrieve the 64px icon"
  );

  // The expired icon for page 2 should have survived.
  Assert.equal(
    await getFaviconUrlForPage(TEST_URL2, 16),
    TEST_URL + favicons[0].name,
    "Should retrieve the expired 16px icon"
  );
});

add_task(async function test_expire_root() {
  async function countEntries(tablename) {
    await PlacesTestUtils.promiseAsyncUpdates();
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute("SELECT * FROM " + tablename);
    return rows.length;
  }

  // Clear the database.
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_FAVICONS_EXPIRED);
  PlacesUtils.favicons.expireAllFavicons();
  await promise;

  Assert.equal(await countEntries("moz_icons"), 0, "There should be no icons");

  let pageURI = NetUtil.newURI("http://root.mozilla.com/");
  await PlacesTestUtils.addVisits(pageURI);

  // Insert an expired icon.
  let iconURI = NetUtil.newURI(pageURI.spec + "favicon-normal16.png");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    iconURI,
    SMALLPNG_DATA_URI.spec,
    PlacesUtils.toPRTime(new Date(1)),
    systemPrincipal
  );
  await setFaviconForPage(pageURI, iconURI);

  Assert.equal(
    await countEntries("moz_icons_to_pages"),
    1,
    "There should be 1 association"
  );

  // Now insert a new root icon.
  let rootIconURI = NetUtil.newURI(pageURI.spec + "favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    rootIconURI,
    SMALLPNG_DATA_URI.spec,
    0,
    systemPrincipal
  );
  await setFaviconForPage(pageURI, rootIconURI);

  // Only the root icon should have survived.
  Assert.equal(
    await getFaviconUrlForPage(pageURI, 16),
    rootIconURI.spec,
    "Should retrieve the root icon."
  );
  Assert.equal(
    await countEntries("moz_icons_to_pages"),
    0,
    "There should be no associations"
  );
});
