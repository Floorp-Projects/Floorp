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
function runTests() {
  dontExpireThumbnailURLs([URL, URL_COPY]);

  yield clearHistory();
  yield addVisitsAndRepopulateNewTabLinks(URL, next);
  yield createThumbnail();

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
  while (file.exists() || fileCopy.exists()) {
    yield clearHistory();
  }

  yield addVisitsAndRepopulateNewTabLinks(URL, next);
  yield createThumbnail();

  // Clear the last 10 minutes of browsing history.
  yield clearHistory(true);

  // Retry until the file is gone because Windows locks it sometimes.
  clearFile(file, URL);
}

function clearFile(aFile, aURL) {
  if (aFile.exists())
    // Re-add our URL to the history so that history observer's onDeleteURI()
    // is called again.
    addVisits(makeURI(aURL), function() {
      // Try again...
      yield clearHistory(true);
      clearFile(aFile, aURL);
    });
}

function clearHistory(aUseRange) {
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

  s.sanitize();
  s.range = null;
  s.ignoreTimespan = true;

  executeSoon(next);
}

function createThumbnail() {
  addTab(URL, function () {
    whenFileExists(URL, function () {
      gBrowser.removeTab(gBrowser.selectedTab);
      next();
    });
  });
}
