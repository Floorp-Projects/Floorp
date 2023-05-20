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

var certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);
/**
 * Test certificate (i.e. build/pgo/certs/mochitest.client).
 *
 * @type {nsIX509Cert}
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
 *            2. The return value nsIWritablePropertyBag2 passed to the dialog.
 */
function openClientAuthDialog(cert) {
  let certList = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
  certList.appendElement(cert);

  let returnVals = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  let win = window.openDialog(
    "chrome://pippki/content/clientauthask.xhtml",
    "",
    "",
    TEST_HOSTNAME,
    TEST_ORG,
    TEST_ISSUER_ORG,
    TEST_PORT,
    certList,
    returnVals
  );
  return TestUtils.topicObserved("cert-dialog-loaded").then(() => [
    win,
    returnVals,
  ]);
}

/**
 * Checks that the contents of the given cert chooser dialog match the details
 * of build/pgo/certs/mochitest.client.
 *
 * @param {window} win The cert chooser window.
 * @param {string} notBefore
 *        The formatted notBefore date of mochitest.client.
 * @param {string} notAfter
 *        The formatted notAfter date of mochitest.client.
 */
function checkDialogContents(win, notBefore, notAfter) {
  is(
    win.document.getElementById("hostname").textContent,
    `${TEST_HOSTNAME}:${TEST_PORT}`,
    "Actual and expected hostname and port should be equal"
  );
  is(
    win.document.getElementById("organization").textContent,
    `Organization: “${TEST_ORG}”`,
    "Actual and expected organization should be equal"
  );
  is(
    win.document.getElementById("issuer").textContent,
    `Issued Under: “${TEST_ISSUER_ORG}”`,
    "Actual and expected issuer organization should be equal"
  );

  is(
    win.document.getElementById("nicknames").label,
    "Mochitest client [03]",
    "Actual and expected selected cert nickname and serial should be equal"
  );
  is(
    win.document.getElementById("nicknames").itemCount,
    1,
    "correct number of items"
  );

  let [subject, serialNum, validity, issuer, tokenName] = win.document
    .getElementById("details")
    .value.split("\n");
  is(
    subject,
    "Issued to: CN=Mochitest client",
    "Actual and expected subject should be equal"
  );
  is(
    serialNum,
    "Serial number: 03",
    "Actual and expected serial number should be equal"
  );
  is(
    validity,
    `Valid from ${notBefore} to ${notAfter}`,
    "Actual and expected validity should be equal"
  );
  is(
    issuer,
    "Issued by: OU=Profile Guided Optimization,O=Mozilla Testing,CN=Temporary Certificate Authority",
    "Actual and expected issuer should be equal"
  );
  is(
    tokenName,
    "Stored on: Software Security Device",
    "Actual and expected token name should be equal"
  );
}

function findCertByCommonName(commonName) {
  for (let cert of certDB.getCerts()) {
    if (cert.commonName == commonName) {
      return cert;
    }
  }
  return null;
}

add_setup(async function () {
  cert = findCertByCommonName("Mochitest client");
  isnot(cert, null, "Should be able to find the test client cert");
});

// Test that the contents of the dialog correspond to the details of the
// provided cert.
add_task(async function testContents() {
  const formatter = new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "long",
  });
  let [win] = await openClientAuthDialog(cert);
  checkDialogContents(
    win,
    formatter.format(new Date(cert.validity.notBefore / 1000)),
    formatter.format(new Date(cert.validity.notAfter / 1000))
  );
  await BrowserTestUtils.closeWindow(win);
});

// Test that the right values are returned when the dialog is accepted.
add_task(async function testAcceptDialogReturnValues() {
  let [win, retVals] = await openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = true;
  info("Accepting dialog");
  win.document.getElementById("certAuthAsk").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  ok(
    retVals.get("certChosen"),
    "Return value should signal user chose a certificate"
  );
  is(
    retVals.get("selectedIndex"),
    0,
    "0 should be returned as the selected index"
  );
  ok(
    retVals.get("rememberSelection"),
    "Return value should signal 'Remember this decision' checkbox was checked"
  );
});

// Test that the right values are returned when the dialog is canceled.
add_task(async function testCancelDialogReturnValues() {
  let [win, retVals] = await openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = false;
  info("Canceling dialog");
  win.document.getElementById("certAuthAsk").cancelDialog();
  await BrowserTestUtils.windowClosed(win);

  ok(
    !retVals.get("certChosen"),
    "Return value should signal user did not choose a certificate"
  );
  ok(
    !retVals.get("rememberSelection"),
    "Return value should signal 'Remember this decision' checkbox was unchecked"
  );
});
