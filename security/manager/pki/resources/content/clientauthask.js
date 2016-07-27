/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

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
 * The param block to get params from and set results on.
 * @type nsIDialogParamBlock
 */
var dialogParams;
/**
 * The checkbox storing whether the user wants to remember the selected cert.
 * @type nsIDOMXULCheckboxElement
 */
var rememberBox;

function onLoad() {
  dialogParams = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);

  bundle = document.getElementById("pippki_bundle");
  let rememberSetting =
    Services.prefs.getBoolPref("security.remember_cert_checkbox_default_setting");

  rememberBox = document.getElementById("rememberBox");
  rememberBox.label = bundle.getString("clientAuthRemember");
  rememberBox.checked = rememberSetting;

  let hostname = dialogParams.GetString(0);
  let org = dialogParams.GetString(1);
  let issuerOrg = dialogParams.GetString(2);
  let port = dialogParams.GetInt(0);
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
  certArray = dialogParams.objects.queryElementAt(0, Ci.nsIArray);
  for (let i = 0; i < certArray.length; i++) {
    let menuItemNode = document.createElement("menuitem");
    let cert = certArray.queryElementAt(i, Ci.nsIX509Cert);
    let nickAndSerial =
      bundle.getFormattedString("clientAuthNickAndSerial",
                                [cert.nickname, cert.serialNumber]);
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
  // Signal that the user accepted.
  dialogParams.SetInt(0, 1);
  let index = parseInt(document.getElementById("nicknames").value);
  // Signal the index of the selected cert in the list of cert nicknames
  // provided.
  dialogParams.SetInt(1, index);
  // Signal whether the user wanted to remember the selection.
  dialogParams.SetInt(2, rememberBox.checked);
  return true;
}

function doCancel() {
  // Signal that the user cancelled.
  dialogParams.SetInt(0, 0);
  // Signal whether the user wanted to remember the "selection".
  dialogParams.SetInt(2, rememberBox.checked);
  return true;
}
