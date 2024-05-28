/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(() => {
  Services.prefs.setIntPref(
    "network.cookie.cookieBehavior",
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );
  Services.prefs.setBoolPref(
    "network.cookie.cookieBehavior.optInPartitioning",
    true
  );
  Services.prefs.setBoolPref("network.cookie.CHIPS.enabled", true);
  Services.prefs.setBoolPref("dom.storage_access.enabled", true);
  Services.prefs.setBoolPref("dom.storage_access.prompt.testing", true);
  Services.cookies.removeAll();
  Services.perms.removeAll();
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  Services.prefs.clearUserPref(
    "network.cookie.cookieBehavior.optInPartitioning"
  );
  Services.prefs.clearUserPref("network.cookie.CHIPS.enabled");
  Services.prefs.clearUserPref("dom.storage_access.enabled");
  Services.prefs.clearUserPref("dom.storage_access.prompt.testing");
  Services.cookies.removeAll();
  Services.perms.removeAll();
});

const COOKIE_PARTITIONED =
  "cookie=partitioned; Partitioned; Secure; SameSite=None;";
const COOKIE_UNPARTITIONED = "cookie=unpartitioned; Secure; SameSite=None;";

const PATH = "/browser/netwerk/cookie/test/browser/";
const PATH_EMPTY = PATH + "file_empty.html";
const HTTP_COOKIE_SET = PATH + "chips.sjs?set";
const HTTP_COOKIE_GET = PATH + "chips.sjs?get";

const FIRST_PARTY = "example.com";
const THIRD_PARTY = "example.org";

const URL_DOCUMENT_FIRSTPARTY = "https://" + FIRST_PARTY + PATH_EMPTY;
const URL_DOCUMENT_THIRDPARTY = "https://" + THIRD_PARTY + PATH_EMPTY;
const FIRST_PARTY_DOMAIN = "https://" + FIRST_PARTY;
const THIRD_PARTY_DOMAIN = "https://" + THIRD_PARTY;
const URL_HTTP_FIRSTPARTY = FIRST_PARTY_DOMAIN + "/" + HTTP_COOKIE_SET;
const URL_HTTP_THIRDPARTY = THIRD_PARTY_DOMAIN + "/" + HTTP_COOKIE_SET;

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
  let uri = NetUtil.newURI(url);
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
// This calls CookieServiceChild::SetCookieStringFromDocument() internally.
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
      FIRST_PARTY
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      FIRST_PARTY
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].value, "partitioned");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].value, "unpartitioned");

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// Set partitioned and unpartitioned cookie from third-party document with storage
// access. CHIPS "Partitioned" cookie MUST always be stored in partitioned jar.
// This calls CookieServiceChild::SetCookieStringFromDocument() internally.
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
            SpecialPowers.wrap(content.document).notifyUserGestureActivation();
            await SpecialPowers.addPermission(
              "storageAccessAPI",
              true,
              content.location.href
            );
            await SpecialPowers.wrap(content.document).requestStorageAccess();

            ok(
              await content.document.hasStorageAccess(),
              "example.org should have storageAccess"
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
      THIRD_PARTY
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      THIRD_PARTY
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].value, "partitioned");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].value, "unpartitioned");

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
  // chips.sjs being loaded.
  const tab = BrowserTestUtils.addTab(gBrowser, URL_HTTP_FIRSTPARTY);
  const browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  // Get cookies from partitioned jar
  let partitioned = Services.cookies.getCookiesWithOriginAttributes(
    partitionedOAs,
    FIRST_PARTY
  );
  // Get cookies from unpartitioned jar
  let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
    unpartitionedOAs,
    FIRST_PARTY
  );

  // Assert partitioned/unpartitioned cookie were stored in correct jars
  Assert.equal(partitioned.length, 1);
  Assert.equal(partitioned[0].value, "partitioned");
  Assert.equal(unpartitioned.length, 1);
  Assert.equal(unpartitioned[0].value, "unpartitioned");

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
    await SpecialPowers.spawn(
      browser,
      [THIRD_PARTY_DOMAIN, URL_HTTP_THIRDPARTY],
      async (url, fetchUrl) => {
        let ifr = content.document.createElement("iframe");
        ifr.src = url;
        content.document.body.appendChild(ifr);
        // Send http request with "set" query parameter, partitioned and
        // unpartitioned cookie will be set through http response from chips.sjs.
        await ContentTaskUtils.waitForEvent(ifr, "load");

        // Spawn iframe bc
        await SpecialPowers.spawn(
          await ifr.browsingContext,
          [fetchUrl],
          async url => {
            SpecialPowers.wrap(content.document).notifyUserGestureActivation();
            await SpecialPowers.addPermission(
              "storageAccessAPI",
              true,
              content.location.href
            );
            await SpecialPowers.wrap(content.document).requestStorageAccess();

            ok(
              await content.document.hasStorageAccess(),
              "example.org should have storageAccess"
            );

            await content.fetch(url);
          }
        );
      }
    );

    // Get cookies from partitioned jar
    let partitioned = Services.cookies.getCookiesWithOriginAttributes(
      partitionedOAs,
      THIRD_PARTY
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      THIRD_PARTY
    );

    // Assert partitioned/unpartitioned cookie were stored in correct jars
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].value, "partitioned");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].value, "unpartitioned");

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// TODO CHIPS - Tests for CookieServiceChild::SetCookieStringFromHttp() need
// to be added. Since this is only checkable on onProxyConnectSuccess needs
// proxy setup test harness. It is also called after onStartRequest() (Http)
// but cookies are already set by the parents
// CookieService::SetCookieStringFromHttp() call.

