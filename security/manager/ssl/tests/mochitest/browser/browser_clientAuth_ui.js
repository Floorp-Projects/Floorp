// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that the client authentication certificate chooser correctly displays
// provided information and correctly returns user input.

const TEST_HOSTNAME = "Test Hostname";
const TEST_ORG = "Test Org";
const TEST_ISSUER_ORG = "Test Issuer Org";
const TEST_PORT = 123;

var certDB = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);
/**
 * Test certificate (i.e. build/pgo/certs/mochitest.client).
 * @type nsIX509Cert
 */
var cert;

/**
 * Opens the client auth cert chooser dialog.
 *
 * @param {nsIX509Cert} cert The cert to pass to the dialog for display.
 * @returns {Promise}
 *          A promise that resolves when the dialog has finished loading, with
 *          an array consisting of:
 *            1. The window of the opened dialog.
 *            2. The nsIDialogParamBlock passed to the dialog.
 */
function openClientAuthDialog(cert) {
  let params = Cc["@mozilla.org/embedcomp/dialogparam;1"]
                 .createInstance(Ci.nsIDialogParamBlock);

  let certList = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  certList.appendElement(cert, false);
  let array = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  array.appendElement(certList, false);
  params.objects = array;

  params.SetString(0, TEST_HOSTNAME);
  params.SetString(1, TEST_ORG);
  params.SetString(2, TEST_ISSUER_ORG);
  params.SetInt(0, TEST_PORT);

  let win = window.openDialog("chrome://pippki/content/clientauthask.xul", "",
                              "", params);
  return new Promise((resolve, reject) => {
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad);
      resolve([win, params]);
    });
  });
}

/**
 * Checks that the contents of the given cert chooser dialog match the details
 * of build/pgo/certs/mochitest.client.
 *
 * @param {window} win The cert chooser window.
 * @param {String} notBefore
 *        The notBeforeLocalTime attribute of mochitest.client.
 * @param {String} notAfter
 *        The notAfterLocalTime attribute of mochitest.client.
 */
function checkDialogContents(win, notBefore, notAfter) {
  Assert.equal(win.document.getElementById("hostname").textContent,
               `${TEST_HOSTNAME}:${TEST_PORT}`,
               "Actual and expected hostname and port should be equal");
  // “ and ” don't seem to work when embedded in the following literals, which
  // is why escape codes are used instead.
  Assert.equal(win.document.getElementById("organization").textContent,
               `Organization: \u201C${TEST_ORG}\u201D`,
               "Actual and expected organization should be equal");
  Assert.equal(win.document.getElementById("issuer").textContent,
               `Issued Under: \u201C${TEST_ISSUER_ORG}\u201D`,
               "Actual and expected issuer organization should be equal");

  Assert.equal(win.document.getElementById("nicknames").label,
               "test client certificate [03]",
               "Actual and expected selected cert nickname and serial should " +
               "be equal");

  let [subject, serialNum, validity, issuer, tokenName] =
    win.document.getElementById("details").value.split("\n");
  Assert.equal(subject, "Issued to: CN=Mochitest client",
               "Actual and expected subject should be equal");
  Assert.equal(serialNum, "Serial number: 03",
               "Actual and expected serial number should be equal");
  Assert.equal(validity, `Valid from ${notBefore} to ${notAfter}`,
               "Actual and expected validity should be equal");
  Assert.equal(issuer,
               "Issued by: CN=Temporary Certificate Authority,O=Mozilla " +
               "Testing,OU=Profile Guided Optimization",
               "Actual and expected issuer should be equal");
  Assert.equal(tokenName, "Stored on: Software Security Device",
               "Actual and expected token name should be equal");
}

add_task(function* setup() {
  cert = certDB.findCertByNickname("test client certificate");
  Assert.notEqual(cert, null, "Should be able to find the test client cert");
});

// Test that the contents of the dialog correspond to the details of the
// provided cert.
add_task(function* testContents() {
  let [win, params] = yield openClientAuthDialog(cert);
  checkDialogContents(win, cert.validity.notBeforeLocalTime,
                      cert.validity.notAfterLocalTime);
  yield BrowserTestUtils.closeWindow(win);
});

// Test that the right values are returned when the dialog is accepted.
add_task(function* testAcceptDialogReturnValues() {
  let [win, params] = yield openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = true;
  info("Accepting dialog");
  win.document.getElementById("certAuthAsk").acceptDialog();
  yield BrowserTestUtils.windowClosed(win);

  Assert.equal(params.GetInt(0), 1,
               "1 should be returned to signal user accepted");
  Assert.equal(params.GetInt(1), 0,
               "0 should be returned as the selected index");
  Assert.equal(params.GetInt(2), 1,
               "1 should be returned as the state of the 'Remember this " +
               "decision' checkbox");
});

// Test that the right values are returned when the dialog is canceled.
add_task(function* testCancelDialogReturnValues() {
  let [win, params] = yield openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = false;
  info("Canceling dialog");
  win.document.getElementById("certAuthAsk").cancelDialog();
  yield BrowserTestUtils.windowClosed(win);

  Assert.equal(params.GetInt(0), 0,
               "0 should be returned to signal user canceled");
  Assert.equal(params.GetInt(2), 0,
               "0 should be returned as the state of the 'Remember this " +
               "decision' checkbox");
});
