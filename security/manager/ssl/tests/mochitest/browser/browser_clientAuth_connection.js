// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests various scenarios connecting to a server that requires client cert
// authentication. Also tests that nsIClientAuthDialogs.chooseCertificate
// is called at the appropriate times and with the correct arguments.

const { MockRegistrar } =
  ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", {});

const DialogState = {
  // Assert that chooseCertificate() is never called.
  ASSERT_NOT_CALLED: "ASSERT_NOT_CALLED",
  // Return that the user selected the first given cert.
  RETURN_CERT_SELECTED: "RETURN_CERT_SELECTED",
  // Return that the user canceled.
  RETURN_CERT_NOT_SELECTED: "RETURN_CERT_NOT_SELECTED",
};

let sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);

// Mock implementation of nsIClientAuthDialogs.
const gClientAuthDialogs = {
  _state: DialogState.ASSERT_NOT_CALLED,

  set state(newState) {
    info(`old state: ${this._state}`);
    this._state = newState;
    info(`new state: ${this._state}`);
  },

  get state() {
    return this._state;
  },

  chooseCertificate(ctx, hostname, port, organization, issuerOrg, certList,
                    selectedIndex) {
    Assert.notEqual(this.state, DialogState.ASSERT_NOT_CALLED,
                    "chooseCertificate() should be called only when expected");

    let caud = ctx.QueryInterface(Ci.nsIClientAuthUserDecision);
    Assert.notEqual(caud, null,
                    "nsIClientAuthUserDecision should be queryable from the " +
                    "given context");
    caud.rememberClientAuthCertificate = false;

    Assert.equal(hostname, "requireclientcert.example.com",
                 "Hostname should be 'requireclientcert.example.com'");
    Assert.equal(port, 443, "Port should be 443");
    Assert.equal(organization, "",
                 "Server cert Organization should be empty/not present");
    Assert.equal(issuerOrg, "Mozilla Testing",
                 "Server cert issuer Organization should be 'Mozilla Testing'");

    // For mochitests, only the cert at build/pgo/certs/mochitest.client should
    // be selectable, so we do some brief checks to confirm this.
    Assert.notEqual(certList, null, "Cert list should not be null");
    Assert.equal(certList.length, 1, "Only 1 certificate should be available");
    let cert = certList.queryElementAt(0, Ci.nsIX509Cert);
    Assert.notEqual(cert, null, "Cert list should contain an nsIX509Cert");
    Assert.equal(cert.commonName, "Mochitest client",
                 "Cert CN should be 'Mochitest client'");

    if (this.state == DialogState.RETURN_CERT_SELECTED) {
      selectedIndex.value = 0;
      return true;
    }
    return false;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIClientAuthDialogs])
};

add_task(async function setup() {
  let clientAuthDialogsCID =
    MockRegistrar.register("@mozilla.org/nsClientAuthDialogs;1",
                           gClientAuthDialogs);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(clientAuthDialogsCID);
  });
});

/**
 * Test helper for the tests below.
 *
 * @param {String} prefValue
 *        Value to set the "security.default_personal_cert" pref to.
 * @param {String} expectedURL
 *        If the connection is expected to load successfully, the URL that
 *        should load. If the connection is expected to fail and result in an
 *        error page, |undefined|.
 */
async function testHelper(prefValue, expectedURL) {
  await SpecialPowers.pushPrefEnv({"set": [
    ["security.default_personal_cert", prefValue],
  ]});

  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser,
                                 "https://requireclientcert.example.com:443");

  // |loadedURL| will be a string URL if browserLoaded() wins the race, or
  // |undefined| if waitForErrorPage() wins the race.
  let loadedURL = await Promise.race([
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser),
    BrowserTestUtils.waitForErrorPage(gBrowser.selectedBrowser),
  ]);
  Assert.equal(expectedURL, loadedURL, "Expected and actual URLs should match");

  // Ensure previously successful connections don't influence future tests.
  sdr.logoutAndTeardown();
}

// Test that if a certificate is chosen automatically the connection succeeds,
// and that nsIClientAuthDialogs.chooseCertificate() is never called.
add_task(async function testCertChosenAutomatically() {
  gClientAuthDialogs.state = DialogState.ASSERT_NOT_CALLED;
  await testHelper("Select Automatically",
                    "https://requireclientcert.example.com/");
});

// Test that if the user doesn't choose a certificate, the connection fails and
// an error page is displayed.
add_task(async function testCertNotChosenByUser() {
  gClientAuthDialogs.state = DialogState.RETURN_CERT_NOT_SELECTED;
  await testHelper("Ask Every Time", undefined);
});

// Test that if the user chooses a certificate the connection suceeeds.
add_task(async function testCertChosenByUser() {
  gClientAuthDialogs.state = DialogState.RETURN_CERT_SELECTED;
  await testHelper("Ask Every Time",
                    "https://requireclientcert.example.com/");
});
