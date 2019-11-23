// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests various aspects of the cert delete confirmation dialog.
// Among other things, tests that for each type of cert that can be deleted:
// 1. The various lines of explanation text are correctly set.
// 2. The implementation correctly falls back through multiple cert attributes
//    to determine what to display to represent a cert.

/**
 * An array of tree items corresponding to TEST_CASES.
 * @type nsICertTreeItem[]
 */
var gCertArray = [];

const FAKE_HOST_PORT = "Fake host and port";

/**
 * @typedef {TestCase}
 * @type Object
 * @property {String} certFilename
 *           Filename of the cert, or null if we don't want to import a cert for
 *           this test case (i.e. we expect the hostPort attribute of
 *           nsICertTreeItem to be used).
 * @property {String} expectedDisplayString
 *           The string we expect the UI to display to represent the given cert.
 * @property {String} expectedSerialNumber
 *           The serial number we expect the UI to display if it exists.
 */

/**
 * A list of test cases representing certs that get "deleted".
 * @type TestCase[]
 */
const TEST_CASES = [
  {
    certFilename: null,
    expectedDisplayString: FAKE_HOST_PORT,
    expectedSerialNumber: null,
  },
  {
    certFilename: "has-cn.pem",
    expectedDisplayString: "Foo",
    expectedSerialNumber: null,
  },
  {
    certFilename: "has-ou.pem",
    expectedDisplayString: "Bar",
    expectedSerialNumber: null,
  },
  {
    certFilename: "has-o.pem",
    expectedDisplayString: "Baz",
    expectedSerialNumber: null,
  },
  {
    certFilename: "has-non-empty-subject.pem",
    expectedDisplayString: "C=US",
    expectedSerialNumber: null,
  },
  {
    certFilename: "has-empty-subject.pem",
    expectedDisplayString: "Certificate with serial number: 0A",
    expectedSerialNumber: "0A",
  },
];

/**
 * Opens the cert delete confirmation dialog.
 *
 * @param {String} tabID
 *        The ID of the cert category tab the certs to delete belong to.
 * @returns {Promise}
 *          A promise that resolves when the dialog has finished loading, with
 *          an array consisting of:
 *            1. The window of the opened dialog.
 *            2. The return value object passed to the dialog.
 */
function openDeleteCertConfirmDialog(tabID) {
  let retVals = {
    deleteConfirmed: false,
  };
  let win = window.openDialog(
    "chrome://pippki/content/deletecert.xul",
    "",
    "",
    tabID,
    gCertArray,
    retVals
  );
  return new Promise((resolve, reject) => {
    win.addEventListener(
      "load",
      function() {
        executeSoon(() => resolve([win, retVals]));
      },
      { once: true }
    );
  });
}

add_task(async function setup() {
  for (let testCase of TEST_CASES) {
    let cert = null;
    if (testCase.certFilename) {
      cert = await readCertificate(testCase.certFilename, ",,");
    }
    let certTreeItem = {
      hostPort: FAKE_HOST_PORT,
      cert,
      QueryInterface: ChromeUtils.generateQI(["nsICertTreeItem"]),
    };
    gCertArray.push(certTreeItem);
  }
});

/**
 * Test helper for the below test cases.
 *
 * @param {String} tabID
 *        ID of the cert category tab the certs to delete belong to.
 * @param {String} expectedTitleL10nId
 *        The L10nId of title the dialog is expected to have.
 * @param {String} expectedConfirmL10nId
 *        The l10n id of confirmation message the dialog expected to show.
 * @param {String} expectedImpactL10nId
 *        The l10n id of impact the dialog expected to show.
 */