// Get partitioned and unpartitioned cookies from document (child).
// This calls CookieServiceChild::GetCookieStringFromDocument() internally.
add_task(
  async function test_chips_send_partitioned_and_unpartitioned_document_child() {
    const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    // Spawn document bc
    await SpecialPowers.spawn(
      browser,
      [COOKIE_PARTITIONED, COOKIE_UNPARTITIONED],
      async (partitioned, unpartitioned) => {
        content.document.cookie = partitioned;
        content.document.cookie = unpartitioned;

        // Assert both unpartitioned and partitioned cookie are returned.
        let cookies = content.document.cookie;
        ok(
          cookies.includes("cookie=partitioned"),
          "Cookie from partitioned jar was sent."
        );
        ok(
          cookies.includes("cookie=unpartitioned"),
          "Cookie from unpartitioned jar was sent."
        );
      }
    );

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);

// Get partitioned and unpartitioned cookies from document (child) after
// storageAccess was granted. This calls CookieServiceChild::TrackCookieLoad()
// internally to update child's cookies.
add_task(
  async function test_chips_send_partitioned_and_unpartitioned_on_storage_access_child() {
    // Set example.org first-party unpartitioned cookie
    await BrowserTestUtils.withNewTab(
      URL_DOCUMENT_THIRDPARTY,
      async browser => {
        info("Set a first party cookie via `document.cookie`.");
        await SpecialPowers.spawn(
          browser,
          [COOKIE_UNPARTITIONED],
          async unpartitioned => {
            content.document.cookie = unpartitioned;
            is(
              content.document.cookie,
              "cookie=unpartitioned",
              "Unpartitioned cookie was set."
            );
          }
        );
      }
    );

    // Assert cookie was set on parent cookie service for example.org.
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      THIRD_PARTY
    );
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].value, "unpartitioned");

    // Load example.com as first-party in tab
    const tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    const browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    // Set third-party cookie from example.org iframe, get storageAccess and
    // check cookies.
    // Spawn document bc
    await SpecialPowers.spawn(
      browser,
      [URL_DOCUMENT_THIRDPARTY, COOKIE_PARTITIONED],
      async (url, partitioned) => {
        // Create third-party iframe
        let ifr = content.document.createElement("iframe");
        ifr.src = url;
        content.document.body.appendChild(ifr);
        await ContentTaskUtils.waitForEvent(ifr, "load");

        // Spawn iframe bc
        await SpecialPowers.spawn(
          await ifr.browsingContext,
          [partitioned],
          async partitioned => {
            ok(
              !(await content.document.hasStorageAccess()),
              "example.org should not have storageAccess initially."
            );

            // Set a partitioned third-party cookie and assert its the only.
            content.document.cookie = partitioned;
            is(
              content.document.cookie,
              "cookie=partitioned",
              "Partitioned cookie was set."
            );

            info("Simulate user activation.");
            SpecialPowers.wrap(content.document).notifyUserGestureActivation();

            info("Request storage access.");
            await content.document.requestStorageAccess();

            ok(
              await content.document.hasStorageAccess(),
              "example.org should now have storageAccess."
            );

            // Assert both unpartitioned and partitioned cookie are returned.
            let cookies = content.document.cookie;
            ok(
              cookies.includes("cookie=partitioned"),
              "Cookie from partitioned jar was sent."
            );
            ok(
              cookies.includes("cookie=unpartitioned"),
              "Cookie from unpartitioned jar was sent."
            );
          }
        );
      }
    );

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
    Services.perms.removeAll();
  }
);

