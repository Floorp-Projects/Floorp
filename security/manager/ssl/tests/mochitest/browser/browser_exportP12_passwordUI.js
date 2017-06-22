// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that the UI for setting the password on a to be exported PKCS #12 file:
//   1. Correctly requires the password to be typed in twice as confirmation.
//   2. Calculates and displays the strength of said password.

/**
 * @typedef {TestCase}
 * @type Object
 * @property {String} name
 *           The name of the test case for display purposes.
 * @property {String} password1
 *           The password to enter into the first password textbox.
 * @property {String} password2
 *           The password to enter into the second password textbox.
 * @property {String} strength
 *           The expected strength of the password in the range [0, 100].
 */

/**
 * A list of test cases representing various inputs to the password textboxes.
 * @type TestCase[]
 */
const TEST_CASES = [
  { name: "empty",
    password1: "",
    password2: "",
    strength: "0" },
  { name: "match-weak",
    password1: "foo",
    password2: "foo",
    strength: "10" },
  { name: "match-medium",
    password1: "foo123",
    password2: "foo123",
    strength: "60" },
  { name: "match-strong",
    password1: "fooBARBAZ 1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?一二三",
    password2: "fooBARBAZ 1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?一二三",
    strength: "100" },
  { name: "mismatch-weak",
    password1: "foo",
    password2: "bar",
    strength: "10" },
  { name: "mismatch-medium",
    password1: "foo123",
    password2: "bar",
    strength: "60" },
  { name: "mismatch-strong",
    password1: "fooBARBAZ 1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?一二三",
    password2: "bar",
    strength: "100" },
];

/**
 * Opens the dialog shown to set the password on a PKCS #12 file being exported.
 *
 * @returns {Promise}
 *          A promise that resolves when the dialog has finished loading, with
 *          an array consisting of:
 *            1. The window of the opened dialog.
 *            2. The return value nsIWritablePropertyBag2 passed to the dialog.
 */
function openSetP12PasswordDialog() {
  let returnVals = Cc["@mozilla.org/hash-property-bag;1"]
                     .createInstance(Ci.nsIWritablePropertyBag2);
  let win = window.openDialog("chrome://pippki/content/setp12password.xul", "",
                              "", returnVals);
  return new Promise((resolve, reject) => {
    win.addEventListener("load", function() {
      resolve([win, returnVals]);
    }, {once: true});
  });
}

// Tests that the first password textbox is the element that is initially
// focused.
add_task(async function testFocus() {
  let [win] = await openSetP12PasswordDialog();
  Assert.equal(win.document.activeElement,
               win.document.getElementById("pw1").inputField,
               "First password textbox should have focus");
  await BrowserTestUtils.closeWindow(win);
});

// Tests that the password strength algorithm used is reasonable, and that the
// Accept button is only enabled if the two passwords match.
add_task(async function testPasswordStrengthAndEquality() {
  let [win] = await openSetP12PasswordDialog();
  let password1Textbox = win.document.getElementById("pw1");
  let password2Textbox = win.document.getElementById("pw2");
  let strengthProgressBar = win.document.getElementById("pwmeter");

  for (let testCase of TEST_CASES) {
    password1Textbox.value = testCase.password1;
    password2Textbox.value = testCase.password2;
    // Setting the value of the password textboxes via |.value| apparently
    // doesn't cause the oninput handlers to be called, so we do it here.
    password1Textbox.oninput();
    password2Textbox.oninput();

    Assert.equal(win.document.documentElement.getButton("accept").disabled,
                 password1Textbox.value != password2Textbox.value,
                 "Actual and expected accept button disable state should " +
                 `match for ${testCase.name}`);
    Assert.equal(strengthProgressBar.value, testCase.strength,
                 "Actual and expected strength value should match for" +
                 `${testCase.name}`);
  }

  await BrowserTestUtils.closeWindow(win);
});

// Test that the right values are returned when the dialog is accepted.
add_task(async function testAcceptDialogReturnValues() {
  let [win, retVals] = await openSetP12PasswordDialog();
  const password = "fooBAR 1234567890`~!@#$%^&*()-_=+{[}]|\\:;'\",<.>/?一二三";
  win.document.getElementById("pw1").value = password;
  win.document.getElementById("pw2").value = password;
  info("Accepting dialog");
  win.document.getElementById("setp12password").acceptDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(retVals.get("confirmedPassword"),
            "Return value should signal user confirmed a password");
  Assert.equal(retVals.get("password"), password,
               "Actual and expected password should match");
});

// Test that the right values are returned when the dialog is canceled.
add_task(async function testCancelDialogReturnValues() {
  let [win, retVals] = await openSetP12PasswordDialog();
  info("Canceling dialog");
  win.document.getElementById("setp12password").cancelDialog();
  await BrowserTestUtils.windowClosed(win);

  Assert.ok(!retVals.get("confirmedPassword"),
            "Return value should signal user didn't confirm a password");
});
