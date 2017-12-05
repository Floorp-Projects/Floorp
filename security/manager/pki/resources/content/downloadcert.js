/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements the functionality of downloadcert.xul: a dialog that allows
 *       a user to confirm whether to import a certificate, and if so what trust
 *       to give it.
 * @argument {nsISupports} window.arguments[0]
 *           Certificate to confirm import of, queryable to nsIX509Cert.
 * @argument {nsISupports} window.arguments[1]
 *           Object to set the return values of calling the dialog on, queryable
 *           to the underlying type of DownloadCertReturnValues.
 */

/**
 * @typedef DownloadCertReturnValues
 * @type nsIWritablePropertyBag2
 * @property {Boolean} importConfirmed
 *           Set to true if the user confirmed import of the cert and accepted
 *           the dialog, false otherwise.
 * @property {Boolean} trustForSSL
 *           Set to true if the cert should be trusted for SSL, false otherwise.
 *           Undefined value if |importConfirmed| is not true.
 * @property {Boolean} trustForEmail
 *           Set to true if the cert should be trusted for e-mail, false
 *           otherwise. Undefined value if |importConfirmed| is not true.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

/**
 * The cert to potentially import.
 * @type nsIX509Cert
 */
var gCert;

/**
 * onload() handler.
 */
function onLoad() {
  gCert = window.arguments[0].QueryInterface(Ci.nsIX509Cert);

  let bundle = document.getElementById("pippki_bundle");
  let caName = gCert.commonName;
  if (caName.length == 0) {
    caName = bundle.getString("unnamedCA");
  }

  setText("trustHeader", bundle.getFormattedString("newCAMessage1", [caName]));
}

/**
 * Handler for the "View Cert" button.
 */
function viewCert() {
  viewCertHelper(window, gCert);
}

/**
 * ondialogaccept() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogAccept() {
  let checkSSL = document.getElementById("trustSSL");
  let checkEmail = document.getElementById("trustEmail");

  let retVals = window.arguments[1].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("importConfirmed", true);
  retVals.setPropertyAsBool("trustForSSL", checkSSL.checked);
  retVals.setPropertyAsBool("trustForEmail", checkEmail.checked);
  return true;
}

/**
 * ondialogcancel() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogCancel() {
  let retVals = window.arguments[1].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("importConfirmed", false);
  return true;
}
