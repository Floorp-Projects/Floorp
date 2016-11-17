/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests getFaviconURLForPage.
 */

// Tests

function run_test()
{
  run_next_test();
}

add_test(function test_normal()
{
  let pageURI = NetUtil.newURI("http://example.com/normal");

  PlacesTestUtils.addVisits(pageURI).then(function() {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI, SMALLPNG_DATA_URI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        function() {
        PlacesUtils.favicons.getFaviconURLForPage(pageURI,
          function(aURI, aDataLen, aData, aMimeType) {
            do_check_true(aURI.equals(SMALLPNG_DATA_URI));

            // Check also the expected data types.
            do_check_true(aDataLen === 0);
            do_check_true(aData.length === 0);
            do_check_true(aMimeType === "");
            run_next_test();
          });
      }, Services.scriptSecurityManager.getSystemPrincipal());
  });
});

add_test(function test_missing()
{
  let pageURI = NetUtil.newURI("http://example.com/missing");

  PlacesUtils.favicons.getFaviconURLForPage(pageURI,
    function(aURI, aDataLen, aData, aMimeType) {
      // Check also the expected data types.
      do_check_true(aURI === null);
      do_check_true(aDataLen === 0);
      do_check_true(aData.length === 0);
      do_check_true(aMimeType === "");
      run_next_test();
    });
});
