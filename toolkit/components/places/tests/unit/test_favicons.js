/* Any copyright is dedicated to the Public Domain. http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for nsIFaviconService.

// The pixel values we get in Windows are sometimes +/- 1 value compared to
// other platforms, so we need to compare against a different set of reference
// images.
let isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

/**
 * setAndGetFaviconData()
 *
 * Calls setFaviconData() with the specified image data,
 * and then retrieves it with getFaviconData().
 *
 * @return an array: [bytes, MIME type].
 */
function setAndGetFaviconData(aFilename, aData, aMimeType) {
  let iconsvc = PlacesUtils.favicons;
  let iconURI = NetUtil.newURI("http://places.test/" + aFilename);
  try {
    iconsvc.setFaviconData(iconURI, aData, aData.length, aMimeType,
                           Number.MAX_VALUE);
  } catch (ex) {}
  let dataURL = iconsvc.getFaviconDataAsDataURL(iconURI);
  try {
    iconsvc.setFaviconDataFromDataURL(iconURI, dataURL, Number.MAX_VALUE);
  } catch (ex) {}
  let mimeTypeOutparam = {};

  let outData = iconsvc.getFaviconData(iconURI, mimeTypeOutparam);

  return [outData, mimeTypeOutparam.value];
}

/**
 * Retrieve and read a file, verifying the length of the returned data.
 *
 * @return the array of bytes read from the file.
 */
function readFileOfLength(name, expected) {
  let file = do_get_file(name);
  let data = readFileData(file);
  do_check_eq(data.length, expected);
  return data;
}

/**
 * Load a favicon file, verifying expected length.
 *
 * @return the loaded data to allow composing more complex checks.
 */
function setAndGetFaviconDataFromName(iconName, inMimeType, expectedLength) {
  let inData = readFileOfLength(iconName, expectedLength);
  let [outData, outMimeType] = setAndGetFaviconData(iconName, inData, inMimeType);
  return [inData, outData, outMimeType];
}

/**
 * Load a non-oversized favicon file, verifying expected contents and length.
 */
function check_icon_data(iconName, inMimeType, expectedLength) {
  let [inData, outData, outMimeType] =
    setAndGetFaviconDataFromName(iconName, inMimeType, expectedLength);

  // Ensure input and output are identical.
  do_check_eq(inMimeType, outMimeType);
  do_check_true(compareArrays(inData, outData));
}

/**
 * Load an oversized favicon file, verifying expected contents and lengths
 * against a translated PNG file.
 * If skipContent is true, the expected and output data are not compared.
 */
function check_oversized_icon_data(iconName, inMimeType, expectedLength, skipContent) {
  let [inData, outData, outMimeType] =
    setAndGetFaviconDataFromName(iconName, inMimeType, expectedLength);

  // Read in the expected output.
  let expectedFile = do_get_file("expected-" + iconName + ".png");
  let expectedData = readFileData(expectedFile);

  // Compare thet expected data to the actual data.
  do_check_eq("image/png", outMimeType);
  if (!skipContent) {
    do_check_true(compareArrays(expectedData, outData));
  }
}

/*
 * Done with utilities! On to the tests.
 */
function run_test() {
  run_next_test();
}

add_test(function test_storing_a_normal_16x16_icon() {
  // 16x16 png, 286 bytes.
  let iconName   = "favicon-normal16.png";
  let inMimeType = "image/png";
  check_icon_data(iconName, inMimeType, 286);
  run_next_test();
});

add_test(function test_storing_a_normal_32x32_icon() {
  // 32x32 png, 344 bytes.
  let iconName   = "favicon-normal32.png";
  let inMimeType = "image/png";
  check_icon_data(iconName, inMimeType, 344);
  run_next_test();
});

add_test(function test_storing_an_oversize_16x16_icon() {
  //  in: 16x16 ico, 1406 bytes.
  // out: 16x16 png
  let iconName   = "favicon-big16.ico";
  let inMimeType = "image/x-icon";
  check_oversized_icon_data(iconName, inMimeType, 1406);
  run_next_test();
});

add_test(function test_storing_an_oversize_4x4_icon() {
  //  in: 4x4 jpg, 4751 bytes.
  // out: 16x16 png
  let iconName   = "favicon-big4.jpg";
  let inMimeType = "image/jpeg";
  check_oversized_icon_data(iconName, inMimeType, 4751);
  run_next_test();
});

