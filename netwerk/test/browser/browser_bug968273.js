/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function OpenCacheEntry(key, flags, lci) {
  return new Promise(resolve => {
    key = Services.io.newURI(key);
    function CacheListener() {}
    CacheListener.prototype = {
      QueryInterface: ChromeUtils.generateQI(["nsICacheEntryOpenCallback"]),

      onCacheEntryCheck() {
        return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
      },

      onCacheEntryAvailable(entry) {
        resolve(entry);
      },

      run() {
        let storage = Services.cache2.diskCacheStorage(lci);
        storage.asyncOpenURI(key, "", flags, this);
      },
    };

    new CacheListener().run();
  });
}

async function do_test_cache_persistent(https) {
  let scheme = https ? "https" : "http";
  let url =
    scheme + "://example.com/browser/netwerk/test/browser/file_bug968273.html";
  let redirectUrl =
    scheme +
    "://example.com/browser/netwerk/test/browser/bug968273_redirect.html";

  await BrowserTestUtils.openNewForegroundTab(gBrowser, url, true);

  let loadContextInfo = Services.loadContextInfo.custom(false, {
    partitionKey: `(${scheme},example.com)`,
  });

  let entry = await OpenCacheEntry(
    redirectUrl,
    Ci.nsICacheStorage.OPEN_NORMALLY,
    loadContextInfo
  );

  Assert.ok(
    entry.persistent == https,
    https
      ? "Permanent redirects over HTTPS can be persistent"
      : "Permanent redirects over HTTP cannot be persistent"
  );

  gBrowser.removeCurrentTab();
  Services.cache2.clear();
}

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cache.persist_permanent_redirects_http", false],
      ["dom.security.https_first", false],
    ],
  });
});

add_task(async function test_cache_persistent() {
  await do_test_cache_persistent(true);
  await do_test_cache_persistent(false);
});
