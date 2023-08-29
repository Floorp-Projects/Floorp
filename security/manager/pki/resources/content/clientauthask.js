/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const { parse, pemToDER } = ChromeUtils.importESModule(
  "chrome://global/content/certviewer/certDecoder.mjs"
);

/**
 * @file Implements the functionality of clientauthask.xhtml: a dialog that allows
 *       a user pick a client certificate for TLS client authentication.
 * @param {object} window.arguments.0
 *           An Object with the properties:
 *              {String} hostname
 *                 The hostname of the server requesting client authentication.
 *              {Array<nsIX509Cert>} certArray
 *                 Array of certificates the user can choose from
 *              {Object} retVal
 *                 Object to set the return values of calling the dialog on.
 *                 See ClientAuthAskReturnValues.
 */

/**
 * @typedef ClientAuthAskReturnValues
 * @type {object}
 * @property {nsIX509Cert} cert
 *           The certificate, if chosen. null otherwise.
 * @property {boolean} rememberDecision
 *           Set to true if the user wanted their cert selection to be
 *           remembered, false otherwise.
 */

/**
 * The array of certs the user can choose from.
 *
 * @type {Array<nsIX509Cert>}
 */
var certArray;

/**
 * The checkbox storing whether the user wants to remember the selected cert.
 *
 * @type {HTMLInputElement} Element checkbox, has to have |checked| property.
 */
var rememberBox;

async function onLoad() {
  let rememberSetting = Services.prefs.getBoolPref(
    "security.remember_cert_checkbox_default_setting"
  );
  rememberBox = document.getElementById("rememberBox");
  rememberBox.checked = rememberSetting;

  certArray = window.arguments[0].certArray;

  document.l10n.setAttributes(
    document.getElementById("clientAuthSiteIdentification"),
    "client-auth-site-identification",
    { hostname: window.arguments[0].hostname }
  );

  let selectElement = document.getElementById("nicknames");
  for (let i = 0; i < certArray.length; i++) {
    let menuItemNode = document.createXULElement("menuitem");
    let cert = certArray[i];
    let nickAndSerial = `${cert.displayName} [${cert.serialNumber}]`;
    menuItemNode.setAttribute("value", i);
    menuItemNode.setAttribute("label", nickAndSerial); // This is displayed.
    selectElement.menupopup.appendChild(menuItemNode);
    if (i == 0) {
      selectElement.selectedItem = menuItemNode;
    }
  }

  await setDetails();
  document.addEventListener("dialogaccept", doOK);
  document.addEventListener("dialogcancel", doCancel);

  Services.obs.notifyObservers(
    document.getElementById("certAuthAsk"),
    "cert-dialog-loaded"
  );
}

/**
 * Populates the details section with information concerning the selected cert.
 */
async function setDetails() {
  let index = parseInt(document.getElementById("nicknames").value);
  let cert = certArray[index];
  document.l10n.setAttributes(
    document.getElementById("clientAuthCertDetailsIssuedTo"),
    "client-auth-cert-details-issued-to",
    { issuedTo: cert.subjectName }
  );
  document.l10n.setAttributes(
    document.getElementById("clientAuthCertDetailsSerialNumber"),
    "client-auth-cert-details-serial-number",
    { serialNumber: cert.serialNumber }
  );
  const formatter = new Intl.DateTimeFormat(undefined, {
    dateStyle: "medium",
    timeStyle: "long",
  });
  document.l10n.setAttributes(
    document.getElementById("clientAuthCertDetailsValidityPeriod"),
    "client-auth-cert-details-validity-period",
    {
      notBefore: formatter.format(new Date(cert.validity.notBefore / 1000)),
      notAfter: formatter.format(new Date(cert.validity.notAfter / 1000)),
    }
  );
  let parsedCert = await parse(pemToDER(cert.getBase64DERString()));
  let keyUsages = parsedCert.ext.keyUsages;
  if (keyUsages && keyUsages.purposes.length) {
    document.l10n.setAttributes(
      document.getElementById("clientAuthCertDetailsKeyUsages"),
      "client-auth-cert-details-key-usages",
      { keyUsages: keyUsages.purposes.join(", ") }
    );
  }
  let emailAddresses = cert.getEmailAddresses();
  if (emailAddresses.length) {
    document.l10n.setAttributes(
      document.getElementById("clientAuthCertDetailsEmailAddresses"),
      "client-auth-cert-details-email-addresses",
      { emailAddresses: emailAddresses.join(", ") }
    );
  }
  document.l10n.setAttributes(
    document.getElementById("clientAuthCertDetailsIssuedBy"),
    "client-auth-cert-details-issued-by",
    { issuedBy: cert.issuerName }
  );
  document.l10n.setAttributes(
    document.getElementById("clientAuthCertDetailsStoredOn"),
    "client-auth-cert-details-stored-on",
    { storedOn: cert.tokenName }
  );
}

async function onCertSelected() {
  await setDetails();
}

function doOK() {
  let { retVals } = window.arguments[0];
  let index = parseInt(document.getElementById("nicknames").value);
  let cert = certArray[index];
  retVals.cert = cert;
  retVals.rememberDecision = rememberBox.checked;
}

function doCancel() {
  let { retVals } = window.arguments[0];
  retVals.cert = null;
  retVals.rememberDecision = rememberBox.checked;
}
