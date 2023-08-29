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
  let certArray = [cert];
  let retVals = { cert: undefined, rememberDecision: undefined };
  let win = window.openDialog(
    "chrome://pippki/content/clientauthask.xhtml",
    "",
    "",
    { hostname: TEST_HOSTNAME, certArray, retVals }
  );
  return TestUtils.topicObserved("cert-dialog-loaded").then(() => {
    return { win, retVals };
  });
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
async function checkDialogContents(win, notBefore, notAfter) {
  await TestUtils.waitForCondition(() => {
    return win.document
      .getElementById("clientAuthSiteIdentification")
      .textContent.includes(`${TEST_HOSTNAME}`);
  });
  let nicknames = win.document.getElementById("nicknames");
  await TestUtils.waitForCondition(() => {
    return nicknames.label == "Mochitest client [03]";
  });
  await TestUtils.waitForCondition(() => {
    return nicknames.itemCount == 1;
  });
  let subject = win.document.getElementById("clientAuthCertDetailsIssuedTo");
  await TestUtils.waitForCondition(() => {
    return subject.textContent == "Issued to: CN=Mochitest client";
  });
  let serialNum = win.document.getElementById(
    "clientAuthCertDetailsSerialNumber"
  );
  await TestUtils.waitForCondition(() => {
    return serialNum.textContent == "Serial number: 03";
  });
  let validity = win.document.getElementById(
    "clientAuthCertDetailsValidityPeriod"
  );
  await TestUtils.waitForCondition(() => {
    return validity.textContent == `Valid from ${notBefore} to ${notAfter}`;
  });
  let issuer = win.document.getElementById("clientAuthCertDetailsIssuedBy");
  await TestUtils.waitForCondition(() => {
    return (
      issuer.textContent ==
      "Issued by: OU=Profile Guided Optimization,O=Mozilla Testing,CN=Temporary Certificate Authority"
    );
  });
  let tokenName = win.document.getElementById("clientAuthCertDetailsStoredOn");
  await TestUtils.waitForCondition(() => {
    return tokenName.textContent == "Stored on: Software Security Device";
  });
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
  let { win } = await openClientAuthDialog(cert);
  await checkDialogContents(
    win,
    formatter.format(new Date(cert.validity.notBefore / 1000)),
    formatter.format(new Date(cert.validity.notAfter / 1000))
  );
  await BrowserTestUtils.closeWindow(win);
});

// Test that the right values are returned when the dialog is accepted.
add_task(async function testAcceptDialogReturnValues() {
  let { win, retVals } = await openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = true;
  info("Accepting dialog");
  win.document.getElementById("certAuthAsk").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  is(retVals.cert, cert, "cert should be returned as chosen cert");
  ok(
    retVals.rememberDecision,
    "Return value should signal 'Remember this decision' checkbox was checked"
  );
});

// Test that the right values are returned when the dialog is canceled.
add_task(async function testCancelDialogReturnValues() {
  let { win, retVals } = await openClientAuthDialog(cert);
  win.document.getElementById("rememberBox").checked = false;
  info("Canceling dialog");
  win.document.getElementById("certAuthAsk").cancelDialog();
  await BrowserTestUtils.windowClosed(win);

  ok(
    !retVals.cert,
    "Return value should signal user did not choose a certificate"
  );
  ok(
    !retVals.rememberDecision,
    "Return value should signal 'Remember this decision' checkbox was unchecked"
  );
});
