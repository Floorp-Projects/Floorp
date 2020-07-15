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

  registerCleanupFunction(() => {
    MockRegistrar.unregister(clientAuthRememberServiceCID);
  });

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
});
