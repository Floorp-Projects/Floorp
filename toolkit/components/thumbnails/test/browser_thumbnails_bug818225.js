/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL =
  "http://mochi.test:8888/browser/toolkit/components/thumbnails/" +
  "test/background_red.html?" +
  Date.now();

// Test PageThumbs API function getThumbnailPath
add_task(async function thumbnails_bg_bug818225() {
  let path = PageThumbs.getThumbnailPath(URL);
  await testIfExists(path, false, "Thumbnail file does not exist");
  await promiseAddVisitsAndRepopulateNewTabLinks(URL);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: URL,
    },
    async browser => {
      gBrowserThumbnails.clearTopSiteURLCache();
      await whenFileExists(URL);
    }
  );

  path = PageThumbs.getThumbnailPath(URL);
  let expectedPath = PageThumbsStorageService.getFilePathForURL(URL);
  is(path, expectedPath, "Thumbnail file has correct path");

  await testIfExists(path, true, "Thumbnail file exists");
});

function testIfExists(aPath, aExpected, aMessage) {
  return IOUtils.exists(aPath).then(
    function onSuccess(exists) {
      is(exists, aExpected, aMessage);
    },
    function onFailure(error) {
      ok(false, `IOUtils.exists() failed ${error}`);
    }
  );
}
