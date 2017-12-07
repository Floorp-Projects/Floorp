/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_DISK_CACHE_SSL = "browser.cache.disk_cache_ssl";
const URL = "://example.com/browser/toolkit/components/thumbnails/" +
            "test/privacy_cache_control.sjs";

function* runTests() {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF_DISK_CACHE_SSL);
  });

  let positive = [
    // A normal HTTP page without any Cache-Control header.
    {scheme: "http", cacheControl: null, diskCacheSSL: false},

    // A normal HTTP page with 'Cache-Control: private'.
    {scheme: "http", cacheControl: "private", diskCacheSSL: false},

    // Capture HTTPS pages if browser.cache.disk_cache_ssl == true.
    {scheme: "https", cacheControl: null, diskCacheSSL: true},
    {scheme: "https", cacheControl: "public", diskCacheSSL: true},
    {scheme: "https", cacheControl: "private", diskCacheSSL: true}
  ];

  let negative = [
    // Never capture pages with 'Cache-Control: no-store'.
    {scheme: "http", cacheControl: "no-store", diskCacheSSL: false},
    {scheme: "http", cacheControl: "no-store", diskCacheSSL: true},
    {scheme: "https", cacheControl: "no-store", diskCacheSSL: false},
    {scheme: "https", cacheControl: "no-store", diskCacheSSL: true},

    // Don't capture HTTPS pages by default.
    {scheme: "https", cacheControl: null, diskCacheSSL: false},
    {scheme: "https", cacheControl: "public", diskCacheSSL: false},
    {scheme: "https", cacheControl: "private", diskCacheSSL: false}
  ];

  yield checkCombinations(positive, true);
  yield checkCombinations(negative, false);
}

function checkCombinations(aCombinations, aResult) {
  let combi = aCombinations.shift();
  if (!combi) {
    next();
    return;
  }

  let url = combi.scheme + URL;
  if (combi.cacheControl)
    url += "?" + combi.cacheControl;
  Services.prefs.setBoolPref(PREF_DISK_CACHE_SSL, combi.diskCacheSSL);

  // Add the test page as a top link so it has a chance to be thumbnailed
  addVisitsAndRepopulateNewTabLinks(url, _ => {
    testCombination(combi, url, aCombinations, aResult);
  });
}

function testCombination(combi, url, aCombinations, aResult) {
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
  let browser = gBrowser.selectedBrowser;

  whenLoaded(browser, () => {
    let msg = JSON.stringify(combi) + " == " + aResult;
    PageThumbs.shouldStoreThumbnail(browser, (aIsSafeSite) => {
      is(aIsSafeSite, aResult, msg);
      gBrowser.removeTab(tab);
      // Continue with the next combination.
      checkCombinations(aCombinations, aResult);
    });
  });
}
