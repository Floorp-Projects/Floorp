/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Repeatedly opens the certificate viewer dialog with various certificates and
// determines that the viewer correctly identifies either what usages those
// certificates are valid for or what errors prevented the certificates from
// being verified.

var { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});

add_task(async function testCAandTitle() {
  let cert = await readCertificate("ca.pem", "CTu,CTu,CTu");
  let win = await displayCertificate(cert);
  checkUsages(win, ["SSL Certificate Authority"]);

  // There's no real need to test the title for every cert, so we just test it
  // once here.
  Assert.equal(win.document.title, "Certificate Viewer: \u201Cca\u201D",
               "Actual and expected title should match");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testSSLEndEntity() {
  let cert = await readCertificate("ssl-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkUsages(win, ["SSL Server Certificate", "SSL Client Certificate"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testEmailEndEntity() {
  let cert = await readCertificate("email-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkUsages(win, ["Email Recipient Certificate", "Email Signer Certificate"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testCodeSignEndEntity() {
  let cert = await readCertificate("code-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkUsages(win, ["Object Signer"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testExpired() {
  let cert = await readCertificate("expired-ca.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, "Could not verify this certificate because it has expired.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testIssuerExpired() {
  let cert = await readCertificate("ee-from-expired-ca.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the CA certificate " +
             "is invalid.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testUnknownIssuer() {
  let cert = await readCertificate("unknown-issuer.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the issuer is " +
             "unknown.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testInsecureAlgo() {
  let cert = await readCertificate("md5-ee.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because it was signed using " +
             "a signature algorithm that was disabled because that algorithm " +
             "is not secure.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testUntrusted() {
  let cert = await readCertificate("untrusted-ca.pem", "p,p,p");
  let win = await displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because it is not trusted.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testUntrustedIssuer() {
  let cert = await readCertificate("ee-from-untrusted-ca.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the issuer is not " +
             "trusted.");
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testRevoked() {
  // Note that there's currently no way to un-do this. This should only be a
  // problem if another test re-uses a certificate with this same key (perhaps
  // likely) and subject (less likely).
  let certBlocklist = Cc["@mozilla.org/security/certblocklist;1"]
                        .getService(Ci.nsICertBlocklist);
  certBlocklist.revokeCertBySubjectAndPubKey(
    "MBIxEDAOBgNVBAMMB3Jldm9rZWQ=", // CN=revoked
    "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8="); // hash of the shared key
  let cert = await readCertificate("revoked.pem", ",,");
  let win = await displayCertificate(cert);
  // As of bug 1312827, OneCRL only applies to TLS web server certificates, so
  // this certificate will actually verify successfully for every end-entity
  // usage except TLS web server.
  checkUsages(win, ["Email Recipient Certificate", "Email Signer Certificate",
                    "Object Signer", "SSL Client Certificate"]);
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function testInvalid() {
  // This certificate has a keyUsage extension asserting cRLSign and
  // keyCertSign, but it doesn't have a basicConstraints extension. This
  // shouldn't be valid for any usage. Sadly, we give a pretty lame error
  // message in this case.
  let cert = await readCertificate("invalid.pem", ",,");
  let win = await displayCertificate(cert);
  checkError(win, "Could not verify this certificate for unknown reasons.");
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
 * @return {String[]}
 *         An array of strings describing the usages the certificate is valid
 *         for.
 */
function getUsages(win) {
  let determinedUsages = [];
  let verifyInfoBox = win.document.getElementById("verify_info_box");
  Array.from(verifyInfoBox.children).forEach(child => {
    if (child.getAttribute("hidden") != "true" &&
        child.getAttribute("id") != "verified") {
      determinedUsages.push(child.getAttribute("value"));
    }
  });
  return determinedUsages.sort();
}

/**
 * Given a certificate viewer window, returns the error string describing a
 * failure encountered when determining the certificate's usages. It will be
 * "This certificate has been verified for the following uses:" when the
 * certificate has successfully verified for at least one usage.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @return {String}
 *         A string describing the error encountered, or the success message if
 *         the certificate is valid for at least one usage.
 */
function getError(win) {
  return win.document.getElementById("verified").textContent;
}

/**
 * Given a certificate viewer window and an array of expected usage
 * descriptions, verifies that the window is actually showing that the
 * certificate has validated for those usages.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @param {String[]} usages
 *        An array of expected usage descriptions.
 */
function checkUsages(win, usages) {
  Assert.equal(getError(win),
               "This certificate has been verified for the following uses:",
               "should have successful verification message");
  let determinedUsages = getUsages(win);
  usages.sort();
  Assert.equal(determinedUsages.length, usages.length,
               "number of usages as determined by cert viewer should be equal");
  while (usages.length > 0) {
    Assert.equal(determinedUsages.pop(), usages.pop(),
                 "usages as determined by cert viewer should be equal");
  }
}

/**
 * Given a certificate viewer window and an expected error, verifies that the
 * window is actually showing that error.
 *
 * @param {window} win
 *        The certificate viewer window.
 * @param {String} error
 *        The expected error message.
 */
function checkError(win, error) {
  let determinedUsages = getUsages(win);
  Assert.equal(determinedUsages.length, 0,
               "should not have any successful usages in error case");
  Assert.equal(getError(win), error,
               "determined error should be the same as expected error");
}
