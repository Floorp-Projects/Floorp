/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests root icons associations and expiration
 */

add_task(async function() {
  let pageURI = NetUtil.newURI("http://www.places.test/page/");
  await PlacesTestUtils.addVisits(pageURI);
  let faviconURI = NetUtil.newURI("http://www.places.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLPNG_DATA_URI.spec, 0, systemPrincipal);
  await setFaviconForPage(pageURI, faviconURI);

  // Sanity checks.
  Assert.equal(await getFaviconUrlForPage(pageURI), faviconURI.spec);
  Assert.equal(await getFaviconUrlForPage("https://places.test/somethingelse/"),
               faviconURI.spec);

  // Check database entries.
  await PlacesTestUtils.promiseAsyncUpdates();
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 1, "There should only be 1 icon entry");
  Assert.equal(rows[0].getResultByName("root"), 1, "It should be marked as a root icon");
  rows = await db.execute("SELECT * FROM moz_pages_w_icons");
  Assert.equal(rows.length, 0, "There should be no page entry");
  rows = await db.execute("SELECT * FROM moz_icons_to_pages");
  Assert.equal(rows.length, 0, "There should be no relation entry");

  // Add another pages to the same host. The icon should not be removed.
  await PlacesTestUtils.addVisits("http://places.test/page2/");
  await PlacesUtils.history.remove(pageURI);

  // Still works since the icon has not been removed.
  Assert.equal(await getFaviconUrlForPage(pageURI), faviconURI.spec);

  // Remove all the pages for the given domain.
  await PlacesUtils.history.remove("http://places.test/page2/");
  // The icon should be removed along with the domain.
  rows = await db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 0, "The icon should have been removed");
});

add_task(async function test_removePagesByTimeframe() {
  // Add a visit in the past with no directly associated icon.
  await PlacesTestUtils.addVisits({uri: "http://www.places.test/old/", visitDate: new Date(Date.now() - 86400000)});
  let pageURI = NetUtil.newURI("http://www.places.test/page/");
  await PlacesTestUtils.addVisits({uri: pageURI, visitDate: new Date(Date.now() - 7200000)});
  let faviconURI = NetUtil.newURI("http://www.places.test/page/favicon.ico");
  let rootIconURI = NetUtil.newURI("http://www.places.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLSVG_DATA_URI.spec, 0, systemPrincipal);
  await setFaviconForPage(pageURI, faviconURI);
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    rootIconURI, SMALLPNG_DATA_URI.spec, 0, systemPrincipal);
  await setFaviconForPage(pageURI, rootIconURI);

  // Sanity checks.
  Assert.equal(await getFaviconUrlForPage(pageURI),
               faviconURI.spec, "Should get the biggest icon");
  Assert.equal(await getFaviconUrlForPage(pageURI, 1),
               rootIconURI.spec, "Should get the smallest icon");
  Assert.equal(await getFaviconUrlForPage("http://www.places.test/old/"),
               rootIconURI.spec, "Should get the root icon");

  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(Date.now() - 14400000),
    endDate: new Date()
  });

  // Check database entries.
  await PlacesTestUtils.promiseAsyncUpdates();
  let db = await PlacesUtils.promiseDBConnection();
  let rows = await db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 1, "There should only be 1 icon entry");
  Assert.equal(rows[0].getResultByName("root"), 1, "It should be marked as a root icon");
  rows = await db.execute("SELECT * FROM moz_pages_w_icons");
  Assert.equal(rows.length, 0, "There should be no page entry");
  rows = await db.execute("SELECT * FROM moz_icons_to_pages");
  Assert.equal(rows.length, 0, "There should be no relation entry");

  await PlacesUtils.history.removeByFilter({
    beginDate: new Date(0),
    endDt: new Date()
  });
  await PlacesTestUtils.promiseAsyncUpdates();
  rows = await db.execute("SELECT * FROM moz_icons");
  // Debug logging for possible intermittent failure (bug 1358368).
  if (rows.length != 0) {
    dump_table("moz_icons");
  }
  Assert.equal(rows.length, 0, "There should be no icon entry");
});

add_task(async function test_different_host() {
  let pageURI = NetUtil.newURI("http://places.test/page/");
  await PlacesTestUtils.addVisits(pageURI);
  let faviconURI = NetUtil.newURI("http://mozilla.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLPNG_DATA_URI.spec, 0, systemPrincipal);
  await setFaviconForPage(pageURI, faviconURI);

  Assert.equal(await getFaviconUrlForPage(pageURI),
               faviconURI.spec, "Should get the png icon");
});

add_task(async function test_same_size() {
  // Add two icons with the same size, one is a root icon. Check that the
  // non-root icon is preferred when a smaller size is requested.
  let data = readFileData(do_get_file("favicon-normal32.png"));
  let pageURI = NetUtil.newURI("http://new_places.test/page/");
  await PlacesTestUtils.addVisits(pageURI);

  let faviconURI = NetUtil.newURI("http://new_places.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconData(faviconURI, data, data.length, "image/png");
  await setFaviconForPage(pageURI, faviconURI);
  faviconURI = NetUtil.newURI("http://new_places.test/another_icon.ico");
  PlacesUtils.favicons.replaceFaviconData(faviconURI, data, data.length, "image/png");
  await setFaviconForPage(pageURI, faviconURI);

  Assert.equal(await getFaviconUrlForPage(pageURI, 20),
               faviconURI.spec, "Should get the non-root icon");
});
