/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { STATE_IS_SECURE, STATE_IS_BROKEN, STATE_IS_INSECURE } =
  Ci.nsIWebProgressListener;

// from ../../../build/pgo/server-locations.txt
const NO_CERT = "https://nocert.example.com:443";
const SELF_SIGNED = "https://self-signed.example.com:443";
const UNTRUSTED = "https://untrusted.example.com:443";
const EXPIRED = "https://expired.example.com:443";
const MISMATCH_EXPIRED = "https://mismatch.expired.example.com:443";
const MISMATCH_UNTRUSTED = "https://mismatch.untrusted.example.com:443";
const UNTRUSTED_EXPIRED = "https://untrusted-expired.example.com:443";
const MISMATCH_UNTRUSTED_EXPIRED =
  "https://mismatch.untrusted-expired.example.com:443";

const BAD_CERTS = [
  NO_CERT,
  SELF_SIGNED,
  UNTRUSTED,
  EXPIRED,
  MISMATCH_EXPIRED,
  MISMATCH_UNTRUSTED,
  UNTRUSTED_EXPIRED,
  MISMATCH_UNTRUSTED_EXPIRED,
];

function getConnectionState() {
  // prevents items that are being lazy loaded causing issues
  document.getElementById("identity-icon-box").click();
  gIdentityHandler.refreshIdentityPopup();
  return document.getElementById("identity-popup").getAttribute("connection");
}

/**
 * Compares the security state of the page with what is expected.
 * Returns one of "secure", "broken", "insecure", or "unknown".
 */
function isSecurityState(browser, expectedState) {
  const ui = browser.securityUI;
  if (!ui) {
    ok(false, "No security UI to get the security state");
    return;
  }

  const isSecure = ui.state & STATE_IS_SECURE;
  const isBroken = ui.state & STATE_IS_BROKEN;
  const isInsecure = ui.state & STATE_IS_INSECURE;

  let actualState;
  if (isSecure && !(isBroken || isInsecure)) {
    actualState = "secure";
  } else if (isBroken && !(isSecure || isInsecure)) {
    actualState = "broken";
  } else if (isInsecure && !(isSecure || isBroken)) {
    actualState = "insecure";
  } else {
    actualState = "unknown";
  }

  is(
    expectedState,
    actualState,
    `Expected state is ${expectedState} and actual state is ${actualState}`
  );
}

add_task(async function testDefault({ Security }) {
  for (const url of BAD_CERTS) {
    info(`Navigating to ${url}`);
    const loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
    BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
    await loaded;

    is(
      getConnectionState(),
      "cert-error-page",
      "Security error page is present"
    );
    isSecurityState(gBrowser, "insecure");
  }
});

add_task(async function testIgnore({ client }) {
  const { Security } = client;
  info("Enable security certificate override");
  await Security.setIgnoreCertificateErrors({ ignore: true });

  for (const url of BAD_CERTS) {
    info(`Navigating to ${url}`);
    BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

    is(
      getConnectionState(),
      "secure-cert-user-overridden",
      "Security certificate was overridden by user"
    );
    isSecurityState(gBrowser, "secure");
  }
});

add_task(async function testUnignore({ client }) {
  const { Security } = client;
  info("Disable security certificate override");
  await Security.setIgnoreCertificateErrors({ ignore: false });

  for (const url of BAD_CERTS) {
    info(`Navigating to ${url}`);
    const loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
    BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
    await loaded;

    is(
      getConnectionState(),
      "cert-error-page",
      "Security error page is present"
    );
    isSecurityState(gBrowser, "insecure");
  }
});

// smoke test for unignored -> ignored -> unignored
add_task(async function testToggle({ client }) {
  const { Security } = client;
  let loaded;

  info("Enable security certificate override");
  await Security.setIgnoreCertificateErrors({ ignore: true });

  info(`Navigating to ${UNTRUSTED} having set the override`);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, UNTRUSTED);
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);

  is(
    getConnectionState(),
    "secure-cert-user-overridden",
    "Security certificate was overridden by user"
  );
  isSecurityState(gBrowser, "secure");

  info("Disable security certificate override");
  await Security.setIgnoreCertificateErrors({ ignore: false });

  info(`Navigating to ${UNTRUSTED} having unset the override`);
  loaded = BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, UNTRUSTED);
  await loaded;

  is(
    getConnectionState(),
    "cert-error-page",
    "Security error page is present by default"
  );
  isSecurityState(gBrowser, "insecure");
});
