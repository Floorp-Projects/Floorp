// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests various scenarios connecting to a server that requires client cert
// authentication. Also tests that nsIClientAuthDialogs.chooseCertificate
// is called at the appropriate times and with the correct arguments.

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

const DialogState = {
  // Assert that chooseCertificate() is never called.
  ASSERT_NOT_CALLED: "ASSERT_NOT_CALLED",
  // Return that the user selected the first given cert.
  RETURN_CERT_SELECTED: "RETURN_CERT_SELECTED",
  // Return that the user canceled.
  RETURN_CERT_NOT_SELECTED: "RETURN_CERT_NOT_SELECTED",
};

var sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);

var gExpectedClientCertificateChoices;

// Mock implementation of nsIClientAuthDialogs.
const gClientAuthDialogs = {
  _state: DialogState.ASSERT_NOT_CALLED,
  _rememberClientAuthCertificate: false,
  _chooseCertificateCalled: false,

  set state(newState) {
    info(`old state: ${this._state}`);
    this._state = newState;
    info(`new state: ${this._state}`);
  },

  get state() {
    return this._state;
  },

  set rememberClientAuthCertificate(value) {
    this._rememberClientAuthCertificate = value;
  },

  get rememberClientAuthCertificate() {
    return this._rememberClientAuthCertificate;
  },

  get chooseCertificateCalled() {
    return this._chooseCertificateCalled;
  },

  set chooseCertificateCalled(value) {
    this._chooseCertificateCalled = value;
  },

  chooseCertificate(
    hostname,
    port,
    organization,
    issuerOrg,
    certList,
    selectedIndex,
    rememberClientAuthCertificate
  ) {
    this.chooseCertificateCalled = true;
    Assert.notEqual(
      this.state,
      DialogState.ASSERT_NOT_CALLED,
      "chooseCertificate() should be called only when expected"
    );

    rememberClientAuthCertificate.value = this.rememberClientAuthCertificate;

    Assert.equal(
      hostname,
      "requireclientcert.example.com",
      "Hostname should be 'requireclientcert.example.com'"
    );
    Assert.equal(port, 443, "Port should be 443");
    Assert.equal(
      organization,
      "",
      "Server cert Organization should be empty/not present"
    );
    Assert.equal(
      issuerOrg,
      "Mozilla Testing",
      "Server cert issuer Organization should be 'Mozilla Testing'"
    );

    // For mochitests, the cert at build/pgo/certs/mochitest.client should be
    // selectable as well as one of the PGO certs we loaded in `setup`, so we do
    // some brief checks to confirm this.
    Assert.notEqual(certList, null, "Cert list should not be null");
    Assert.equal(
      certList.length,
      gExpectedClientCertificateChoices,
      `${gExpectedClientCertificateChoices} certificates should be available`
    );

    for (let cert of certList.enumerate(Ci.nsIX509Cert)) {
      Assert.notEqual(cert, null, "Cert list should contain nsIX509Certs");
      Assert.equal(
        cert.issuerCommonName,
        "Temporary Certificate Authority",
        "cert should have expected issuer CN"
      );
    }

    if (this.state == DialogState.RETURN_CERT_SELECTED) {
      selectedIndex.value = 0;
      return true;
    }
    return false;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIClientAuthDialogs]),
};

