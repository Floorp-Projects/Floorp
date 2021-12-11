/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/";
const URL_COPY = URL + "#copy";

const { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");

/**
 * These tests ensure that the thumbnail storage is working as intended.
 * Newly captured thumbnails should be saved as files and they should as well
 * be removed when the user sanitizes their history.
 */
add_task(async function thumbnails_storage() {
  dontExpireThumbnailURLs([URL, URL_COPY]);

  await promiseClearHistory();
  await promiseAddVisitsAndRepopulateNewTabLinks(URL);
  await promiseCreateThumbnail();

  // Make sure Storage.copy() updates an existing file.
  await PageThumbsStorage.copy(URL, URL_COPY);
  let copy = new FileUtils.File(
    PageThumbsStorageService.getFilePathForURL(URL_COPY)
  );
  let mtime = (copy.lastModifiedTime -= 60);

  await PageThumbsStorage.copy(URL, URL_COPY);
  isnot(
    new FileUtils.File(PageThumbsStorageService.getFilePathForURL(URL_COPY))
      .lastModifiedTime,
    mtime,
    "thumbnail file was updated"
  );

  let file = new FileUtils.File(
    PageThumbsStorageService.getFilePathForURL(URL)
  );
  let fileCopy = new FileUtils.File(
    PageThumbsStorageService.getFilePathForURL(URL_COPY)
  );

  // Clear the browser history. Retry until the files are gone because Windows
  // locks them sometimes.
  info("Clearing history");
  while (file.exists() || fileCopy.exists()) {
    await promiseClearHistory();
  }
  info("History is clear");

  info("Repopulating");
  await promiseAddVisitsAndRepopulateNewTabLinks(URL);
  await promiseCreateThumbnail();

  info("Clearing the last 10 minutes of browsing history");
  // Clear the last 10 minutes of browsing history.
  await promiseClearHistory(true);

  info("Attempt to clear file");
  // Retry until the file is gone because Windows locks it sometimes.
  await promiseClearFile(file, URL);

  info("Done");
});

async function promiseClearFile(aFile, aURL) {
  if (!aFile.exists()) {
    return undefined;
  }
  // Re-add our URL to the history so that history observer's onDeleteURI()
  // is called again.
  await PlacesTestUtils.addVisits(makeURI(aURL));
  await promiseClearHistory(true);
  // Then retry.
  return promiseClearFile(aFile, aURL);
}

function promiseClearHistory(aUseRange) {
  let options = {};
  if (aUseRange) {
    let usec = Date.now() * 1000;
    options.range = [usec - 10 * 60 * 1000 * 1000, usec];
    options.ignoreTimespan = false;
  }
  return Sanitizer.sanitize(["history"], options);
}

async function promiseCreateThumbnail() {
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
}
