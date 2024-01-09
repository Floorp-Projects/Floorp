/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const urlPath = "/browser/netwerk/cookie/test/browser/file_empty.html";
const baseDomain = "example.com";

// eslint doesn't like http
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const URL_INSECURE_COM = "http://" + baseDomain + urlPath;
const URL_SECURE_COM = "https://" + baseDomain + urlPath;

// common cookie strings
const COOKIE_BASIC = "foo=one";
const COOKIE_OTHER = "foo=two";
const COOKIE_THIRD = "foo=three";
const COOKIE_FORTH = "foo=four";

function securify(cookie) {
  return cookie + "; Secure";
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("dom.security.https_first");
  Services.prefs.clearUserPref("network.cookie.cookieBehavior");
  Services.prefs.clearUserPref(
    "network.cookieJarSettings.unblocked_for_testing"
  );
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
  Services.prefs.clearUserPref("network.cookie.sameSite.noneRequiresSecure");
  Services.prefs.clearUserPref("network.cookie.sameSite.schemeful");
  info("Cleaning up the test");
});

async function setup() {
  // HTTPS-First would interfere with this test.
  Services.prefs.setBoolPref("dom.security.https_first", false);

  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );
  Services.prefs.setBoolPref("network.cookie.sameSite.schemeful", true);
  Services.cookies.removeAll();
}
add_task(setup);

// note:
// 1. The URL scheme will not matter for insecure cookies, since
//    cookies are not "schemeful" in this sense.
//    So an insecure cookie set anywhere will be visible on http and https sites
//    Secure cookies are different, they will only be visible from https sites
//    and will prevent cookie setting of the same name on insecure sites.
//
// 2. The different processes (tabs) shouldn't matter since
//    cookie adds/changes are distributed to other processes on a need-to-know
//    basis.

add_task(async function test_insecure_cant_overwrite_secure_via_doc() {
  // insecure
  const tab1 = BrowserTestUtils.addTab(gBrowser, URL_INSECURE_COM);
  const browser = gBrowser.getBrowserForTab(tab1);
  await BrowserTestUtils.browserLoaded(browser);

  // secure
  const tab2 = BrowserTestUtils.addTab(gBrowser, URL_SECURE_COM);
  const browser2 = gBrowser.getBrowserForTab(tab2);
  await BrowserTestUtils.browserLoaded(browser2);

  // init with insecure cookie on insecure origin child process
  await SpecialPowers.spawn(
    browser,
    [COOKIE_BASIC, COOKIE_BASIC],
    (cookie, expected) => {
      content.document.cookie = cookie;
      is(content.document.cookie, expected);
    }
  );

  // insecure cookie visible on secure origin process (sanity check)
  await SpecialPowers.spawn(browser2, [COOKIE_BASIC], expected => {
    is(content.document.cookie, expected);
  });

  // overwrite insecure cookie on secure origin with secure cookie (sanity check)
  await SpecialPowers.spawn(
    browser2,
    [securify(COOKIE_OTHER), COOKIE_OTHER],
    (cookie, expected) => {
      content.document.cookie = cookie;
      is(content.document.cookie, expected);
    }
  );

  // insecure cookie will NOT overwrite the secure one on insecure origin
  // and cookie.document appears blank
  await SpecialPowers.spawn(browser, [COOKIE_THIRD, ""], (cookie, expected) => {
    content.document.cookie = cookie; // quiet failure here
    is(content.document.cookie, expected);
  });

  // insecure cookie will overwrite secure cookie on secure origin
  // a bit weird, but this is normal
  await SpecialPowers.spawn(
    browser2,
    [COOKIE_FORTH, COOKIE_FORTH],
    (cookie, expected) => {
      content.document.cookie = cookie;
      is(content.document.cookie, expected);
    }
  );

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  Services.cookies.removeAll();
});