add_task(async function setup() {
  let clientAuthDialogsCID = MockRegistrar.register(
    "@mozilla.org/nsClientAuthDialogs;1",
    gClientAuthDialogs
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(clientAuthDialogsCID);
  });

  // This CA has the expected keyCertSign and cRLSign usages. It should not be
  // presented for use as a client certificate.
  await readCertificate("pgo-ca-regular-usages.pem", "CTu,CTu,CTu");
  // This CA has all keyUsages. For compatibility with preexisting behavior, it
  // will be presented for use as a client certificate.
  await readCertificate("pgo-ca-all-usages.pem", "CTu,CTu,CTu");
  // This client certificate was issued by an intermediate that was issued by
  // the test CA. The server only lists the test CA's subject distinguished name
  // as an acceptible issuer name for client certificates. If the implementation
  // can determine that the test CA is a root CA for the client certificate and
  // thus is acceptible to use, it should be included in the chooseCertificate
  // callback. At the beginning of this test (speaking of this file as a whole),
  // the client is not aware of the intermediate, and so it is not available in
  // the callback.
  await readCertificate("client-cert-via-intermediate.pem", ",,");
  gExpectedClientCertificateChoices = 2;
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
 * @param {Object} options
 *        Optional options object to pass on to the window that gets opened.
 */
async function testHelper(prefValue, expectedURL, options = undefined) {
  gClientAuthDialogs.chooseCertificateCalled = false;
  await SpecialPowers.pushPrefEnv({
    set: [["security.default_personal_cert", prefValue]],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow(options);

  await BrowserTestUtils.loadURI(
    win.gBrowser.selectedBrowser,
    "https://requireclientcert.example.com:443"
  );

  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    "https://requireclientcert.example.com/",
    true
  );
  let loadedURL = win.gBrowser.selectedBrowser.documentURI.spec;
  Assert.ok(
    loadedURL.startsWith(expectedURL),
    `Expected and actual URLs should match (got '${loadedURL}', expected '${expectedURL}')`
  );
  Assert.equal(
    gClientAuthDialogs.chooseCertificateCalled,
    prefValue == "Ask Every Time",
    "chooseCertificate should have been called if we were expecting it to be called"
  );

  await win.close();

  // This clears the TLS session cache so we don't use a previously-established
  // ticket to connect and bypass selecting a client auth certificate in
  // subsequent tests.
  sdr.logout();
}

// Test that if a certificate is chosen automatically the connection succeeds,
// and that nsIClientAuthDialogs.chooseCertificate() is never called.
add_task(async function testCertChosenAutomatically() {
  gClientAuthDialogs.state = DialogState.ASSERT_NOT_CALLED;
  await testHelper(
    "Select Automatically",
    "https://requireclientcert.example.com/"
  );
  // This clears all saved client auth certificate state so we don't influence
  // subsequent tests.
  sdr.logoutAndTeardown();
});

// Test that if the user doesn't choose a certificate, the connection fails and
// an error page is displayed.
add_task(async function testCertNotChosenByUser() {
  gClientAuthDialogs.state = DialogState.RETURN_CERT_NOT_SELECTED;
  await testHelper(
    "Ask Every Time",
    "about:neterror?e=nssFailure2&u=https%3A//requireclientcert.example.com/"
  );
  sdr.logoutAndTeardown();
});

// Test that if the user chooses a certificate the connection suceeeds.
add_task(async function testCertChosenByUser() {
  gClientAuthDialogs.state = DialogState.RETURN_CERT_SELECTED;
  await testHelper("Ask Every Time", "https://requireclientcert.example.com/");
  sdr.logoutAndTeardown();
});

// Test that if the user chooses a certificate in a private browsing window,
// configures Firefox to remember this certificate for the duration of the
// session, closes that window (and thus all private windows), reopens a private
// window, and visits that site again, they are re-asked for a certificate (i.e.
// any state from the previous private session should be gone). Similarly, after
// closing that private window, if the user opens a non-private window, they
// again should be asked to choose a certificate (i.e. private state should not
// be remembered/used in non-private contexts).
add_task(async function testClearPrivateBrowsingState() {
  gClientAuthDialogs.rememberClientAuthCertificate = true;
  gClientAuthDialogs.state = DialogState.RETURN_CERT_SELECTED;
  await testHelper("Ask Every Time", "https://requireclientcert.example.com/", {
    private: true,
  });
  await testHelper("Ask Every Time", "https://requireclientcert.example.com/", {
    private: true,
  });
  await testHelper("Ask Every Time", "https://requireclientcert.example.com/");
  // NB: we don't `sdr.logoutAndTeardown()` in between the two calls to
  // `testHelper` because that would clear all client auth certificate state and
  // obscure what we're testing (that Firefox properly clears the relevant state
  // when the last private window closes).
  sdr.logoutAndTeardown();
});

// Test that 3rd party certificates are taken into account when filtering client
// certificates based on the acceptible CA list sent by the server.
add_task(async function testCertFilteringWithIntermediate() {
  let intermediateBytes = await OS.File.read(
    getTestFilePath("intermediate.pem")
  ).then(
    data => {
      let decoder = new TextDecoder();
      let pem = decoder.decode(data);
      let base64 = pemToBase64(pem);
      let bin = atob(base64);
      let bytes = [];
      for (let i = 0; i < bin.length; i++) {
        bytes.push(bin.charCodeAt(i));
      }
      return bytes;
    },
    error => {
      throw error;
    }
  );
  let nssComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsINSSComponent);
  nssComponent.addEnterpriseIntermediate(intermediateBytes);
  gExpectedClientCertificateChoices = 3;
  gClientAuthDialogs.state = DialogState.RETURN_CERT_SELECTED;
  await testHelper("Ask Every Time", "https://requireclientcert.example.com/");
  sdr.logoutAndTeardown();
  // This will reset the added intermediate.
  await SpecialPowers.pushPrefEnv({
    set: [["security.enterprise_roots.enabled", true]],
  });
});
