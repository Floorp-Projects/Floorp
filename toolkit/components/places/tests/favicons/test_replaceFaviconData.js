/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for mozIAsyncFavicons::replaceFaviconData()
 */

var iconsvc = PlacesUtils.favicons;
var histsvc = PlacesUtils.history;
var systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

var originalFavicon = {
  file: do_get_file("favicon-normal16.png"),
  uri: uri(do_get_file("favicon-normal16.png")),
  data: readFileData(do_get_file("favicon-normal16.png")),
  mimetype: "image/png"
};

var uniqueFaviconId = 0;
function createFavicon(fileName) {
  let tempdir = Services.dirsvc.get("TmpD", Ci.nsILocalFile);

  // remove any existing file at the path we're about to copy to
  let outfile = tempdir.clone();
  outfile.append(fileName);
  try { outfile.remove(false); } catch (e) {}

  originalFavicon.file.copyToFollowingLinks(tempdir, fileName);

  let stream = Cc["@mozilla.org/network/file-output-stream;1"]
    .createInstance(Ci.nsIFileOutputStream);
  stream.init(outfile, 0x02 | 0x08 | 0x10, 0o600, 0);

  // append some data that sniffers/encoders will ignore that will distinguish
  // the different favicons we'll create
  uniqueFaviconId++;
  let uniqueStr = "uid:" + uniqueFaviconId;
  stream.write(uniqueStr, uniqueStr.length);
  stream.close();

  do_check_eq(outfile.leafName.substr(0, fileName.length), fileName);

  return {
    file: outfile,
    uri: uri(outfile),
    data: readFileData(outfile),
    mimetype: "image/png"
  };
}

function checkCallbackSucceeded(callbackMimetype, callbackData, sourceMimetype, sourceData) {
  do_check_eq(callbackMimetype, sourceMimetype);
  do_check_true(compareArrays(callbackData, sourceData));
}

function run_test() {
  // check that the favicon loaded correctly
  do_check_eq(originalFavicon.data.length, 286);
  run_next_test();
}

