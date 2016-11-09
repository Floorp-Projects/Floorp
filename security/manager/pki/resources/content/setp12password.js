/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * @file Implements the functionality of setp12password.xul: a dialog that lets
 *       the user confirm the password to set on a PKCS #12 file.
 * @argument {nsISupports} window.arguments[0]
 *           Object to set the return values of calling the dialog on, queryable
 *           to the underlying type of SetP12PasswordReturnValues.
 */

/**
 * @typedef SetP12PasswordReturnValues
 * @type nsIWritablePropertyBag2
 * @property {Boolean} confirmedPassword
 *           Set to true if the user entered two matching passwords and
 *           confirmed the dialog.
 * @property {String} password
 *           The password the user entered. Undefined value if
 *           |confirmedPassword| is not true.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

/**
 * onload() handler.
 */
function onLoad() {
  // Ensure the first password textbox has focus.
  document.getElementById("pw1").focus();
}

/**
 * ondialogaccept() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogAccept() {
  let password = document.getElementById("pw1").value;

  let retVals = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("confirmedPassword", true);
  retVals.setPropertyAsAString("password", password);
  return true;
}

/**
 * ondialogcancel() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogCancel() {
  let retVals = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("confirmedPassword", false);
  return true;
}

/**
 * Calculates the strength of the given password, suitable for use in updating
 * a progress bar that represents said strength.
 *
 * The strength of the password is calculated by checking the number of:
 *   - Characters
 *   - Numbers
 *   - Non-alphanumeric chars
 *   - Upper case characters
 *
 * @param {String} password
 *        The password to calculate the strength of.
 * @returns {Number}
 *          The strength of the password in the range [0, 100].
 */
function getPasswordStrength(password) {
  let lengthStrength = password.length;
  if (lengthStrength > 5) {
    lengthStrength = 5;
  }

  let nonNumericChars = password.replace(/[0-9]/g, "");
  let numericStrength = password.length - nonNumericChars.length;
  if (numericStrength > 3) {
    numericStrength = 3;
  }

  let nonSymbolChars = password.replace(/\W/g, "");
  let symbolStrength = password.length - nonSymbolChars.length;
  if (symbolStrength > 3) {
    symbolStrength = 3;
  }

  let nonUpperAlphaChars = password.replace(/[A-Z]/g, "");
  let upperAlphaStrength = password.length - nonUpperAlphaChars.length;
  if (upperAlphaStrength > 3) {
    upperAlphaStrength = 3;
  }

  let strength = (lengthStrength * 10) - 20 + (numericStrength * 10) +
                 (symbolStrength * 15) + (upperAlphaStrength * 10);
  if (strength < 0) {
    strength = 0;
  }
  if (strength > 100) {
    strength = 100;
  }

  return strength;
}

/**
 * oninput() handler for both password textboxes.
 *
 * @param {Boolean} recalculatePasswordStrength
 *                  Whether to recalculate the strength of the first password.
 */
function onPasswordInput(recalculatePasswordStrength) {
  let pw1 = document.getElementById("pw1").value;

  if (recalculatePasswordStrength) {
    document.getElementById("pwmeter").value = getPasswordStrength(pw1);
  }

  // Disable the accept button if the two passwords don't match, and enable it
  // if the passwords do match.
  let pw2 = document.getElementById("pw2").value;
  document.documentElement.getButton("accept").disabled = (pw1 != pw2);
}
