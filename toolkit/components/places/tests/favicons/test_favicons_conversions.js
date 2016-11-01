/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the image conversions done by the favicon service.
 */

// //////////////////////////////////////////////////////////////////////////////
// / Globals

// The pixel values we get on Windows are sometimes +/- 1 value compared to
// other platforms, so we need to skip some image content tests.
var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

/**
 * Checks the conversion of the given test image file.
 *
 * @param aFileName
 *        File that contains the favicon image, located in the test folder.
 * @param aFileMimeType
 *        MIME type of the image contained in the file.
 * @param aFileLength
 *        Expected length of the file.
 * @param aExpectConversion
 *        If false, the icon should be stored as is.  If true, the expected data
 *        is loaded from a file named "expected-" + aFileName + ".png".
 * @param aVaryOnWindows
 *        Indicates that the content of the converted image can be different on
 *        Windows and should not be checked on that platform.
 * @param aCallback
 *        This function is called after the check finished.
 */
function checkFaviconDataConversion(aFileName, aFileMimeType, aFileLength,
                                    aExpectConversion, aVaryOnWindows,
                                    aCallback) {
  let pageURI = NetUtil.newURI("http://places.test/page/" + aFileName);
  PlacesTestUtils.addVisits({ uri: pageURI, transition: TRANSITION_TYPED }).then(
    function () {
      let faviconURI = NetUtil.newURI("http://places.test/icon/" + aFileName);
      let fileData = readFileOfLength(aFileName, aFileLength);

      PlacesUtils.favicons.replaceFaviconData(faviconURI, fileData, fileData.length,
                                              aFileMimeType);
      PlacesUtils.favicons.setAndFetchFaviconForPage(pageURI, faviconURI, true,
        PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
        function CFDC_verify(aURI, aDataLen, aData, aMimeType) {
          if (!aExpectConversion) {
            do_check_true(compareArrays(aData, fileData));
            do_check_eq(aMimeType, aFileMimeType);
          } else {
            if (!aVaryOnWindows || !isWindows) {
              let expectedFile = do_get_file("expected-" + aFileName + ".png");
              do_check_true(compareArrays(aData, readFileData(expectedFile)));
            }
            do_check_eq(aMimeType, "image/png");
          }

          aCallback();
        }, Services.scriptSecurityManager.getSystemPrincipal());
    });
}

// //////////////////////////////////////////////////////////////////////////////
// / Tests

function run_test() {
  run_next_test();
}

add_test(function test_storing_a_normal_16x16_icon() {
  // 16x16 png, 286 bytes.
  // optimized: no
  checkFaviconDataConversion("favicon-normal16.png", "image/png", 286,
                             false, false, run_next_test);
});

add_test(function test_storing_a_normal_32x32_icon() {
  // 32x32 png, 344 bytes.
  // optimized: no
  checkFaviconDataConversion("favicon-normal32.png", "image/png", 344,
                             false, false, run_next_test);
});

add_test(function test_storing_a_big_16x16_icon() {
  //  in: 16x16 ico, 1406 bytes.
  // optimized: no
  checkFaviconDataConversion("favicon-big16.ico", "image/x-icon", 1406,
                             false, false, run_next_test);
});

add_test(function test_storing_an_oversize_4x4_icon() {
  //  in: 4x4 jpg, 4751 bytes.
  // optimized: yes
  checkFaviconDataConversion("favicon-big4.jpg", "image/jpeg", 4751,
                             true, false, run_next_test);
});

add_test(function test_storing_an_oversize_32x32_icon() {
  //  in: 32x32 jpg, 3494 bytes.
  // optimized: yes
  checkFaviconDataConversion("favicon-big32.jpg", "image/jpeg", 3494,
                             true, true, run_next_test);
});

add_test(function test_storing_an_oversize_48x48_icon() {
  //  in: 48x48 ico, 56646 bytes.
  // (howstuffworks.com icon, contains 13 icons with sizes from 16x16 to
  // 48x48 in varying depths)
  // optimized: yes
  checkFaviconDataConversion("favicon-big48.ico", "image/x-icon", 56646,
                             true, false, run_next_test);
});

add_test(function test_storing_an_oversize_64x64_icon() {
  //  in: 64x64 png, 10698 bytes.
  // optimized: yes
  checkFaviconDataConversion("favicon-big64.png", "image/png", 10698,
                             true, false, run_next_test);
});

add_test(function test_scaling_an_oversize_160x3_icon() {
  //  in: 160x3 jpg, 5095 bytes.
  // optimized: yes
  checkFaviconDataConversion("favicon-scale160x3.jpg", "image/jpeg", 5095,
                             true, false, run_next_test);
});

add_test(function test_scaling_an_oversize_3x160_icon() {
  //  in: 3x160 jpg, 5059 bytes.
  // optimized: yes
  checkFaviconDataConversion("favicon-scale3x160.jpg", "image/jpeg", 5059,
                             true, false, run_next_test);
});