add_test(function test_storing_an_oversize_32x32_icon() {
  //  in: 32x32 jpg, 3494 bytes.
  // out: 16x16 png
  let iconName   = "favicon-big32.jpg";
  let inMimeType = "image/jpeg";

  // Content check disabled on Windows due to problems with pixels varying
  // slightly.
  check_oversized_icon_data(iconName, inMimeType, 3494, isWindows);
  run_next_test();
});

add_test(function test_storing_an_oversize_48x48_icon() {
  //  in: 48x48 ico, 56646 bytes.
  // (howstuffworks.com icon, contains 13 icons with sizes from 16x16 to
  // 48x48 in varying depths)
  // out: 16x16 png
  let iconName   = "favicon-big48.ico";
  let inMimeType = "image/x-icon";
  check_oversized_icon_data(iconName, inMimeType, 56646);
  run_next_test();
});

add_test(function test_storing_an_oversize_64x64_icon() {
  //  in: 64x64 png, 10698 bytes.
  // out: 16x16 png
  let iconName   = "favicon-big64.png";
  let inMimeType = "image/png";
  check_oversized_icon_data(iconName, inMimeType, 10698);
  run_next_test();
});

add_test(function test_scaling_an_oversize_160x3_icon() {
  //  in: 160x3 jpg, 5095 bytes.
  // out: 16x16 png
  let iconName   = "favicon-scale160x3.jpg";
  let inMimeType = "image/jpeg";
  check_oversized_icon_data(iconName, inMimeType, 5095);
  run_next_test();
});

add_test(function test_scaling_an_oversize_3x160_icon() {
  //  in: 3x160 jpg, 5059 bytes.
  // out: 16x16 png
  let iconName = "favicon-scale3x160.jpg";
  let inMimeType = "image/jpeg";
  check_oversized_icon_data(iconName, inMimeType, 5059);
  run_next_test();
});

/*
 * The following few tests are asynchronous but share state. We bundle that
 * state into two arrays: `icons` and `pages`.
 * The tests are in four parts:
 *
 * 1. setup, where we add some history visits, and set an icon for one of them;
 * 2. getFaviconURLForPage, where we test synchronous retrieval;
 * 3. second_and_third, where we add icons for the remaining two pages, and test
 *    them synchronously;
 * 4. getFaviconDataForPage, which tests asynchronous retrieval.
 */
let icons = [
  {
    name: "favicon-normal32.png",
    mime: "image/png",
    data: null,
    uri:  NetUtil.newURI("file:///./favicon-normal32.png")
  },
  {
    name: "favicon-normal16.png",
    mime: "image/png",
    data: null,
    uri:  NetUtil.newURI("file:///./favicon-normal16.png")
  }
];

let pages = [
  NetUtil.newURI("http://foo.bar/"),
  NetUtil.newURI("http://bar.foo/"),
  NetUtil.newURI("http://foo.bar.moz/")
];

add_test(function test_set_and_get_favicon_setup() {
  do_log_info("Setup code for set/get favicon.");
  let [icon0, icon1] = icons;

  // 32x32 png, 344 bytes.
  icon0.data = readFileOfLength(icon0.name, 344);

  // 16x16 png, 286 bytes.
  icon1.data = readFileOfLength(icon1.name, 286);

  // Add visits to the DB.
  for each (let uri in pages) {
    PlacesUtils.history.addVisit(uri, Date.now() * 1000, null,
                                 PlacesUtils.history.TRANSITION_TYPED,
                                 false, 0);
  }

  // Set first page icon.
  try {
    PlacesUtils.favicons.setFaviconData(icon0.uri, icon0.data, icon0.data.length,
                                        icon0.mime, Number.MAX_VALUE);
  } catch (ex) {
    do_throw("Failure setting first page icon: " + ex);
  }
  PlacesUtils.favicons.setFaviconUrlForPage(pages[0], icon0.uri);
  do_check_guid_for_uri(pages[0]);

  let favicon = PlacesUtils.favicons.getFaviconForPage(pages[0]);
  do_check_true(icon0.uri.equals(favicon));

  run_next_test();
});

add_test(function test_set_and_get_favicon_getFaviconURLForPage() {
  let [icon0] = icons;
  PlacesUtils.favicons.getFaviconURLForPage(pages[0], {
      onFaviconDataAvailable: function(aURI, aDataLen, aData, aMimeType) {
        do_check_true(icon0.uri.equals(aURI));
        do_check_eq(aDataLen, 0);
        do_check_eq(aData.length, 0);
        do_check_eq(aMimeType, "");
        run_next_test();
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIFaviconDataCallback])
  });
});

