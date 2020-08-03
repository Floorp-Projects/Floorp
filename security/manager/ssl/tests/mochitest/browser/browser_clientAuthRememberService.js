// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

/**
 * Test certificate (i.e. build/pgo/certs/mochitest.client).
 * @type nsIX509Cert
 */
var cert;
var cert2;
var cert3;

var sdr = Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing);
var certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

var deleted = false;

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

function findCertByCommonName(commonName) {
  for (let cert of certDB.getCerts()) {
    if (cert.commonName == commonName) {
      return cert;
    }
  }
  return null;
}

async function testHelper(connectURL, expectedURL) {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  await SpecialPowers.pushPrefEnv({
    set: [["security.default_personal_cert", "Ask Every Time"]],
  });

  await BrowserTestUtils.loadURI(win.gBrowser.selectedBrowser, connectURL);

  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    expectedURL,
    true
  );
  let loadedURL = win.gBrowser.selectedBrowser.documentURI.spec;
  Assert.ok(
    loadedURL.startsWith(expectedURL),
    `Expected and actual URLs should match (got '${loadedURL}', expected '${expectedURL}')`
  );

  await win.close();

  // This clears the TLS session cache so we don't use a previously-established
  // ticket to connect and bypass selecting a client auth certificate in
  // subsequent tests.
  sdr.logout();
}

async function openRequireClientCert() {
  gClientAuthDialogs.chooseCertificateCalled = false;
  await testHelper(
    "https://requireclientcert.example.com:443",
    "https://requireclientcert.example.com/"
  );
}

async function openRequireClientCert2() {
  gClientAuthDialogs.chooseCertificateCalled = false;
  await testHelper(
    "https://requireclientcert-2.example.com:443",
    "https://requireclientcert-2.example.com/"
  );
}

// Mock implementation of nsIClientAuthRememberService
const gClientAuthRememberService = {
  forgetRememberedDecision(key) {
    deleted = true;
    Assert.equal(
      key,
      "exampleKey2",
      "Expected to get the same key that was passed in getDecisions()"
    );
  },

  getDecisions() {
    return [
      {
        asciiHost: "example.com",
        dbKey: cert.dbKey,
        entryKey: "exampleKey1",
      },
      {
        asciiHost: "example.org",
        dbKey: cert2.dbKey,
        entryKey: "exampleKey2",
      },
      {
        asciiHost: "example.test",
        dbKey: cert3.dbKey,
        entryKey: "exampleKey3",
      },
    ];
  },

  QueryInterface: ChromeUtils.generateQI(["nsIClientAuthRememberService"]),
};

const gClientAuthDialogs = {
  _chooseCertificateCalled: false,

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
    rememberClientAuthCertificate.value = true;
    this.chooseCertificateCalled = true;
    selectedIndex.value = 0;
    return true;
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIClientAuthDialogs]),
};

add_task(async function testRememberedDecisionsUI() {
  cert = findCertByCommonName("Mochitest client");
  cert2 = await readCertificate("pgo-ca-all-usages.pem", ",,");
  cert3 = await readCertificate("client-cert-via-intermediate.pem", ",,");
  isnot(cert, null, "Should be able to find the test client cert");
  isnot(cert2, null, "Should be able to find pgo-ca-all-usages.pem");
  isnot(cert3, null, "Should be able to find client-cert-via-intermediate.pem");

  let clientAuthRememberServiceCID = MockRegistrar.register(
    "@mozilla.org/security/clientAuthRememberService;1",
    gClientAuthRememberService
  );

  let win = await openCertManager();

  let listItems = win.document
    .getElementById("rememberedList")
    .querySelectorAll("richlistitem");

  Assert.equal(
    listItems.length,
    3,
    "Expected rememberedList to only have one item"
  );

  let labels = win.document
    .getElementById("rememberedList")
    .querySelectorAll("label");

  Assert.equal(
    labels.length,
    9,
    "Expected the rememberedList to have three labels"
  );

  let expectedHosts = ["example.com", "example.org", "example.test"];
  let hosts = [labels[0].value, labels[3].value, labels[6].value];
  let expectedNames = [cert.commonName, cert2.commonName, cert3.commonName];
  let names = [labels[1].value, labels[4].value, labels[7].value];
  let expectedSerialNumbers = [
    cert.serialNumber,
    cert2.serialNumber,
    cert3.serialNumber,
  ];
  let serialNumbers = [labels[2].value, labels[5].value, labels[8].value];

  for (let i = 0; i < 3; i++) {
    Assert.equal(hosts[i], expectedHosts[i], "Expected host to be asciiHost");
    Assert.equal(
      names[i],
      expectedNames[i],
      "Expected name to be the commonName of the cert"
    );
    Assert.equal(
      serialNumbers[i],
      expectedSerialNumbers[i],
      "Expected serialNumber to be the serialNumber of the cert"
    );
  }

  win.document.getElementById("rememberedList").selectedIndex = 1;

  win.document.getElementById("remembered_deleteButton").click();

  Assert.ok(deleted, "Expected forgetRememberedDecision() to get called");

  win.document.getElementById("certmanager").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  MockRegistrar.unregister(clientAuthRememberServiceCID);
});

add_task(async function testDeletingRememberedDecisions() {
  let clientAuthDialogsCID = MockRegistrar.register(
    "@mozilla.org/nsClientAuthDialogs;1",
    gClientAuthDialogs
  );
  let cars = Cc["@mozilla.org/security/clientAuthRememberService;1"].getService(
    Ci.nsIClientAuthRememberService
  );

  await openRequireClientCert();
  Assert.ok(
    gClientAuthDialogs.chooseCertificateCalled,
    "chooseCertificate should have been called if visiting 'requireclientcert.example.com' for the first time"
  );

  await openRequireClientCert();
  Assert.ok(
    !gClientAuthDialogs.chooseCertificateCalled,
    "chooseCertificate should not have been called if visiting 'requireclientcert.example.com' for the second time"
  );

  await openRequireClientCert2();
  Assert.ok(
    gClientAuthDialogs.chooseCertificateCalled,
    "chooseCertificate should have been called if visiting'requireclientcert-2.example.com' for the first time"
  );

  let originAttributes = { privateBrowsingId: 0 };
  cars.deleteDecisionsByHost("requireclientcert.example.com", originAttributes);

  await openRequireClientCert();
  Assert.ok(
    gClientAuthDialogs.chooseCertificateCalled,
    "chooseCertificate should have been called after removing all remembered decisions for 'requireclientcert.example.com'"
  );

  await openRequireClientCert2();
  Assert.ok(
    !gClientAuthDialogs.chooseCertificateCalled,
    "chooseCertificate should not have been called if visiting 'requireclientcert-2.example.com' for the second time"
  );

  MockRegistrar.unregister(clientAuthDialogsCID);
});
