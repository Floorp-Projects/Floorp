/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

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

  document.addEventListener("dialogaccept", onDialogAccept);

  let certMsg = document.getElementById("certmsg");
  document.l10n.setAttributes(certMsg, "edit-trust-ca", { certName: gCert.commonName});

  let sslCheckbox = document.getElementById("trustSSL");
  sslCheckbox.checked = gCertDB.isCertTrusted(gCert, Ci.nsIX509Cert.CA_CERT,
                                              Ci.nsIX509CertDB.TRUSTED_SSL);

  let emailCheckbox = document.getElementById("trustEmail");
  emailCheckbox.checked = gCertDB.isCertTrusted(gCert, Ci.nsIX509Cert.CA_CERT,
                                                Ci.nsIX509CertDB.TRUSTED_EMAIL);
}

/**
 * ondialogaccept() handler.
 */
function onDialogAccept() {
  let sslCheckbox = document.getElementById("trustSSL");
  let emailCheckbox = document.getElementById("trustEmail");
  let trustSSL = sslCheckbox.checked ? Ci.nsIX509CertDB.TRUSTED_SSL : 0;
  let trustEmail = emailCheckbox.checked ? Ci.nsIX509CertDB.TRUSTED_EMAIL : 0;

  gCertDB.setCertTrust(gCert, Ci.nsIX509Cert.CA_CERT, trustSSL | trustEmail);
}
