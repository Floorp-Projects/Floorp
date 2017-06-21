/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const FAVICON_URI = NetUtil.newURI(do_get_file("favicon-normal32.png"));
const FAVICON_DATA = readFileData(do_get_file("favicon-normal32.png"));
const FAVICON_MIMETYPE = "image/png";
const ICON32_URL = "http://places.test/favicon-normal32.png";

add_task(async function test_normal() {
  do_check_eq(FAVICON_DATA.length, 344);
  let pageURI = NetUtil.newURI("http://example.com/normal");

  await PlacesTestUtils.addVisits(pageURI);
  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI, FAVICON_URI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        function() {
        PlacesUtils.favicons.getFaviconDataForPage(pageURI,
          function(aURI, aDataLen, aData, aMimeType) {
            do_check_true(aURI.equals(FAVICON_URI));
            do_check_eq(FAVICON_DATA.length, aDataLen);
            do_check_true(compareArrays(FAVICON_DATA, aData));
            do_check_eq(FAVICON_MIMETYPE, aMimeType);
            resolve();
          });
      }, Services.scriptSecurityManager.getSystemPrincipal());
  });
});

add_task(async function test_missing() {
  let pageURI = NetUtil.newURI("http://example.com/missing");

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(pageURI,
      function(aURI, aDataLen, aData, aMimeType) {
        // Check also the expected data types.
        do_check_true(aURI === null);
        do_check_true(aDataLen === 0);
        do_check_true(aData.length === 0);
        do_check_true(aMimeType === "");
        resolve();
      });
  });
});

add_task(async function test_fallback() {
  const ROOT_URL = "https://www.example.com/";
  const ROOT_ICON_URL = ROOT_URL + "favicon.ico";
  const SUBPAGE_URL = ROOT_URL + "/missing";

  do_print("Set icon for the root");
  await PlacesTestUtils.addVisits(ROOT_URL);
  let data = readFileData(do_get_file("favicon-normal16.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ROOT_ICON_URL),
                                          data, data.length, "image/png");
  await setFaviconForPage(ROOT_URL, ROOT_ICON_URL);

  do_print("check fallback icons");
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(ROOT_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      });
  });
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(SUBPAGE_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      });
  });

  do_print("Now add a proper icon for the page");
  await PlacesTestUtils.addVisits(SUBPAGE_URL);
  let data32 = readFileData(do_get_file("favicon-normal32.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ICON32_URL),
                                          data32, data32.length, "image/png");
  await setFaviconForPage(SUBPAGE_URL, ICON32_URL);

  do_print("check no fallback icons");
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(ROOT_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ROOT_ICON_URL);
        Assert.equal(aDataLen, data.length);
        Assert.deepEqual(aData, data);
        Assert.equal(aMimeType, "image/png");
        resolve();
      });
  });
  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconDataForPage(NetUtil.newURI(SUBPAGE_URL),
      (aURI, aDataLen, aData, aMimeType) => {
        Assert.equal(aURI.spec, ICON32_URL);
        Assert.equal(aDataLen, data32.length);
        Assert.deepEqual(aData, data32);
        Assert.equal(aMimeType, "image/png");
        resolve();
      });
  });
});