add_test(function test_set_and_get_favicon_second_and_third() {
  let [icon0, icon1] = icons;
  try {
    PlacesUtils.favicons.setFaviconData(icon1.uri, icon1.data, icon1.data.length,
                                        icon1.mime, Number.MAX_VALUE);
  } catch (ex) {
    do_throw("Failure setting second page icon: " + ex);
  }
  PlacesUtils.favicons.setFaviconUrlForPage(pages[1], icon1.uri);
  do_check_guid_for_uri(pages[1]);
  do_check_true(icon1.uri.equals(PlacesUtils.favicons.getFaviconForPage(pages[1])));

  // Set third page icon as the same as first page one.
  try {
    PlacesUtils.favicons.setFaviconData(icon0.uri, icon0.data, icon0.data.length,
                                        icon0.mime, Number.MAX_VALUE);
  } catch (ex) {
    do_throw("Failure setting third page icon: " + ex);
  }
  PlacesUtils.favicons.setFaviconUrlForPage(pages[2], icon0.uri);
  do_check_guid_for_uri(pages[2]);
  let page3favicon = PlacesUtils.favicons.getFaviconForPage(pages[2]);
  do_check_true(icon0.uri.equals(page3favicon));

  // Check first page icon.
  let out1MimeType = {};
  let out1Data = PlacesUtils.favicons.getFaviconData(icon0.uri, out1MimeType);
  do_check_eq(icon0.mime, out1MimeType.value);
  do_check_true(compareArrays(icon0.data, out1Data));

  // Check second page icon.
  let out2MimeType = {};
  let out2Data = PlacesUtils.favicons.getFaviconData(icon1.uri, out2MimeType);
  do_check_eq(icon1.mime, out2MimeType.value);
  do_check_true(compareArrays(icon1.data, out2Data));

  // Check third page icon.
  let out3MimeType = {};
  let out3Data = PlacesUtils.favicons.getFaviconData(page3favicon, out3MimeType);
  do_check_eq(icon0.mime, out3MimeType.value);
  do_check_true(compareArrays(icon0.data, out3Data));
  run_next_test();
});

add_test(function test_set_and_get_favicon_getFaviconDataForPage() {
  let [icon0] = icons;
  PlacesUtils.favicons.getFaviconDataForPage(pages[0], {
      onFaviconDataAvailable: function(aURI, aDataLen, aData, aMimeType) {
        do_check_true(aURI.equals(icon0.uri));
        do_check_eq(icon0.mime, icon0.mime);
        do_check_true(compareArrays(icon0.data, aData));
        do_check_eq(aDataLen, aData.length);
        run_next_test();
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIFaviconDataCallback])
    });
});

add_test(function test_favicon_links() {
  let pageURI = NetUtil.newURI("http://foo.bar/");
  let faviconURI = NetUtil.newURI("file:///./favicon-normal32.png");
  do_check_eq(PlacesUtils.favicons.getFaviconImageForPage(pageURI).spec,
              PlacesUtils.favicons.getFaviconLinkForIcon(faviconURI).spec);
  run_next_test();
});

add_test(function test_failed_favicon_cache() {
  // 32x32 png, 344 bytes.
  let iconName = "favicon-normal32.png";
  let faviconURI = NetUtil.newURI("file:///./" + iconName);

  PlacesUtils.favicons.addFailedFavicon(faviconURI);
  do_check_true(PlacesUtils.favicons.isFailedFavicon(faviconURI));
  PlacesUtils.favicons.removeFailedFavicon(faviconURI);
  do_check_false(PlacesUtils.favicons.isFailedFavicon(faviconURI));
  run_next_test();
});

add_test(function test_getFaviconData_on_the_default_favicon() {
  let icon = PlacesUtils.favicons.defaultFavicon;
  let outMimeType = {};
  let outData = PlacesUtils.favicons.getFaviconData(icon, outMimeType);
  do_check_eq(outMimeType.value, "image/png");

  // Read in the icon and compare it to what the API returned above.
  let istream = NetUtil.newChannel(PlacesUtils.favicons.defaultFavicon).open();
  let expectedData = readInputStreamData(istream);
  do_check_true(compareArrays(outData, expectedData));
  run_next_test();
});
