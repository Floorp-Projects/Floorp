/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/";
const URL_COPY = URL + "#copy";

XPCOMUtils.defineLazyGetter(this, "Sanitizer", function() {
  let tmp = {};
  Services.scriptloader.loadSubScript("chrome://browser/content/sanitize.js", tmp);
  return tmp.Sanitizer;
});

/**
 * These tests ensure that the thumbnail storage is working as intended.
 * Newly captured thumbnails should be saved as files and they should as well
 * be removed when the user sanitizes their history.
 */
function* runTests() {
  yield (async function() {
    dontExpireThumbnailURLs([URL, URL_COPY]);

    await promiseClearHistory();
    await promiseAddVisitsAndRepopulateNewTabLinks(URL);
    await promiseCreateThumbnail();

    // Make sure Storage.copy() updates an existing file.
    await PageThumbsStorage.copy(URL, URL_COPY);
    let copy = new FileUtils.File(PageThumbsStorageService.getFilePathForURL(URL_COPY));
    let mtime = copy.lastModifiedTime -= 60;

    await PageThumbsStorage.copy(URL, URL_COPY);
    isnot(new FileUtils.File(PageThumbsStorageService.getFilePathForURL(URL_COPY)).lastModifiedTime, mtime,
          "thumbnail file was updated");

    let file = new FileUtils.File(PageThumbsStorageService.getFilePathForURL(URL));
    let fileCopy = new FileUtils.File(PageThumbsStorageService.getFilePathForURL(URL_COPY));

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
  })();
}

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
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";

  let prefs = Services.prefs.getBranch(s.prefDomain);
  prefs.setBoolPref("history", true);
  prefs.setBoolPref("downloads", false);
  prefs.setBoolPref("cache", false);
  prefs.setBoolPref("cookies", false);
  prefs.setBoolPref("formdata", false);
  prefs.setBoolPref("offlineApps", false);
  prefs.setBoolPref("passwords", false);
  prefs.setBoolPref("sessions", false);
  prefs.setBoolPref("siteSettings", false);

  if (aUseRange) {
    let usec = Date.now() * 1000;
    s.range = [usec - 10 * 60 * 1000 * 1000, usec];
    s.ignoreTimespan = false;
  }

  return s.sanitize().then(() => {
    s.range = null;
    s.ignoreTimespan = true;
  });
}

function promiseCreateThumbnail() {
  return new Promise(resolve => {
    addTab(URL, function() {
      gBrowserThumbnails.clearTopSiteURLCache();
      whenFileExists(URL, function() {
        gBrowser.removeTab(gBrowser.selectedTab);
        resolve();
      });
    });
  });
}
