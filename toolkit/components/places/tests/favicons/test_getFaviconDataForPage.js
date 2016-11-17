/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests getFaviconDataForPage.
 */

// Globals

const FAVICON_URI = NetUtil.newURI(do_get_file("favicon-normal32.png"));
const FAVICON_DATA = readFileData(do_get_file("favicon-normal32.png"));
const FAVICON_MIMETYPE = "image/png";

// Tests

function run_test()
{
  // Check that the favicon loaded correctly before starting the actual tests.
  do_check_eq(FAVICON_DATA.length, 344);
  run_next_test();
}

add_test(function test_normal()
{
  let pageURI = NetUtil.newURI("http://example.com/normal");

  PlacesTestUtils.addVisits(pageURI).then(function() {
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
            run_next_test();
          });
      }, Services.scriptSecurityManager.getSystemPrincipal());
  });
});

add_test(function test_missing()
{
  let pageURI = NetUtil.newURI("http://example.com/missing");

  PlacesUtils.favicons.getFaviconDataForPage(pageURI,
    function(aURI, aDataLen, aData, aMimeType) {
      // Check also the expected data types.
      do_check_true(aURI === null);
      do_check_true(aDataLen === 0);
      do_check_true(aData.length === 0);
      do_check_true(aMimeType === "");
      run_next_test();
    });
});
