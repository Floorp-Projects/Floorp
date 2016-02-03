/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const URL = "http://mochi.test:8888/";
const URL_COPY = URL + "#copy";

XPCOMUtils.defineLazyGetter(this, "Sanitizer", function () {
  let tmp = {};
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript("chrome://browser/content/sanitize.js", tmp);
  return tmp.Sanitizer;
});

/**
 * These tests ensure that the thumbnail storage is working as intended.
 * Newly captured thumbnails should be saved as files and they should as well
 * be removed when the user sanitizes their history.
 */
function* runTests() {
  yield Task.spawn(function*() {
    dontExpireThumbnailURLs([URL, URL_COPY]);

    yield promiseClearHistory();
    yield promiseAddVisitsAndRepopulateNewTabLinks(URL);
    yield promiseCreateThumbnail();

    // Make sure Storage.copy() updates an existing file.
    yield PageThumbsStorage.copy(URL, URL_COPY);
    let copy = new FileUtils.File(PageThumbsStorage.getFilePathForURL(URL_COPY));
    let mtime = copy.lastModifiedTime -= 60;

    yield PageThumbsStorage.copy(URL, URL_COPY);
    isnot(new FileUtils.File(PageThumbsStorage.getFilePathForURL(URL_COPY)).lastModifiedTime, mtime,
          "thumbnail file was updated");

    let file = new FileUtils.File(PageThumbsStorage.getFilePathForURL(URL));
    let fileCopy = new FileUtils.File(PageThumbsStorage.getFilePathForURL(URL_COPY));

    // Clear the browser history. Retry until the files are gone because Windows
    // locks them sometimes.
    info("Clearing history");
    while (file.exists() || fileCopy.exists()) {
      yield promiseClearHistory();
    }
    info("History is clear");

    info("Repopulating");
    yield promiseAddVisitsAndRepopulateNewTabLinks(URL);
    yield promiseCreateThumbnail();

    info("Clearing the last 10 minutes of browsing history");
    // Clear the last 10 minutes of browsing history.
    yield promiseClearHistory(true);

    info("Attempt to clear file");
    // Retry until the file is gone because Windows locks it sometimes.
    yield promiseClearFile(file, URL);

    info("Done");
  });
}

var promiseClearFile = Task.async(function*(aFile, aURL) {
  if (!aFile.exists()) {
    return undefined;
  }
  // Re-add our URL to the history so that history observer's onDeleteURI()
  // is called again.
  yield PlacesTestUtils.addVisits(makeURI(aURL));
  yield promiseClearHistory(true);
  // Then retry.
  return promiseClearFile(aFile, aURL);
});

function promiseClearHistory(aUseRange) {
  let s = new Sanitizer();
  s.prefDomain = "privacy.cpd.";

  let prefs = gPrefService.getBranch(s.prefDomain);
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
    addTab(URL, function () {
      whenFileExists(URL, function () {
        gBrowser.removeTab(gBrowser.selectedTab);
        resolve();
      });
    });
  });
}
