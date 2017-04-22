/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests root icons associations and expiration
 */

add_task(function* () {
  let pageURI = NetUtil.newURI("http://www.places.test/page/");
  yield PlacesTestUtils.addVisits(pageURI);
  let faviconURI = NetUtil.newURI("http://www.places.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLPNG_DATA_URI.spec, 0, Services.scriptSecurityManager.getSystemPrincipal());
  yield setFaviconForPage(pageURI, faviconURI);

  // Sanity checks.
  Assert.equal(yield getFaviconUrlForPage(pageURI), faviconURI.spec);
  Assert.equal(yield getFaviconUrlForPage("https://places.test/somethingelse/"),
               faviconURI.spec);

  // Check database entries.
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 1, "There should only be 1 icon entry");
  Assert.equal(rows[0].getResultByName("root"), 1, "It should be marked as a root icon");
  rows = yield db.execute("SELECT * FROM moz_pages_w_icons");
  Assert.equal(rows.length, 0, "There should be no page entry");
  rows = yield db.execute("SELECT * FROM moz_icons_to_pages");
  Assert.equal(rows.length, 0, "There should be no relation entry");

  // Add another pages to the same host. The icon should not be removed.
  yield PlacesTestUtils.addVisits("http://places.test/page2/");
  yield PlacesUtils.history.remove(pageURI);

  // Still works since the icon has not been removed.
  Assert.equal(yield getFaviconUrlForPage(pageURI), faviconURI.spec);

  // Remove all the pages for the given domain.
  yield PlacesUtils.history.remove("http://places.test/page2/");
  // The icon should be removed along with the domain.
  rows = yield db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 0, "The icon should have been removed");
});

add_task(function* test_removePagesByTimeframe() {
  // Add a visit in the past with no directly associated icon.
  yield PlacesTestUtils.addVisits({uri: "http://www.places.test/old/", visitDate: new Date(Date.now() - 86400000)});
  let pageURI = NetUtil.newURI("http://www.places.test/page/");
  yield PlacesTestUtils.addVisits({uri: pageURI, visitDate: new Date(Date.now() - 7200000)});
  let faviconURI = NetUtil.newURI("http://www.places.test/page/favicon.ico");
  let rootIconURI = NetUtil.newURI("http://www.places.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLSVG_DATA_URI.spec, 0, Services.scriptSecurityManager.getSystemPrincipal());
  yield setFaviconForPage(pageURI, faviconURI);
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    rootIconURI, SMALLPNG_DATA_URI.spec, 0, Services.scriptSecurityManager.getSystemPrincipal());
  yield setFaviconForPage(pageURI, rootIconURI);

  // Sanity checks.
  Assert.equal(yield getFaviconUrlForPage(pageURI),
               faviconURI.spec, "Should get the biggest icon");
  Assert.equal(yield getFaviconUrlForPage(pageURI, 1),
               rootIconURI.spec, "Should get the smallest icon");
  Assert.equal(yield getFaviconUrlForPage("http://www.places.test/old/"),
               rootIconURI.spec, "Should get the root icon");

  PlacesUtils.history.removePagesByTimeframe(
    PlacesUtils.toPRTime(Date.now() - 14400000),
    PlacesUtils.toPRTime(new Date())
  );

  // Check database entries.
  let db = yield PlacesUtils.promiseDBConnection();
  let rows = yield db.execute("SELECT * FROM moz_icons");
  Assert.equal(rows.length, 1, "There should only be 1 icon entry");
  Assert.equal(rows[0].getResultByName("root"), 1, "It should be marked as a root icon");
  rows = yield db.execute("SELECT * FROM moz_pages_w_icons");
  Assert.equal(rows.length, 0, "There should be no page entry");
  rows = yield db.execute("SELECT * FROM moz_icons_to_pages");
  Assert.equal(rows.length, 0, "There should be no relation entry");

  PlacesUtils.history.removePagesByTimeframe(0, PlacesUtils.toPRTime(new Date()));
  yield PlacesTestUtils.promiseAsyncUpdates();
  rows = yield db.execute("SELECT * FROM moz_icons");
  // Debug logging for possible intermittent failure (bug 1358368).
  if (rows.length != 0) {
    dump_table("moz_icons");
    dump_table("moz_hosts");
  }
  Assert.equal(rows.length, 0, "There should be no icon entry");
});

add_task(function* test_different_host() {
  let pageURI = NetUtil.newURI("http://places.test/page/");
  yield PlacesTestUtils.addVisits(pageURI);
  let faviconURI = NetUtil.newURI("http://mozilla.test/favicon.ico");
  PlacesUtils.favicons.replaceFaviconDataFromDataURL(
    faviconURI, SMALLPNG_DATA_URI.spec, 0, Services.scriptSecurityManager.getSystemPrincipal());
  yield setFaviconForPage(pageURI, faviconURI);

  Assert.equal(yield getFaviconUrlForPage(pageURI),
               faviconURI.spec, "Should get the png icon");
});
