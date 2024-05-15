/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for setFaviconForPage()
 */

add_task(async function test_validHistoryURI() {
  let pageURI = uri("http://test1.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let favicon = await createFavicon("favicon1.png");

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI: favicon.uri,
    dataURL: await createDataURLForFavicon(favicon),
    expectedFaviconData: favicon.data,
    expectedFaviconMimeType: favicon.mimeType,
  });

  await IOUtils.remove(favicon.file.path);
  await PlacesUtils.history.clear();
});

add_task(async function test_overrideDefaultFavicon() {
  let pageURI = uri("http://test2.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = await createFavicon("favicon2.png");
  let secondFavicon = await createFavicon("favicon3.png");

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI: firstFavicon.uri,
    dataURL: await createDataURLForFavicon(secondFavicon),
    expectedFaviconData: secondFavicon.data,
    expectedFaviconMimeType: secondFavicon.mimeType,
  });

  await IOUtils.remove(firstFavicon.file.path);
  await IOUtils.remove(secondFavicon.file.path);
  await PlacesUtils.history.clear();
});

add_task(async function test_replaceExisting() {
  let pageURI = uri("http://test3.bar");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = await createFavicon("favicon4.png");
  let secondFavicon = await createFavicon("favicon5.png");

  await new Promise(resolve => {
    PlacesUtils.favicons.setAndFetchFaviconForPage(
      pageURI,
      firstFavicon.uri,
      true,
      PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
      function test_replaceExisting_firstSet_check(
        aURI,
        aDataLen,
        aData,
        aMimeType
      ) {
        Assert.equal(aMimeType, firstFavicon.mimeType);
        Assert.ok(compareArrays(aData, firstFavicon.data));
        checkFaviconDataForPage(
          pageURI,
          firstFavicon.mimeType,
          firstFavicon.data,
          resolve
        );
      },
      Services.scriptSecurityManager.getSystemPrincipal()
    );
  });

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI: firstFavicon.uri,
    dataURL: await createDataURLForFavicon(secondFavicon),
    expectedFaviconData: secondFavicon.data,
    expectedFaviconMimeType: secondFavicon.mimeType,
  });

  await IOUtils.remove(firstFavicon.file.path);
  await IOUtils.remove(secondFavicon.file.path);
  await PlacesUtils.history.clear();
});

add_task(async function test_twiceReplace() {
  let pageURI = uri("http://test5.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let firstFavicon = await createFavicon("favicon9.png");
  let secondFavicon = await createFavicon("favicon10.png");

  let firstFaviconDataURL = await createDataURLForFavicon(firstFavicon);
  await new Promise(resolve => {
    PlacesUtils.favicons.setFaviconForPage(
      pageURI,
      firstFavicon.uri,
      firstFaviconDataURL,
      null,
      resolve
    );
  });

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI: firstFavicon.uri,
    dataURL: await createDataURLForFavicon(secondFavicon),
    expectedFaviconData: secondFavicon.data,
    expectedFaviconMimeType: secondFavicon.mimeType,
  });

  await IOUtils.remove(firstFavicon.file.path);
  await IOUtils.remove(secondFavicon.file.path);
  await PlacesUtils.history.clear();
});

add_task(async function test_userpass() {
  let pageURI = uri("http://user:pass@test1.bar/");
  await PlacesTestUtils.addVisits(pageURI);

  let favicon = await createFavicon("favicon1.png");
  let faviconURI = uri("http://user:pass@test1.bar/favicon1.png");

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI,
    dataURL: await createDataURLForFavicon(favicon),
    expectedFaviconData: favicon.data,
    expectedFaviconMimeType: favicon.mimeType,
  });

  await IOUtils.remove(favicon.file.path);
  await PlacesUtils.history.clear();
});

add_task(async function test_svg() {
  let pageURI = uri("http://example.com/");
  await PlacesTestUtils.addVisits(pageURI);

  const svgContent = "<svg><rect width='1px' height='1px'/></svg>";

  await doTestSetFaviconForPage({
    pageURI,
    faviconURI: uri("http://example.com/favicon.svg"),
    dataURL: uri(`data:image/svg+xml;utf8,${svgContent}`),
    expectedFaviconData: Array.from(new TextEncoder().encode(svgContent)),
    expectedFaviconMimeType: "image/svg+xml",
  });

  await PlacesUtils.history.clear();
});

add_task(async function test_invalidPageURI() {
  await PlacesTestUtils.addVisits(uri("http://example.com/"));
  let favicon = await createFavicon("favicon-invalidPageURI.png");

  for (let invalidURI of [null, "", "http://example.com"]) {
    try {
      info(`Invalid page URI test for [${invalidURI}]`);
      PlacesUtils.favicons.setFaviconForPage(
        invalidURI,
        favicon.uri,
        await createDataURLForFavicon(favicon)
      );
      Assert.ok(false, "Error should happened");
    } catch (e) {
      Assert.ok(true, `Expected error happend [${e.message}]`);
    }
  }
});

add_task(async function test_invalidFaviconURI() {
  let pageURI = uri("http://example.com/");
  await PlacesTestUtils.addVisits(pageURI);
  let favicon = await createFavicon("favicon-invalidFaviconURI.png");

  for (let invalidURI of [null, "", favicon.uri.spec]) {
    try {
      info(`Invalid favicon URI test for [${invalidURI}]`);
      PlacesUtils.favicons.setFaviconForPage(
        pageURI,
        invalidURI,
        await createDataURLForFavicon(favicon)
      );
      Assert.ok(false, "Error should happened");
    } catch (e) {
      Assert.ok(true, `Expected error happend [${e.message}]`);
    }
  }
});

add_task(async function test_invalidFaviconDataURI() {
  let pageURI = uri("http://example.com/");
  await PlacesTestUtils.addVisits(pageURI);
  let faviconURI = uri("http://example.com/favicon.svg");

  for (let invalidURI of [
    null,
    "",
    "data:text/plain;base64,SGVsbG8sIFdvcmxkIQ==",
    // nsIFaviconService::MAX_FAVICON_BUFFER_SIZE = 65536
    uri(`data:image/svg+xml;utf8,<svg><text>${Array(65536)}</text></svg>`),
  ]) {
    try {
      info(`Invalid favicon data URI test for [${invalidURI}]`);
      PlacesUtils.favicons.setFaviconForPage(pageURI, faviconURI, invalidURI);
      Assert.ok(false, "Error should happened");
    } catch (e) {
      Assert.ok(true, `Expected error happend [${e.message}]`);
    }
  }
});

async function doTestSetFaviconForPage({
  pageURI,
  faviconURI,
  dataURL,
  expectedFaviconData,
  expectedFaviconMimeType,
}) {
  let result = await new Promise(resolve => {
    PlacesUtils.favicons.setFaviconForPage(
      pageURI,
      faviconURI,
      dataURL,
      null,
      resolve
    );
  });

  info("Check the result of setFaviconForPage");
  Assert.equal(result, 0);

  await new Promise(resolve => {
    checkFaviconDataForPage(
      pageURI,
      expectedFaviconMimeType,
      expectedFaviconData,
      resolve
    );
  });
}
