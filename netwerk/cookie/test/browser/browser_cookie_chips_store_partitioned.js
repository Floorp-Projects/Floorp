/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async => {
  // These are functional and not integration tests, cookieBehavior accept lets
  // us have StorageAccess on thirparty.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.cookieBehavior.optInPartitioning",
    true
  );
  Services.prefs.setBoolPref("dom.storage_access.enabled", true);
  Services.cookies.removeAll();
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  Services.prefs.clearUserPref(
    "network.cookie.cookieBehavior.optInPartitioning"
  );
  Services.prefs.clearUserPref("dom.storage_access.enabled");
  Services.cookies.removeAll();
});

const COOKIE_PARTITIONED = "part=value; Partitioned; Secure; SameSite=None;";
const COOKIE_UNPARTITIONED = "unpart=value; Secure; SameSite=None;";

const pathEmpty = "/browser/netwerk/cookie/test/browser/file_empty.html";
const pathCookie = "/browser/netwerk/cookie/test/browser/partitioned.sjs";
const firstParty = "example.com";
const thirdParty = "example.org";

const URL_DOCUMENT_FIRSTPARTY = "https://" + firstParty + pathEmpty;
const URL_DOCUMENT_THIRDPARTY = "https://" + thirdParty + pathEmpty;

const URL_HTTP_FIRSTPARTY = "https://" + firstParty + pathCookie;
const URL_HTTP_THIRDPARTY = "https://" + thirdParty + pathCookie;

function createOriginAttributes(partitionKey) {
  return JSON.stringify({
    firstPartyDomain: "",
    geckoViewSessionContextId: "",
    inIsolatedMozBrowser: false,
    partitionKey,
    privateBrowsingId: 0,
    userContextId: 0,
  });
}

function createPartitonKey(url) {
  let uri = NetUtil.newURI(URL_DOCUMENT_FIRSTPARTY);
  return `(${uri.scheme},${uri.host})`;
}

// OriginAttributes used to access partitioned and unpartitioned cookie jars
// in all tests.
const partitionedOAs = createOriginAttributes(
  createPartitonKey(URL_DOCUMENT_FIRSTPARTY)
);
const unpartitionedOAs = createOriginAttributes("");

// Set partitioned and unpartitioned cookie from first-party document.
// CHIPS "Partitioned" cookie MUST always be stored in partitioned jar.
// This calls CookieServiceChild::SetCookieStringFromDocument() internally
// CookieService::SetCookieStringFromDocument() is not explicitly tested since
// CHIPS are in the common function CookieCommons::CreateCookieFromDocument().
add_task(
  async function test_chips_store_partitioned_document_first_party_child() {
    const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    // Set partitioned and unpartitioned cookie from document child-side
    await SpecialPowers.spawn(
      browser,
      [COOKIE_PARTITIONED, COOKIE_UNPARTITIONED],
      (partitioned, unpartitioned) => {
        content.document.cookie = partitioned;
        content.document.cookie = unpartitioned;
      }
    );

    // Get cookies from partitioned jar
    let partitioned = Services.cookies.getCookiesWithOriginAttributes(
      partitionedOAs,
      firstParty
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      firstParty
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].name, "part");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].name, "unpart");

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// Set partitioned and unpartitioned cookie from third-party document with storage
// access. CHIPS "Partitioned" cookie MUST always be stored in partitioned jar.
// This calls CookieServiceChild::SetCookieStringFromDocument() internally
// CookieService::SetCookieStringFromDocument() is not explicitly tested since
// CHIPS are in the common function CookieCommons::CreateCookieFromDocument().
add_task(
  async function test_chips_store_partitioned_document_third_party_storage_access_child() {
    const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    // Spawn document bc
    await SpecialPowers.spawn(
      browser,
      [URL_DOCUMENT_THIRDPARTY, COOKIE_PARTITIONED, COOKIE_UNPARTITIONED],
      async (url, partitioned, unpartitioned) => {
        let ifr = content.document.createElement("iframe");
        ifr.src = url;
        content.document.body.appendChild(ifr);
        await ContentTaskUtils.waitForEvent(ifr, "load");

        // Spawn iframe bc
        await SpecialPowers.spawn(
          await ifr.browsingContext,
          [partitioned, unpartitioned],
          async (partitioned, unpartitioned) => {
            ok(
              await content.document.hasStorageAccess(),
              "example.org should have storageAccess by CookieBehavior 0 / test setup"
            );

            content.document.cookie = partitioned;
            content.document.cookie = unpartitioned;
          }
        );
      }
    );

    // Get cookies from partitioned jar
    let partitioned = Services.cookies.getCookiesWithOriginAttributes(
      partitionedOAs,
      thirdParty
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      thirdParty
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].name, "part");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].name, "unpart");

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// Set partitioned and unpartitioned cookie from first-party http load.
// CHIPS "Partitioned" cookie MUST always be stored in partitioned jar.
// This calls CookieService::SetCookieStringFromHttp() internally.
add_task(async function test_chips_store_partitioned_http_first_party_parent() {
  // Set partitioned and unpartitioned cookie from http parent side through
  // partitoned.sjs being loaded
  const tab = BrowserTestUtils.addTab(gBrowser, URL_HTTP_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedOAs,
    firstParty
  );
  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs,
    firstParty
  );

  // Assert partitioned/unpartitioned cookie were stored in correct jars
  Assert.equal(partitioned.length, 1);
  Assert.equal(partitioned[0].name, "b");
  Assert.equal(unpartitioned.length, 1);
  Assert.equal(unpartitioned[0].name, "a");

  // Cleanup
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

// Set partitioned and unpartitioned cookie from third-party http load.
// CHIPS "Partitioned" cookie MUST always be stored in partitioned jar.
// This calls CookieService::SetCookieStringFromHttp() internally.
add_task(
  async function test_chips_store_partitioned_http_third_party_storage_access_parent() {
    const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    // Spawn document bc
    await SpecialPowers.spawn(browser, [URL_HTTP_THIRDPARTY], async url => {
      let ifr = content.document.createElement("iframe");
      ifr.src = url;
      content.document.body.appendChild(ifr);
      // Set partitioned and unpartitioned cookie from http parent side through
      // partitoned.sjs being loaded
      await ContentTaskUtils.waitForEvent(ifr, "load");

      // Spawn iframe bc
      await SpecialPowers.spawn(await ifr.browsingContext, [], async () => {
        ok(
          await content.document.hasStorageAccess(),
          "example.org should have storageAccess by CookieBehavior 0 / test setup"
        );
      });
    });

    // Get cookies from partitioned jar
    let partitioned = Services.cookies.getCookiesWithOriginAttributes(
      partitionedOAs,
      thirdParty
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      thirdParty
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].name, "b");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].name, "a");

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// TODO CHIPS - Tests for CookieServiceChild::SetCookieStringFromHttp() need
// to be added. Since this only called on onProxyConnectSuccess needs proxy
// setup harness.