// Set partitioned and unpartitioned cookies for URL_DOCUMENT_FIRSTPARTY, then
// load URL again, assure cookies are correctly send to content child process.
// This tests CookieServiceParent::TrackCookieLoad() internally.
add_task(
  async function test_chips_send_partitioned_and_unpartitioned_document_parent() {
    // Set example.com first-party unpartitioned and partitioned cookie, then
    // close tab.
    await BrowserTestUtils.withNewTab(
      URL_DOCUMENT_FIRSTPARTY,
      async browser => {
        await SpecialPowers.spawn(
          browser,
          [COOKIE_PARTITIONED, COOKIE_UNPARTITIONED],
          async (partitioned, unpartitioned) => {
            content.document.cookie = unpartitioned;
            content.document.cookie = partitioned;
            let cookies = content.document.cookie;
            ok(
              cookies.includes("cookie=unpartitioned"),
              "Unpartitioned cookie was set."
            );
            ok(
              cookies.includes("cookie=partitioned"),
              "Partitioned cookie was set."
            );
          }
        );
      }
    );

    // Assert we have one partitioned and one unpartitioned cookie set.
    // Get cookies from partitioned jar
    let partitioned = Services.cookies.getCookiesWithOriginAttributes(
      partitionedOAs,
      FIRST_PARTY
    );
    // Get cookies from unpartitioned jar
    let unpartitioned = Services.cookies.getCookiesWithOriginAttributes(
      unpartitionedOAs,
      FIRST_PARTY
    );
    Assert.equal(partitioned.length, 1);
    Assert.equal(partitioned[0].value, "partitioned");
    Assert.equal(unpartitioned.length, 1);
    Assert.equal(unpartitioned[0].value, "unpartitioned");

    // Reload example.com and assert previously set cookies are correctly
    // send to content child document.
    await BrowserTestUtils.withNewTab(
      URL_DOCUMENT_FIRSTPARTY,
      async browser => {
        await SpecialPowers.spawn(browser, [], () => {
          let cookies = content.document.cookie;
          ok(
            cookies.includes("cookie=unpartitioned"),
            "Unpartitioned cookie was sent."
          );
          ok(
            cookies.includes("cookie=partitioned"),
            "Partitioned cookie was sent."
          );
        });
      }
    );

    // Cleanup
    Services.cookies.removeAll();
  }
);

// Set partitioned and unpartitioned cookies for URL_DOCUMENT_FIRSTPARTY, then
// send http request, assure cookies are correctly send in "Cookie" header.
// This tests CookieService::GetCookieStringFromHttp() internally.
add_task(
  async function test_chips_send_partitioned_and_unpartitioned_http_parent() {
    // Load empty document.
    let tab = BrowserTestUtils.addTab(gBrowser, URL_DOCUMENT_FIRSTPARTY);
    let browser = gBrowser.getBrowserForTab(tab);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [HTTP_COOKIE_SET, HTTP_COOKIE_GET],
      async (set, get) => {
        // Send http request with "set" query parameter, partitioned and
        // unpartitioned cookie will be set through http response.
        await content.fetch(set);

        // Assert cookies were set to document.
        let cookies = content.document.cookie;
        ok(
          cookies.includes("cookie=unpartitioned"),
          "Unpartitioned cookie was set to document."
        );
        ok(
          cookies.includes("cookie=partitioned"),
          "Partitioned cookie was set to document."
        );

        // Send http request with "get" query parameter, chips.sjs will return
        // the request "Cookie" header string.
        await content
          .fetch(get)
          .then(response => response.text())
          .then(requestCookies => {
            // Assert cookies were sent in http request.
            ok(
              requestCookies.includes("cookie=unpartitioned"),
              "Unpartitioned cookie was sent in http request."
            );
            ok(
              requestCookies.includes("cookie=partitioned"),
              "Partitioned cookie was sent in http request."
            );
          });
      }
    );

    // Cleanup
    BrowserTestUtils.removeTab(tab);
    Services.cookies.removeAll();
  }
);
