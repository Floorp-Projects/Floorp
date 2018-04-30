// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that the cert download/import UI correctly identifies the cert being
// downloaded, and allows the trust of the cert to be specified.

const { MockRegistrar } =
  ChromeUtils.import("resource://testing-common/MockRegistrar.jsm", {});

/**
 * @typedef {TestCase}
 * @type Object
 * @property {String} certFilename
 *           Filename of the cert for this test case.
 * @property {String} expectedDisplayString
 *           The string we expect the UI to display to represent the given cert.
 * @property {nsIX509Cert} cert
 *           Handle to the cert once read in setup().
 */

/**
 * A list of test cases representing certs that get "downloaded".
 * @type TestCase[]
 */
const TEST_CASES = [
  { certFilename: "has-cn.pem",
    expectedDisplayString: "Foo",
    cert: null },
  { certFilename: "has-empty-subject.pem",
    expectedDisplayString: "Certificate Authority (unnamed)",
    cert: null },
];

/**
 * Opens the cert download dialog.
 *
 * @param {nsIX509Cert} cert
 *        The cert to pass to the dialog for display.
 * @returns {Promise}
 *          A promise that resolves when the dialog has finished loading, with
 *          an array consisting of:
 *            1. The window of the opened dialog.
 *            2. The return value nsIWritablePropertyBag2 passed to the dialog.
 */
function openCertDownloadDialog(cert) {
  let returnVals = Cc["@mozilla.org/hash-property-bag;1"]
                     .createInstance(Ci.nsIWritablePropertyBag2);
  let win = window.openDialog("chrome://pippki/content/downloadcert.xul", "",
                              "", cert, returnVals);
  return new Promise((resolve, reject) => {
    win.addEventListener("load", function() {
      executeSoon(() => resolve([win, returnVals]));
    }, {once: true});
  });
}

// Mock implementation of nsICertificateDialogs.
const gCertificateDialogs = {
  expectedCert: null,
  viewCertCallCount: 0,
  confirmDownloadCACert(ctx, cert, trust) {
    Assert.ok(false, "confirmDownloadCACert() should not have been called");
  },
  setPKCS12FilePassword(ctx, password) {
    Assert.ok(false, "setPKCS12FilePassword() should not have been called");
  },
  getPKCS12FilePassword(ctx, password) {
    Assert.ok(false, "getPKCS12FilePassword() should not have been called");
  },
  viewCert(ctx, cert) {
    this.viewCertCallCount++;
    Assert.notEqual(cert, null, "Cert to view should not be null");
    Assert.equal(cert, this.expectedCert,
                 "Actual and expected cert should match");
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsICertificateDialogs])
};

add_task(async function setup() {
  for (let testCase of TEST_CASES) {
    testCase.cert = await readCertificate(testCase.certFilename, ",,");
    Assert.notEqual(testCase.cert, null,
                    `'${testCase.certFilename}' should have been read`);
  }

  let certificateDialogsCID =
    MockRegistrar.register("@mozilla.org/nsCertificateDialogs;1",
                           gCertificateDialogs);
  registerCleanupFunction(() => {
    MockRegistrar.unregister(certificateDialogsCID);
  });
});

// Test that the trust header message corresponds to the provided cert, and that
// the View Cert button launches the cert viewer for the provided cert.
add_task(async function testTrustHeaderAndViewCertButton() {
  for (let testCase of TEST_CASES) {
    let [win] = await openCertDownloadDialog(testCase.cert);
    let expectedTrustHeaderString =
      `Do you want to trust \u201C${testCase.expectedDisplayString}\u201D ` +
      "for the following purposes?";
    Assert.equal(win.document.getElementById("trustHeader").textContent,
                 expectedTrustHeaderString,
                 "Actual and expected trust header text should match for " +
                 `${testCase.certFilename}`);

    gCertificateDialogs.viewCertCallCount = 0;
    gCertificateDialogs.expectedCert = testCase.cert;
    info("Pressing View Cert button");
    win.document.getElementById("viewC-button").doCommand();
    Assert.equal(gCertificateDialogs.viewCertCallCount, 1,
                 "viewCert() should've been called once");

    await BrowserTestUtils.closeWindow(win);
  }
});

// Test that the right values are returned when the dialog is accepted.
add_task(async function testAcceptDialogReturnValues() {
  let [win, retVals] = await openCertDownloadDialog(TEST_CASES[0].cert);
  win.document.getElementById("trustSSL").checked = true;
  win.document.getElementById("trustEmail").checked = false;
  info("Accepting dialog");
  win.document.getElementById("download_cert").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(retVals.get("importConfirmed"),
            "Return value should signal user chose to import the cert");
  Assert.ok(retVals.get("trustForSSL"),
            "Return value should signal SSL trust checkbox was checked");
  Assert.ok(!retVals.get("trustForEmail"),
            "Return value should signal E-mail trust checkbox was unchecked");
});

// Test that the right values are returned when the dialog is canceled.
add_task(async function testCancelDialogReturnValues() {
  let [win, retVals] = await openCertDownloadDialog(TEST_CASES[0].cert);
  info("Canceling dialog");
  win.document.getElementById("download_cert").cancelDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(!retVals.get("importConfirmed"),
            "Return value should signal user chose not to import the cert");
});
