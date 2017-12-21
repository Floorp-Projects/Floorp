/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests support for icons with multiple frames (like .ico files).
 */

add_task(async function() {
  //  in: 48x48 ico, 56646 bytes.
  // (howstuffworks.com icon, contains 13 icons with sizes from 16x16 to
  // 48x48 in varying depths)
  let pageURI = NetUtil.newURI("http://places.test/page/");
  await PlacesTestUtils.addVisits(pageURI);
  let faviconURI = NetUtil.newURI("http://places.test/icon/favicon-multi.ico");
  // Fake window.
  let win = { devicePixelRatio: 1.0 };
  let icoData = readFileData(do_get_file("favicon-multi.ico"));
  PlacesUtils.favicons.replaceFaviconData(faviconURI, icoData, icoData.length,
                                          "image/x-icon");
  await setFaviconForPage(pageURI, faviconURI);

  for (let size of [16, 32, 64]) {
    let file = do_get_file(`favicon-multi-frame${size}.png`);
    let data = readFileData(file);

    info("Check getFaviconDataForPage");
    let icon = await getFaviconDataForPage(pageURI, size);
    Assert.equal(icon.mimeType, "image/png");
    Assert.deepEqual(icon.data, data);

    info("Check moz-anno:favicon protocol");
    await compareFavicons(
      Services.io.newFileURI(file),
      PlacesUtils.urlWithSizeRef(win, PlacesUtils.favicons.getFaviconLinkForIcon(faviconURI).spec, size)
    );

    info("Check page-icon protocol");
    await compareFavicons(
      Services.io.newFileURI(file),
      PlacesUtils.urlWithSizeRef(win, "page-icon:" + pageURI.spec, size)
    );
  }
});
