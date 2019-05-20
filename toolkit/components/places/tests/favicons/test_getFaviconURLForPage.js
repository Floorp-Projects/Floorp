/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const ICON32_URL = "http://places.test/favicon-normal32.png";

add_task(async function test_normal() {
  let pageURI = NetUtil.newURI("http://example.com/normal");

  await PlacesTestUtils.addVisits(pageURI);
  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI, SMALLPNG_DATA_URI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        function() {
        PlacesUtils.favicons.getFaviconURLForPage(pageURI,
          function(aURI, aDataLen, aData, aMimeType) {
            Assert.ok(aURI.equals(SMALLPNG_DATA_URI));

            // Check also the expected data types.
            Assert.ok(aDataLen === 0);
            Assert.ok(aData.length === 0);
            Assert.ok(aMimeType === "");
            resolve();
          });
      }, Services.scriptSecurityManager.getSystemPrincipal());
  });
});

add_task(async function test_missing() {
  let pageURI = NetUtil.newURI("http://example.com/missing");

  await new Promise(resolve => {
    PlacesUtils.favicons.getFaviconURLForPage(pageURI,
      function(aURI, aDataLen, aData, aMimeType) {
        // Check also the expected data types.
        Assert.ok(aURI === null);
        Assert.ok(aDataLen === 0);
        Assert.ok(aData.length === 0);
        Assert.ok(aMimeType === "");
        resolve();
      });
  });
});

add_task(async function test_fallback() {
  const ROOT_URL = "https://www.example.com/";
  const ROOT_ICON_URL = ROOT_URL + "favicon.ico";
  const SUBPAGE_URL = ROOT_URL + "/missing";

  info("Set icon for the root");
  await PlacesTestUtils.addVisits(ROOT_URL);
  let data = readFileData(do_get_file("favicon-normal16.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ROOT_ICON_URL),
                                          data, "image/png");
  await setFaviconForPage(ROOT_URL, ROOT_ICON_URL);

  info("check fallback icons");
  Assert.equal(await getFaviconUrlForPage(ROOT_URL), ROOT_ICON_URL,
               "The root should have its favicon");
  Assert.equal(await getFaviconUrlForPage(SUBPAGE_URL), ROOT_ICON_URL,
               "The page should fallback to the root icon");

  info("Now add a proper icon for the page");
  await PlacesTestUtils.addVisits(SUBPAGE_URL);
  let data32 = readFileData(do_get_file("favicon-normal32.png"));
  PlacesUtils.favicons.replaceFaviconData(NetUtil.newURI(ICON32_URL),
                                          data32, "image/png");
  await setFaviconForPage(SUBPAGE_URL, ICON32_URL);

  info("check no fallback icons");
  Assert.equal(await getFaviconUrlForPage(ROOT_URL), ROOT_ICON_URL,
               "The root should still have its favicon");
  Assert.equal(await getFaviconUrlForPage(SUBPAGE_URL), ICON32_URL,
               "The page should also have its icon");
});
