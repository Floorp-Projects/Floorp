/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Repeatedly opens the certificate viewer dialog with various certificates and
// determines that the viewer correctly identifies either what usages those
// certificates are valid for or what errors prevented the certificates from
// being verified.

var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
var { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

add_task(async function testCAandTitle() {
  let cert = await readCertificate("ca.pem", "CTu,CTu,CTu");
  let win = await displayCertificate(cert);
  checkUsages(win, [{id: "verify-ssl-ca"}]);
  checkDetailsPane(win, ["ca"]);

  // There's no real need to test the title for every cert, so we just test it
  // once here.
  Assert.deepEqual(win.document.l10n.getAttributes(win.document.documentElement),
                {args: { certName: "ca"}, id: "cert-viewer-title"},
               "Actual and expected title should match");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testSSLEndEntity() {
  let cert = await readCertificate("ssl-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkUsages(win, [{id: "verify-ssl-server"}, {id: "verify-ssl-client"}]);
  checkDetailsPane(win, ["ca", "ssl-ee"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testEmailEndEntity() {
  let cert = await readCertificate("email-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkUsages(win, [{id: "verify-email-recip"}, {id: "verify-email-signer"}]);
  checkDetailsPane(win, ["ca", "email-ee"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testCodeSignEndEntity() {
  let cert = await readCertificate("code-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified-unknown"});
  checkDetailsPane(win, ["code-ee"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testExpired() {
  let cert = await readCertificate("expired-ca.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified-cert-expired"});
  checkDetailsPane(win, ["expired-ca"]);
  await BrowserTestUtils.closeWindow(win);

  // These tasks may run in any order, so we run this additional testcase in the
  // same task.
  let eeCert = await readCertificate("ee-from-expired-ca.pem", ",,");
  let eeWin = await displayCertificate(eeCert);
  checkError(eeWin, {id: "cert-not-verified-ca-invalid"});
  checkDetailsPane(eeWin, ["ee-from-expired-ca"]);
  await BrowserTestUtils.closeWindow(eeWin);
});

add_task(async function testUnknownIssuer() {
  let cert = await readCertificate("unknown-issuer.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified-issuer-unknown"});
  checkDetailsPane(win, ["unknown-issuer"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testInsecureAlgo() {
  let cert = await readCertificate("md5-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified_algorithm-disabled"});
  checkDetailsPane(win, ["md5-ee"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testUntrusted() {
  let cert = await readCertificate("untrusted-ca.pem", "p,p,p");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified-cert-not-trusted"});
  checkDetailsPane(win, ["untrusted-ca"]);
  await BrowserTestUtils.closeWindow(win);

  // These tasks may run in any order, so we run this additional testcase in the
  // same task.
  let eeCert = await readCertificate("ee-from-untrusted-ca.pem", ",,");
  let eeWin = await displayCertificate(eeCert);
  checkError(eeWin, {id: "cert-not-verified-issuer-not-trusted"});
  checkDetailsPane(eeWin, ["ee-from-untrusted-ca"]);
  await BrowserTestUtils.closeWindow(eeWin);
});

add_task(async function testRevoked() {
  // Note that there's currently no way to un-do this. This should only be a
  // problem if another test re-uses a certificate with this same key (perhaps
  // likely) and subject (less likely).
  if (AppConstants.MOZ_NEW_CERT_STORAGE) {
    let certBlocklist = Cc["@mozilla.org/security/certstorage;1"]
                          .getService(Ci.nsICertStorage);
    let result = await new Promise((resolve) =>
      certBlocklist.setRevocations([{
        QueryInterface: ChromeUtils.generateQI([Ci.nsISubjectAndPubKeyRevocationState]),
        subject: "MBIxEDAOBgNVBAMMB3Jldm9rZWQ=", // CN=revoked
        pubKey: "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=", // hash of the shared key
        state: Ci.nsICertStorage.STATE_ENFORCE, // yes, we want this to be revoked
      }], resolve));
    Assert.equal(result, Cr.NS_OK, "setting revocation state should succeed");
  } else {
    let certBlocklist = Cc["@mozilla.org/security/certblocklist;1"]
                          .getService(Ci.nsICertBlocklist);
    certBlocklist.revokeCertBySubjectAndPubKey(
      "MBIxEDAOBgNVBAMMB3Jldm9rZWQ=", // CN=revoked
      "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8="); // hash of the shared key
  }
  let cert = await readCertificate("revoked.pem", ",,");
  let win = await displayCertificate(cert);
  // As of bug 1312827, OneCRL only applies to TLS web server certificates, so
  // this certificate will actually verify successfully for every end-entity
  // usage except TLS web server.
  checkUsages(win, [{id: "verify-email-recip"}, {id: "verify-email-signer"}, {id: "verify-ssl-client"}]);
  checkDetailsPane(win, ["ca", "revoked"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testInvalid() {
  // This certificate has a keyUsage extension asserting cRLSign and
  // keyCertSign, but it doesn't have a basicConstraints extension. This
  // shouldn't be valid for any usage. Sadly, we give a pretty lame error
  // message in this case.
  let cert = await readCertificate("invalid.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, {id: "cert-not-verified-unknown"});
  checkDetailsPane(win, ["invalid"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testLongOID() {
  // This certificate has a certificatePolicies extension with a policy with a
  // very long OID. This tests that we don't crash when looking at it.
  let cert = await readCertificate("longOID.pem", ",,");
  let win = await displayCertificate(cert);
  checkDetailsPane(win, ["Long OID"]);
  await BrowserTestUtils.closeWindow(win);
});

/**
 * Given a certificate, returns a promise that will resolve when the certificate
 * viewer has opened is displaying that certificate, and has finished
 * determining its valid usages.
 *
 * @param {nsIX509Cert} certificate
 *        The certificate to view and determine usages for.
 * @return {Promise}
 *         A promise that will resolve with a handle on the opened certificate
 *         viewer window when the usages have been determined.
 */
function displayCertificate(certificate) {
  let win = window.openDialog("chrome://pippki/content/certViewer.xul", "",
                              "", certificate);
  return TestUtils.topicObserved("ViewCertDetails:CertUsagesDone",
                                 (subject, data) => subject == win)
  .then(([subject, data]) => subject, error => { throw error; });
}

/**
 * Given a certificate viewer window, finds the usages the certificate is valid
 * for.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @return {Object[]}
 *         An array of objects including the L10n Ids of strings describing
 *         the usages the certificate is valid for.
 */
function getUsages(win) {
  let determinedUsages = [];
  let verifyInfoBox = win.document.getElementById("verify_info_box");
  Array.from(verifyInfoBox.children).forEach(child => {
    if (child.getAttribute("hidden") != "true" &&
        child.getAttribute("id") != "verified") {
      determinedUsages.push(win.document.l10n.getAttributes(child));
    }
  });
  return determinedUsages.sort(compareL10Ids);
}

/**
 * Given a certificate viewer window, returns the error string describing a
 * failure encountered when determining the certificate's usages. It will be
 * "This certificate has been verified for the following uses:" when the
 * certificate has successfully verified for at least one usage.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @return {Object}
 *         A object with L10n id of the string describing the error encountered,
 *        or the success message if the certificate is valid for at least one usage.
 */
function getError(win) {
  let verified = win.document.getElementById("verified");
  return win.document.l10n.getAttributes(verified);
}

/**
 * Given a certificate viewer window and an array of l10n ids of expected usage
 * descriptions, verifies that the window is actually showing that the
 * certificate has validated for those usages.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @param {Object[]} usagesL10nIds
 *        An array of object with l10n ids of expected usage descriptions.
 */
function checkUsages(win, usagesL10nIds) {
  Assert.deepEqual(getError(win),
               { id: "cert-verified" },
               "should have successful verification message");
  let determinedUsages = getUsages(win);
  usagesL10nIds.sort(compareL10Ids);
  Assert.deepEqual(determinedUsages.length, usagesL10nIds.length,
               "number of usages as determined by cert viewer should be equal");
  while (usagesL10nIds.length > 0) {
    Assert.deepEqual(determinedUsages.pop(), usagesL10nIds.pop(),
                 "usages as determined by cert viewer should be equal");
  }
}

/**
 * Given a certificate viewer window and l10n id of an expected error, verifies that the
 * window is actually showing that error.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @param {Object} errorL10nId
 *        The object with l10n id of expected error message.
 */
function checkError(win, errorL10nId) {
  let determinedUsages = getUsages(win);
  Assert.equal(determinedUsages.length, 0,
               "should not have any successful usages in error case");
  Assert.deepEqual(getError(win), errorL10nId,
               "determined error should be the same as expected error");
}

/**
 * Given a certificate viewer window and an expected list of certificate names,
 * verifies that the certificate details pane of the viewer shows the expected
 * certificates in the expected order.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @param {String[]} names
 *        An array of expected certificate names.
 */
function checkDetailsPane(win, names) {
  let tree = win.document.getElementById("treesetDump");
  let nodes = tree.querySelectorAll("treecell");
  Assert.equal(nodes.length, names.length,
               "details pane: should have the expected number of cert names");
  for (let i = 0; i < names.length; i++) {
    Assert.equal(nodes[i].getAttribute("label"), names[i],
                 "details pain: should have expected cert name");
  }
}

/**
 * Given two objects with l10n id, compare them by l10n id for sorting.
 *
 * @param {Objects} ida,idb
 *        The objects with l10n id.
 * @param {integer}
 *        An integer representing true of false.
 */

function compareL10Ids(ida, idb) {
  if (ida.id < idb.id) {
    return -1;
  } else if (ida.id > idb.id) {
    return 1;
  }
  return 0;
}

