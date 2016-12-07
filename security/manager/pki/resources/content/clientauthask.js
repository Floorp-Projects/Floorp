/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements the functionality of clientauthask.xul: a dialog that allows
 *       a user pick a client certificate for TLS client authentication.
 * @argument {String} window.arguments[0]
 *           The hostname of the server requesting client authentication.
 * @argument {String} window.arguments[1]
 *           The Organization of the server cert.
 * @argument {String} window.arguments[2]
 *           The Organization of the issuer of the server cert.
 * @argument {Number} window.arguments[3]
 *           The port of the server.
 * @argument {nsISupports} window.arguments[4]
 *           List of certificates the user can choose from, queryable to
 *           nsIArray<nsIX509Cert>.
 * @argument {nsISupports} window.arguments[5]
 *           Object to set the return values of calling the dialog on, queryable
 *           to the underlying type of ClientAuthAskReturnValues.
 */

/**
 * @typedef ClientAuthAskReturnValues
 * @type nsIWritablePropertyBag2
 * @property {Boolean} certChosen
 *           Set to true if the user chose a cert and accepted the dialog, false
 *           otherwise.
 * @property {Boolean} rememberSelection
 *           Set to true if the user wanted their cert selection to be
 *           remembered, false otherwise.
 * @property {Number} selectedIndex
 *           The index the chosen cert is at for the given cert list. Undefined
 *           value if |certChosen| is not true.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

/**
 * The pippki <stringbundle> element.
 * @type <stringbundle>
 */
var bundle;
/**
 * The array of certs the user can choose from.
 * @type nsIArray<nsIX509Cert>
 */
var certArray;
/**
 * The checkbox storing whether the user wants to remember the selected cert.
 * @type nsIDOMXULCheckboxElement
 */
var rememberBox;

function onLoad() {
  bundle = document.getElementById("pippki_bundle");
  let rememberSetting =
    Services.prefs.getBoolPref("security.remember_cert_checkbox_default_setting");

  rememberBox = document.getElementById("rememberBox");
  rememberBox.label = bundle.getString("clientAuthRemember");
  rememberBox.checked = rememberSetting;

  let hostname = window.arguments[0];
  let org = window.arguments[1];
  let issuerOrg = window.arguments[2];
  let port = window.arguments[3];
  let formattedOrg = bundle.getFormattedString("clientAuthMessage1", [org]);
  let formattedIssuerOrg = bundle.getFormattedString("clientAuthMessage2",
                                                     [issuerOrg]);
  let formattedHostnameAndPort =
    bundle.getFormattedString("clientAuthHostnameAndPort",
                              [hostname, port.toString()]);
  setText("hostname", formattedHostnameAndPort);
  setText("organization", formattedOrg);
  setText("issuer", formattedIssuerOrg);

  let selectElement = document.getElementById("nicknames");
  certArray = window.arguments[4].QueryInterface(Ci.nsIArray);
  for (let i = 0; i < certArray.length; i++) {
    let menuItemNode = document.createElement("menuitem");
    let cert = certArray.queryElementAt(i, Ci.nsIX509Cert);
    let nickAndSerial =
      bundle.getFormattedString("clientAuthNickAndSerial",
                                [cert.displayName, cert.serialNumber]);
    menuItemNode.setAttribute("value", i);
    menuItemNode.setAttribute("label", nickAndSerial); // This is displayed.
    selectElement.firstChild.appendChild(menuItemNode);
    if (i == 0) {
      selectElement.selectedItem = menuItemNode;
    }
  }

  setDetails();
}

/**
 * Populates the details section with information concerning the selected cert.
 */
function setDetails() {
  let index = parseInt(document.getElementById("nicknames").value);
  let cert = certArray.queryElementAt(index, Ci.nsIX509Cert);

  let detailLines = [
    bundle.getFormattedString("clientAuthIssuedTo", [cert.subjectName]),
    bundle.getFormattedString("clientAuthSerial", [cert.serialNumber]),
    bundle.getFormattedString("clientAuthValidityPeriod",
                              [cert.validity.notBeforeLocalTime,
                               cert.validity.notAfterLocalTime]),
  ];
  let keyUsages = cert.keyUsages;
  if (keyUsages) {
    detailLines.push(bundle.getFormattedString("clientAuthKeyUsages",
                                               [keyUsages]));
  }
  let emailAddresses = cert.getEmailAddresses({});
  if (emailAddresses.length > 0) {
    let joinedAddresses = emailAddresses.join(", ");
    detailLines.push(bundle.getFormattedString("clientAuthEmailAddresses",
                                               [joinedAddresses]));
  }
  detailLines.push(bundle.getFormattedString("clientAuthIssuedBy",
                                             [cert.issuerName]));
  detailLines.push(bundle.getFormattedString("clientAuthStoredOn",
                                             [cert.tokenName]));

  document.getElementById("details").value = detailLines.join("\n");
}

function onCertSelected() {
  setDetails();
}

function doOK() {
  let retVals = window.arguments[5].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("certChosen", true);
  let index = parseInt(document.getElementById("nicknames").value);
  retVals.setPropertyAsUint32("selectedIndex", index);
  retVals.setPropertyAsBool("rememberSelection", rememberBox.checked);
  return true;
}

function doCancel() {
  let retVals = window.arguments[5].QueryInterface(Ci.nsIWritablePropertyBag2);
  retVals.setPropertyAsBool("certChosen", false);
  retVals.setPropertyAsBool("rememberSelection", rememberBox.checked);
  return true;
}
