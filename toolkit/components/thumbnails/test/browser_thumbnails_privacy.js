/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_DISK_CACHE_SSL = "browser.cache.disk_cache_ssl";
const URL =
  "://example.com/browser/toolkit/components/thumbnails/" +
  "test/privacy_cache_control.sjs";

add_task(async function thumbnails_privacy() {
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref(PREF_DISK_CACHE_SSL);
  });

  let positive = [
    // A normal HTTP page without any Cache-Control header.
    { scheme: "http", cacheControl: null, diskCacheSSL: false },

    // A normal HTTP page with 'Cache-Control: private'.
    { scheme: "http", cacheControl: "private", diskCacheSSL: false },

    // Capture HTTPS pages if browser.cache.disk_cache_ssl == true.
    { scheme: "https", cacheControl: null, diskCacheSSL: true },
    { scheme: "https", cacheControl: "public", diskCacheSSL: true },
    { scheme: "https", cacheControl: "private", diskCacheSSL: true },
  ];

  let negative = [
    // Never capture pages with 'Cache-Control: no-store'.
    { scheme: "http", cacheControl: "no-store", diskCacheSSL: false },
    { scheme: "http", cacheControl: "no-store", diskCacheSSL: true },
    { scheme: "https", cacheControl: "no-store", diskCacheSSL: false },
    { scheme: "https", cacheControl: "no-store", diskCacheSSL: true },

    // Don't capture HTTPS pages by default.
    { scheme: "https", cacheControl: null, diskCacheSSL: false },
    { scheme: "https", cacheControl: "public", diskCacheSSL: false },
    { scheme: "https", cacheControl: "private", diskCacheSSL: false },
  ];

  await checkCombinations(positive, true);
  await checkCombinations(negative, false);
});

async function checkCombinations(aCombinations, aResult) {
  for (let combination of aCombinations) {
    let url = combination.scheme + URL;
    if (combination.cacheControl) {
      url += "?" + combination.cacheControl;
    }

    await SpecialPowers.pushPrefEnv({
      set: [[PREF_DISK_CACHE_SSL, combination.diskCacheSSL]],
    });

    // Add the test page as a top link so it has a chance to be thumbnailed
    await promiseAddVisitsAndRepopulateNewTabLinks(url);

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url,
      },
      async browser => {
        let msg = JSON.stringify(combination) + " == " + aResult;
        let aIsSafeSite = await PageThumbs.shouldStoreThumbnail(browser);
        Assert.equal(aIsSafeSite, aResult, msg);
      }
    );

    await SpecialPowers.popPrefEnv();
  }
}
