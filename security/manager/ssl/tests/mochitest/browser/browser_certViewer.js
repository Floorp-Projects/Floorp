/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Repeatedly opens the certificate viewer dialog with various certificates and
// determines that the viewer correctly identifies either what usages those
// certificates are valid for or what errors prevented the certificates from
// being verified.

var { OS } = Cu.import("resource://gre/modules/osfile.jsm", {});

var certificates = [];

registerCleanupFunction(function() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  certificates.forEach(cert => {
    certdb.deleteCertificate(cert);
  });
});

add_task(function* () {
  let cert = yield readCertificate("ca.pem", "CTu,CTu,CTu");
  let win = yield displayCertificate(cert);
  checkUsages(win, ["SSL Certificate Authority"]);
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("ssl-ee.pem", ",,");
  let win = yield displayCertificate(cert);
  checkUsages(win, ["SSL Server Certificate", "SSL Client Certificate"]);
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("email-ee.pem", ",,");
  let win = yield displayCertificate(cert);
  checkUsages(win, ["Email Recipient Certificate", "Email Signer Certificate"]);
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("code-ee.pem", ",,");
  let win = yield displayCertificate(cert);
  checkUsages(win, ["Object Signer"]);
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("expired-ca.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win, "Could not verify this certificate because it has expired.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("ee-from-expired-ca.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the CA certificate " +
             "is invalid.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("unknown-issuer.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the issuer is " +
             "unknown.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("md5-ee.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because it was signed using " +
             "a signature algorithm that was disabled because that algorithm " +
             "is not secure.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("untrusted-ca.pem", "p,p,p");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because it is not trusted.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  let cert = yield readCertificate("ee-from-untrusted-ca.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because the issuer is not " +
             "trusted.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  // Note that there's currently no way to un-do this. This should only be a
  // problem if another test re-uses a certificate with this same key (perhaps
  // likely) and subject (less likely).
  let certBlocklist = Cc["@mozilla.org/security/certblocklist;1"]
                        .getService(Ci.nsICertBlocklist);
  certBlocklist.revokeCertBySubjectAndPubKey(
    "MBIxEDAOBgNVBAMMB3Jldm9rZWQ=", // CN=revoked
    "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8="); // hash of the shared key
  let cert = yield readCertificate("revoked.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win,
             "Could not verify this certificate because it has been revoked.");
  yield BrowserTestUtils.closeWindow(win);
});

add_task(function* () {
  // This certificate has a keyUsage extension asserting cRLSign and
  // keyCertSign, but it doesn't have a basicConstraints extension. This
  // shouldn't be valid for any usage. Sadly, we give a pretty lame error
  // message in this case.
  let cert = yield readCertificate("invalid.pem", ",,");
  let win = yield displayCertificate(cert);
  checkError(win, "Could not verify this certificate for unknown reasons.");
  yield BrowserTestUtils.closeWindow(win);
});

/**
 * Helper for readCertificate.
 */
function pemToBase64(pem) {
  return pem.replace(/-----BEGIN CERTIFICATE-----/, "")
            .replace(/-----END CERTIFICATE-----/, "")
            .replace(/[\r\n]/g, "");
}

/**
 * Given the filename of a certificate, returns a promise that will resolve with
 * a handle to the certificate when that certificate has been read and imported
 * with the given trust settings.
 *
 * @param {String} filename
 *        The filename of the certificate (assumed to be in the same directory).
 * @param {String} trustString
 *        A string describing how the certificate should be trusted (see
 *        `certutil -A --help`).
 * @return {Promise}
 *         A promise that will resolve with a handle to the certificate.
 */
function readCertificate(filename, trustString) {
  return OS.File.read(getTestFilePath(filename)).then(data => {
    let decoder = new TextDecoder();
    let pem = decoder.decode(data);
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
    let base64 = pemToBase64(pem);
    certdb.addCertFromBase64(base64, trustString, "unused");
    let cert = certdb.constructX509FromBase64(base64);
    certificates.push(cert); // so we remember to delete this at the end
    return cert;
  }, error => { throw error; });
}

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
  let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  array.appendElement(certificate, false);
  let params = Cc["@mozilla.org/embedcomp/dialogparam;1"]
                 .createInstance(Ci.nsIDialogParamBlock);
  params.objects = array;
  let win = window.openDialog("chrome://pippki/content/certViewer.xul", "",
                              "", params);
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
