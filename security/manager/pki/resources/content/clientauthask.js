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
 * The param block to get params from and set results on.
 * @type nsIDialogParamBlock
 */
var dialogParams;
var itemCount = 0;
/**
 * The checkbox storing whether the user wants to remember the selected cert.
 * @type nsIDOMXULCheckboxElement
 */
var rememberBox;

function onLoad() {
  dialogParams = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);

  let bundle = document.getElementById("pippki_bundle");
  let rememberSetting =
    Services.prefs.getBoolPref("security.remember_cert_checkbox_default_setting");

  rememberBox = document.getElementById("rememberBox");
  rememberBox.label = bundle.getString("clientAuthRemember");
  rememberBox.checked = rememberSetting;

  let cnAndPort = dialogParams.GetString(0);
  let org = dialogParams.GetString(1);
  let issuerOrg = dialogParams.GetString(2);
  let formattedOrg = bundle.getFormattedString("clientAuthMessage1", [org]);
  let formattedIssuerOrg = bundle.getFormattedString("clientAuthMessage2",
                                                     [issuerOrg]);
  setText("hostname", cnAndPort);
  setText("organization", formattedOrg);
  setText("issuer", formattedIssuerOrg);

  let selectElement = document.getElementById("nicknames");
  itemCount = dialogParams.GetInt(0);
  for (let i = 0; i < itemCount; i++) {
    let menuItemNode = document.createElement("menuitem");
    let nick = dialogParams.GetString(i + 3);
    menuItemNode.setAttribute("value", i);
    menuItemNode.setAttribute("label", nick); // this is displayed
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
  let details = dialogParams.GetString(index + itemCount + 3);
  document.getElementById("details").value = details;
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
