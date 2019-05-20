/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for replaceFaviconData()
 */

var iconsvc = PlacesUtils.favicons;

var originalFavicon = {
  file: do_get_file("favicon-normal16.png"),
  uri: uri(do_get_file("favicon-normal16.png")),
  data: readFileData(do_get_file("favicon-normal16.png")),
  mimetype: "image/png",
};

var uniqueFaviconId = 0;
function createFavicon(fileName) {
  let tempdir = Services.dirsvc.get("TmpD", Ci.nsIFile);

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

  Assert.equal(outfile.leafName.substr(0, fileName.length), fileName);

  return {
    file: outfile,
    uri: uri(outfile),
    data: readFileData(outfile),
    mimetype: "image/png",
  };
}

function checkCallbackSucceeded(callbackMimetype, callbackData, sourceMimetype, sourceData) {
  Assert.equal(callbackMimetype, sourceMimetype);
  Assert.ok(compareArrays(callbackData, sourceData));
}

function run_test() {
  // check that the favicon loaded correctly
  Assert.equal(originalFavicon.data.length, 286);
  run_next_test();
}

add_task(async function test_replaceFaviconData_validHistoryURI() {
  info("test replaceFaviconData for valid history uri");

  let pageURI = uri("http://test1.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let favicon = createFavicon("favicon1.png");

  iconsvc.replaceFaviconData(favicon.uri, favicon.data, favicon.mimetype);

  await new Promise(resolve => {
    iconsvc.setAndFetchFaviconForPage(pageURI, favicon.uri, true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      function test_replaceFaviconData_validHistoryURI_check(aURI, aDataLen, aData, aMimeType) {
  dump("GOT " + aMimeType + "\n");
        checkCallbackSucceeded(aMimeType, aData, favicon.mimetype, favicon.data);
        checkFaviconDataForPage(
          pageURI, favicon.mimetype, favicon.data,
          function test_replaceFaviconData_validHistoryURI_callback() {
            favicon.file.remove(false);
            resolve();
          });
      }, systemPrincipal);
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_overrideDefaultFavicon() {
  info("test replaceFaviconData to override a later setAndFetchFaviconForPage");

  let pageURI = uri("http://test2.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon2.png");
  let secondFavicon = createFavicon("favicon3.png");

  iconsvc.replaceFaviconData(
    firstFavicon.uri, secondFavicon.data, secondFavicon.mimetype);

  await new Promise(resolve => {
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
            resolve();
          });
      }, systemPrincipal);
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_replaceExisting() {
  info("test replaceFaviconData to override a previous setAndFetchFaviconForPage");

  let pageURI = uri("http://test3.bar");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon4.png");
  let secondFavicon = createFavicon("favicon5.png");

  await new Promise(resolve => {
    iconsvc.setAndFetchFaviconForPage(
      pageURI, firstFavicon.uri, true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      function test_replaceFaviconData_replaceExisting_firstSet_check(aURI, aDataLen, aData, aMimeType) {
        checkCallbackSucceeded(aMimeType, aData, firstFavicon.mimetype, firstFavicon.data);
        checkFaviconDataForPage(
          pageURI, firstFavicon.mimetype, firstFavicon.data,
          function test_replaceFaviconData_overrideDefaultFavicon_firstCallback() {
            iconsvc.replaceFaviconData(
              firstFavicon.uri, secondFavicon.data, secondFavicon.mimetype);
            PlacesTestUtils.promiseAsyncUpdates().then(() => {
              checkFaviconDataForPage(
                pageURI, secondFavicon.mimetype, secondFavicon.data,
                function test_replaceFaviconData_overrideDefaultFavicon_secondCallback() {
                  firstFavicon.file.remove(false);
                  secondFavicon.file.remove(false);
                  resolve();
                }, systemPrincipal);
            });
          });
      }, systemPrincipal);
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_unrelatedReplace() {
  info("test replaceFaviconData to not make unrelated changes");

  let pageURI = uri("http://test4.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let favicon = createFavicon("favicon6.png");
  let unrelatedFavicon = createFavicon("favicon7.png");

  iconsvc.replaceFaviconData(
    unrelatedFavicon.uri, unrelatedFavicon.data, unrelatedFavicon.mimetype);

  await new Promise(resolve => {
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
            resolve();
          });
      }, systemPrincipal);
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_badInputs() {
  info("test replaceFaviconData to throw on bad inputs");
  let icon = createFavicon("favicon8.png");

  Assert.throws(
    () => iconsvc.replaceFaviconData(icon.uri, icon.data, ""),
    /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(
    () => iconsvc.replaceFaviconData(icon.uri, icon.data, "not-an-image"),
    /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(
    () => iconsvc.replaceFaviconData(null, icon.data, icon.mimetype),
    /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(
    () => iconsvc.replaceFaviconData(icon.uri, [], icon.mimetype),
    /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(
    () => iconsvc.replaceFaviconData(icon.uri, null, icon.mimetype),
    /NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY/);

  icon.file.remove(false);
  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_twiceReplace() {
  info("test replaceFaviconData on multiple replacements");

  let pageURI = uri("http://test5.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = createFavicon("favicon9.png");
  let secondFavicon = createFavicon("favicon10.png");

  iconsvc.replaceFaviconData(
    firstFavicon.uri, firstFavicon.data, firstFavicon.mimetype);
  iconsvc.replaceFaviconData(
    firstFavicon.uri, secondFavicon.data, secondFavicon.mimetype);

  await new Promise(resolve => {
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
            resolve();
          }, systemPrincipal);
      }, systemPrincipal);
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_replaceFaviconData_rootOverwrite() {
  info("test replaceFaviconData doesn't overwrite root = 1");

  async function getRootValue(url) {
    let db = await PlacesUtils.promiseDBConnection();
    let rows = await db.execute("SELECT root FROM moz_icons WHERE icon_url = :url",
                                {url});
    return rows[0].getResultByName("root");
  }

  const PAGE_URL = "http://rootoverwrite.bar/";
  let pageURI = Services.io.newURI(PAGE_URL);
  const ICON_URL = "http://rootoverwrite.bar/favicon.ico";
  let iconURI = Services.io.newURI(ICON_URL);

  await PlacesTestUtils.addVisits(pageURI);

  let icon = createFavicon("favicon9.png");
  PlacesUtils.favicons.replaceFaviconData(iconURI, icon.data, icon.mimetype);
  await PlacesTestUtils.addFavicons(new Map([[PAGE_URL, ICON_URL]]));

  Assert.equal(await getRootValue(ICON_URL), 1, "Check root == 1");
  let icon2 = createFavicon("favicon10.png");
  PlacesUtils.favicons.replaceFaviconData(iconURI, icon2.data, icon2.mimetype);
  // replaceFaviconData doesn't have a callback, but we must wait its updated.
  await PlacesTestUtils.promiseAsyncUpdates();
  Assert.equal(await getRootValue(ICON_URL), 1, "Check root did not change");

  await PlacesUtils.history.clear();
});
