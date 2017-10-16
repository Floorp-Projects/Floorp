/*
 * Tests that adding new icons for a page expired old ones.
 */

add_task(async function test_replaceFaviconData_validHistoryURI() {
  const TEST_URL = "http://mozilla.com/";
  await PlacesTestUtils.addVisits(TEST_URL);
  const TEST_URL2 = "http://test.mozilla.com/";
  await PlacesTestUtils.addVisits(TEST_URL2);

  let favicons = [
    {
      name: "favicon-normal16.png",
      mimeType: "image/png",
      expire: PlacesUtils.toPRTime(new Date(1)) // Very old.
    },
    {
      name: "favicon-normal32.png",
      mimeType: "image/png",
      expire: 0
    },
    {
      name: "favicon-big64.png",
      mimeType: "image/png",
      expire: 0
    },
  ];

  for (let icon of favicons) {
    let data = readFileData(do_get_file(icon.name));
    PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(TEST_URL + icon.name),
                                            data, data.length,
                                            icon.mimeType,
                                            icon.expire);
    await setFaviconForPage(TEST_URL, TEST_URL + icon.name);
    if (icon.expire != 0) {
      PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(TEST_URL + icon.name),
                                              data, data.length,
                                              icon.mimeType,
                                              icon.expire);
      await setFaviconForPage(TEST_URL2, TEST_URL + icon.name);
    }
  }

  // Only the second and the third icons should have survived.
  Assert.equal(await getFaviconUrlForPage(TEST_URL, 16),
               TEST_URL + favicons[1].name,
               "Should retrieve the 32px icon, not the 16px one.");
  Assert.equal(await getFaviconUrlForPage(TEST_URL, 64),
               TEST_URL + favicons[2].name,
               "Should retrieve the 64px icon");

  // The expired icon for page 2 should have survived.
  Assert.equal(await getFaviconUrlForPage(TEST_URL2, 16),
               TEST_URL + favicons[0].name,
               "Should retrieve the expired 16px icon");
});
