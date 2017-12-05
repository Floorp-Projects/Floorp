/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

var gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                .getService(Ci.nsIX509CertDB);
/**
 * Cert to edit the trust of.
 * @type nsIX509Cert
 */
var gCert;

/**
 * onload() handler.
 */
function onLoad() {
  gCert = window.arguments[0];

  let bundle = document.getElementById("pippki_bundle");
  setText("certmsg",
          bundle.getFormattedString("editTrustCA", [gCert.commonName]));

  let sslCheckbox = document.getElementById("trustSSL");
  sslCheckbox.checked = gCertDB.isCertTrusted(gCert, Ci.nsIX509Cert.CA_CERT,
                                              Ci.nsIX509CertDB.TRUSTED_SSL);

  let emailCheckbox = document.getElementById("trustEmail");
  emailCheckbox.checked = gCertDB.isCertTrusted(gCert, Ci.nsIX509Cert.CA_CERT,
                                                Ci.nsIX509CertDB.TRUSTED_EMAIL);
}

/**
 * ondialogaccept() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogAccept() {
  let sslCheckbox = document.getElementById("trustSSL");
  let emailCheckbox = document.getElementById("trustEmail");
  let trustSSL = sslCheckbox.checked ? Ci.nsIX509CertDB.TRUSTED_SSL : 0;
  let trustEmail = emailCheckbox.checked ? Ci.nsIX509CertDB.TRUSTED_EMAIL : 0;

  gCertDB.setCertTrust(gCert, Ci.nsIX509Cert.CA_CERT, trustSSL | trustEmail);
  return true;
}