async function testHelper(
  tabID,
  expectedTitleL10nId,
  expectedConfirmL10nId,
  expectedImpactL10nId
) {
  let [win] = await openDeleteCertConfirmDialog(tabID);
  let certList = win.document.getElementById("certlist");

  Assert.deepEqual(
    win.document.l10n.getAttributes(win.document.documentElement),
    expectedTitleL10nId,
    `Actual and expected titles should match for ${tabID}`
  );
  let confirm = win.document.getElementById("confirm");
  Assert.deepEqual(
    win.document.l10n.getAttributes(confirm),
    expectedConfirmL10nId,
    `Actual and expected confirm message should match for ${tabID}`
  );
  let impact = win.document.getElementById("impact");
  Assert.deepEqual(
    win.document.l10n.getAttributes(impact),
    expectedImpactL10nId,
    `Actual and expected impact should match for ${tabID}`
  );

  Assert.equal(
    certList.itemCount,
    TEST_CASES.length,
    `No. of certs displayed should match for ${tabID}`
  );
  for (let i = 0; i < certList.itemCount; i++) {
    let item = certList.getItemAtIndex(i);
    if (TEST_CASES[i].expectedSerialNumber == null) {
      Assert.equal(
        item.label,
        TEST_CASES[i].expectedDisplayString,
        "Actual and expected display string should match for " +
          `index ${i} for ${tabID}`
      );
    } else {
      Assert.deepEqual(
        win.document.l10n.getAttributes(item.children[0]),
        {
          id: "cert-with-serial",
          args: { serialNumber: TEST_CASES[i].expectedSerialNumber },
        },
        "Actual and expected display string should match for " +
          `index ${i} for ${tabID}`
      );
    }
  }

  await BrowserTestUtils.closeWindow(win);
}

// Test deleting certs from the "Your Certificates" tab.
add_task(async function testDeletePersonalCerts() {
  const expectedTitleL10nId = { id: "delete-user-cert-title", args: null };
  const expectedConfirmL10nId = { id: "delete-user-cert-confirm", args: null };
  const expectedImpactL10nId = { id: "delete-user-cert-impact", args: null };
  await testHelper(
    "mine_tab",
    expectedTitleL10nId,
    expectedConfirmL10nId,
    expectedImpactL10nId
  );
});

// Test deleting certs from the "People" tab.
add_task(async function testDeleteOtherPeopleCerts() {
  const expectedTitleL10nId = { id: "delete-email-cert-title", args: null };
  // ’ doesn't seem to work when embedded in the following literals, which is
  // why escape codes are used instead.
  const expectedConfirmL10nId = { id: "delete-email-cert-confirm", args: null };
  const expectedImpactL10nId = { id: "delete-email-cert-impact", args: null };
  await testHelper(
    "others_tab",
    expectedTitleL10nId,
    expectedConfirmL10nId,
    expectedImpactL10nId
  );
});

// Test deleting certs from the "Servers" tab.
add_task(async function testDeleteServerCerts() {
  const expectedTitleL10nId = { id: "delete-ssl-cert-title", args: null };
  const expectedConfirmL10nId = { id: "delete-ssl-cert-confirm", args: null };
  const expectedImpactL10nId = { id: "delete-ssl-cert-impact", args: null };
  await testHelper(
    "websites_tab",
    expectedTitleL10nId,
    expectedConfirmL10nId,
    expectedImpactL10nId
  );
});

// Test deleting certs from the "Authorities" tab.
add_task(async function testDeleteCACerts() {
  const expectedTitleL10nId = { id: "delete-ca-cert-title", args: null };
  const expectedConfirmL10nId = { id: "delete-ca-cert-confirm", args: null };
  const expectedImpactL10nId = { id: "delete-ca-cert-impact", args: null };
  await testHelper(
    "ca_tab",
    expectedTitleL10nId,
    expectedConfirmL10nId,
    expectedImpactL10nId
  );
});

// Test that the right values are returned when the dialog is accepted.
add_task(async function testAcceptDialogReturnValues() {
  let [win, retVals] = await openDeleteCertConfirmDialog(
    "ca_tab" /* arbitrary */
  );
  info("Accepting dialog");
  win.document.getElementById("deleteCertificate").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(
    retVals.deleteConfirmed,
    "Return value should signal user accepted"
  );
});

// Test that the right values are returned when the dialog is canceled.
add_task(async function testCancelDialogReturnValues() {
  let [win, retVals] = await openDeleteCertConfirmDialog(
    "ca_tab" /* arbitrary */
  );
  info("Canceling dialog");
  win.document.getElementById("deleteCertificate").cancelDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(
    !retVals.deleteConfirmed,
    "Return value should signal user did not accept"
  );
});
