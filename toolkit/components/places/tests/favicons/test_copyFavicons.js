const TEST_URI1 = Services.io.newURI("http://mozilla.com/");
const TEST_URI2 = Services.io.newURI("http://places.com/");
const TEST_URI3 = Services.io.newURI("http://bookmarked.com/");
const LOAD_NON_PRIVATE = PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE;
const LOAD_PRIVATE = PlacesUtils.favicons.FAVICON_LOAD_PRIVATE;

function copyFavicons(source, dest, inPrivate) {
  return new Promise(resolve => {
    PlacesUtils.favicons.copyFavicons(
      source,
      dest,
      inPrivate ? LOAD_PRIVATE : LOAD_NON_PRIVATE,
      resolve
    );
  });
}

function promisePageChanged() {
  return new Promise(resolve => {
    let observer = new NavHistoryObserver();
    observer.onPageChanged = (uri, attribute, newValue, guid) => {
      info("onPageChanged for attribute " + attribute + " and uri " + uri.spec);
      if (attribute == Ci.nsINavHistoryObserver.ATTRIBUTE_FAVICON) {
        PlacesUtils.history.removeObserver(observer);
        Assert.ok(newValue, "newValue should be a valid value");
        Assert.ok(guid, "Guid should be a valid value");
        resolve(uri);
      }
    };
    PlacesUtils.history.addObserver(observer, false);
  });
}

add_task(async function test_copyFavicons_inputcheck() {
  Assert.throws(
    () => PlacesUtils.favicons.copyFavicons(null, TEST_URI2, LOAD_PRIVATE),
    /NS_ERROR_ILLEGAL_VALUE/
  );
  Assert.throws(
    () => PlacesUtils.favicons.copyFavicons(TEST_URI1, null, LOAD_PRIVATE),
    /NS_ERROR_ILLEGAL_VALUE/
  );
  Assert.throws(
    () => PlacesUtils.favicons.copyFavicons(TEST_URI1, TEST_URI2, 3),
    /NS_ERROR_ILLEGAL_VALUE/
  );
  Assert.throws(
    () => PlacesUtils.favicons.copyFavicons(TEST_URI1, TEST_URI2, -1),
    /NS_ERROR_ILLEGAL_VALUE/
  );
  Assert.throws(
    () => PlacesUtils.favicons.copyFavicons(TEST_URI1, TEST_URI2, null),
    /NS_ERROR_ILLEGAL_VALUE/
  );
});

add_task(async function test_copyFavicons_noop() {
  info("Unknown uris");
  Assert.equal(
    await copyFavicons(TEST_URI1, TEST_URI2, false),
    null,
    "Icon should not have been copied"
  );

  info("Unknown dest uri");
  await PlacesTestUtils.addVisits(TEST_URI1);
  Assert.equal(
    await copyFavicons(TEST_URI1, TEST_URI2, false),
    null,
    "Icon should not have been copied"
  );

  info("Unknown dest uri");
  await PlacesTestUtils.addVisits(TEST_URI1);
  Assert.equal(
    await copyFavicons(TEST_URI1, TEST_URI2, false),
    null,
    "Icon should not have been copied"
  );

  info("Unknown dest uri, source has icon");
  await setFaviconForPage(TEST_URI1, SMALLPNG_DATA_URI);
  Assert.equal(
    await copyFavicons(TEST_URI1, TEST_URI2, false),
    null,
    "Icon should not have been copied"
  );

  info("Known uris, source has icon, private");
  await PlacesTestUtils.addVisits(TEST_URI2);
  Assert.equal(
    await copyFavicons(TEST_URI1, TEST_URI2, true),
    null,
    "Icon should not have been copied"
  );

  PlacesUtils.favicons.expireAllFavicons();
  await PlacesUtils.history.clear();
});

add_task(async function test_copyFavicons() {
  info("Normal copy across 2 pages");
  await PlacesTestUtils.addVisits(TEST_URI1);
  await setFaviconForPage(TEST_URI1, SMALLPNG_DATA_URI);
  await setFaviconForPage(TEST_URI1, SMALLSVG_DATA_URI);
  await PlacesTestUtils.addVisits(TEST_URI2);
  let promiseChange = promisePageChanged();
  Assert.equal(
    (await copyFavicons(TEST_URI1, TEST_URI2, false)).spec,
    SMALLSVG_DATA_URI.spec,
    "Icon should have been copied"
  );
  Assert.equal(
    (await promiseChange).spec,
    TEST_URI2.spec,
    "Notification should have fired"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI2, 1),
    SMALLPNG_DATA_URI.spec,
    "Small icon found"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI2),
    SMALLSVG_DATA_URI.spec,
    "Large icon found"
  );

  info("Private copy to a bookmarked page");
  await PlacesUtils.bookmarks.insert({
    url: TEST_URI3,
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
  });
  promiseChange = promisePageChanged();
  Assert.equal(
    (await copyFavicons(TEST_URI1, TEST_URI3, true)).spec,
    SMALLSVG_DATA_URI.spec,
    "Icon should have been copied"
  );
  Assert.equal(
    (await promiseChange).spec,
    TEST_URI3.spec,
    "Notification should have fired"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI3, 1),
    SMALLPNG_DATA_URI.spec,
    "Small icon found"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI3),
    SMALLSVG_DATA_URI.spec,
    "Large icon found"
  );

  PlacesUtils.favicons.expireAllFavicons();
  await PlacesUtils.history.clear();
});

add_task(async function test_copyFavicons_overlap() {
  info("Copy to a page that has one of the favicons already");
  await PlacesTestUtils.addVisits(TEST_URI1);
  await setFaviconForPage(TEST_URI1, SMALLPNG_DATA_URI);
  await setFaviconForPage(TEST_URI1, SMALLSVG_DATA_URI);
  await PlacesTestUtils.addVisits(TEST_URI2);
  await setFaviconForPage(TEST_URI2, SMALLPNG_DATA_URI);
  let promiseChange = promisePageChanged();
  Assert.equal(
    (await copyFavicons(TEST_URI1, TEST_URI2, false)).spec,
    SMALLSVG_DATA_URI.spec,
    "Icon should have been copied"
  );
  Assert.equal(
    (await promiseChange).spec,
    TEST_URI2.spec,
    "Notification should have fired"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI2, 1),
    SMALLPNG_DATA_URI.spec,
    "Small icon found"
  );
  Assert.equal(
    await getFaviconUrlForPage(TEST_URI2),
    SMALLSVG_DATA_URI.spec,
    "Large icon found"
  );
});