add_task(function* test_replaceFaviconData_validHistoryURI() {
  do_print("test replaceFaviconData for valid history uri");

  let pageURI = uri("http://test1.bar/");
  yield PlacesTestUtils.addVisits(pageURI);

  let favicon = createFavicon("favicon1.png");

  iconsvc.replaceFaviconData(favicon.uri, favicon.data, favicon.data.length,
    favicon.mimetype);

  let deferSetAndFetchFavicon = Promise.defer();
  iconsvc.setAndFetchFaviconForPage(pageURI, favicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconData_validHistoryURI_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, favicon.mimetype, favicon.data);
      checkFaviconDataForPage(
        pageURI, favicon.mimetype, favicon.data,
        function test_replaceFaviconData_validHistoryURI_callback() {
          favicon.file.remove(false);
          deferSetAndFetchFavicon.resolve();
        });
    }, systemPrincipal);
  yield deferSetAndFetchFavicon.promise;

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_replaceFaviconData_overrideDefaultFavicon() {
  do_print("test replaceFaviconData to override a later setAndFetchFaviconForPage");

  let pageURI = uri("http://test2.bar/");
  yield PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon2.png");
  let secondFavicon = createFavicon("favicon3.png");

  iconsvc.replaceFaviconData(
    firstFavicon.uri, secondFavicon.data, secondFavicon.data.length,
    secondFavicon.mimetype);

  let deferSetAndFetchFavicon = Promise.defer();
  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconData_overrideDefaultFavicon_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconData_overrideDefaultFavicon_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          deferSetAndFetchFavicon.resolve();
        });
    }, systemPrincipal);
  yield deferSetAndFetchFavicon.promise;

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_replaceFaviconData_replaceExisting() {
  do_print("test replaceFaviconData to override a previous setAndFetchFaviconForPage");

  let pageURI = uri("http://test3.bar");
  yield PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon4.png");
  let secondFavicon = createFavicon("favicon5.png");

  let deferSetAndFetchFavicon = Promise.defer();
  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconData_replaceExisting_firstSet_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, firstFavicon.mimetype, firstFavicon.data);
      checkFaviconDataForPage(
        pageURI, firstFavicon.mimetype, firstFavicon.data,
        function test_replaceFaviconData_overrideDefaultFavicon_firstCallback() {
          iconsvc.replaceFaviconData(
            firstFavicon.uri, secondFavicon.data, secondFavicon.data.length,
            secondFavicon.mimetype);
          PlacesTestUtils.promiseAsyncUpdates().then(() => {
            checkFaviconDataForPage(
              pageURI, secondFavicon.mimetype, secondFavicon.data,
              function test_replaceFaviconData_overrideDefaultFavicon_secondCallback() {
                firstFavicon.file.remove(false);
                secondFavicon.file.remove(false);
                deferSetAndFetchFavicon.resolve();
              }, systemPrincipal);
          });
        });
    }, systemPrincipal);
  yield deferSetAndFetchFavicon.promise;

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_replaceFaviconData_unrelatedReplace() {
  do_print("test replaceFaviconData to not make unrelated changes");

  let pageURI = uri("http://test4.bar/");
  yield PlacesTestUtils.addVisits(pageURI);

  let favicon = createFavicon("favicon6.png");
  let unrelatedFavicon = createFavicon("favicon7.png");

  iconsvc.replaceFaviconData(
    unrelatedFavicon.uri, unrelatedFavicon.data, unrelatedFavicon.data.length,
    unrelatedFavicon.mimetype);

  let deferSetAndFetchFavicon = Promise.defer();
  iconsvc.setAndFetchFaviconForPage(
    pageURI, favicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconData_unrelatedReplace_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, favicon.mimetype, favicon.data);
      checkFaviconDataForPage(
        pageURI, favicon.mimetype, favicon.data,
        function test_replaceFaviconData_unrelatedReplace_callback() {
          favicon.file.remove(false);
          unrelatedFavicon.file.remove(false);
          deferSetAndFetchFavicon.resolve();
        });
    }, systemPrincipal);
  yield deferSetAndFetchFavicon.promise;

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_replaceFaviconData_badInputs() {
  do_print("test replaceFaviconData to throw on bad inputs");

  let favicon = createFavicon("favicon8.png");

  let ex = null;
  try {
    iconsvc.replaceFaviconData(
      favicon.uri, favicon.data, favicon.data.length, "");
  } catch (e) {
    ex = e;
  } finally {
    do_check_true(!!ex);
  }

  ex = null;
  try {
    iconsvc.replaceFaviconData(
      null, favicon.data, favicon.data.length, favicon.mimeType);
  } catch (e) {
    ex = e;
  } finally {
    do_check_true(!!ex);
  }

  ex = null;
  try {
    iconsvc.replaceFaviconData(
      favicon.uri, null, 0, favicon.mimeType);
  } catch (e) {
    ex = e;
  } finally {
    do_check_true(!!ex);
  }

  favicon.file.remove(false);

  yield PlacesTestUtils.clearHistory();
});

add_task(function* test_replaceFaviconData_twiceReplace() {
  do_print("test replaceFaviconData on multiple replacements");

  let pageURI = uri("http://test5.bar/");
  yield PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon9.png");
  let secondFavicon = createFavicon("favicon10.png");

  iconsvc.replaceFaviconData(
    firstFavicon.uri, firstFavicon.data, firstFavicon.data.length,
    firstFavicon.mimetype);
  iconsvc.replaceFaviconData(
    firstFavicon.uri, secondFavicon.data, secondFavicon.data.length,
    secondFavicon.mimetype);

  let deferSetAndFetchFavicon = Promise.defer();
  iconsvc.setAndFetchFaviconForPage(
    pageURI, firstFavicon.uri, true,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    function test_replaceFaviconData_twiceReplace_check(aURI, aDataLen, aData, aMimeType) {
      checkCallbackSucceeded(aMimeType, aData, secondFavicon.mimetype, secondFavicon.data);
      checkFaviconDataForPage(
        pageURI, secondFavicon.mimetype, secondFavicon.data,
        function test_replaceFaviconData_twiceReplace_callback() {
          firstFavicon.file.remove(false);
          secondFavicon.file.remove(false);
          deferSetAndFetchFavicon.resolve();
        }, systemPrincipal);
    }, systemPrincipal);
  yield deferSetAndFetchFavicon.promise;

  yield PlacesTestUtils.clearHistory();
});
