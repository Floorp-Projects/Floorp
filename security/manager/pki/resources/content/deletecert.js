/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements the functionality of deletecert.xul: a dialog that allows a
 *       user to confirm whether to delete certain certificates.
 * @argument {String} window.arguments[0]
 *           One of the tab IDs listed in certManager.xul.
 * @argument {nsICertTreeItem[]} window.arguments[1]
 *           An array of cert tree items representing the certs to delete.
 * @argument {DeleteCertReturnValues} window.arguments[2]
 *           Object holding the return values of calling the dialog.
 */

/**
 * @typedef DeleteCertReturnValues
 * @type Object
 * @property {Boolean} deleteConfirmed
 *           Set to true if the user confirmed deletion of the given certs,
 *           false otherwise.
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

/**
 * Returns the most appropriate string to represent the given nsICertTreeItem.
 * @param {nsICertTreeItem} certTreeItem
 *        The item to represent.
 * @returns {String}
 *          A representative string.
 */
function certTreeItemToString(certTreeItem) {
  let cert = certTreeItem.cert;
  if (!cert) {
    return certTreeItem.hostPort;
  }

  const attributes = [
    cert.commonName,
    cert.organizationalUnit,
    cert.organization,
    cert.subjectName,
  ];
  for (let attribute of attributes) {
    if (attribute) {
      return attribute;
    }
  }

  let bundle = document.getElementById("pippki_bundle");
  return bundle.getFormattedString("certWithSerial", [cert.serialNumber]);
}

/**
 * onload() handler.
 */
function onLoad() {
  let typeFlag = window.arguments[0];
  let bundle = document.getElementById("pippki_bundle");
  let title;
  let confirm;
  let impact;

  switch (typeFlag) {
    case "mine_tab":
      title = bundle.getString("deleteUserCertTitle");
      confirm = bundle.getString("deleteUserCertConfirm");
      impact = bundle.getString("deleteUserCertImpact");
      break;
    case "websites_tab":
      title = bundle.getString("deleteSslCertTitle3");
      confirm = bundle.getString("deleteSslCertConfirm3");
      impact = bundle.getString("deleteSslCertImpact3");
      break;
    case "ca_tab":
      title = bundle.getString("deleteCaCertTitle2");
      confirm = bundle.getString("deleteCaCertConfirm2");
      impact = bundle.getString("deleteCaCertImpactX2");
      break;
    case "others_tab":
      title = bundle.getString("deleteEmailCertTitle");
      confirm = bundle.getString("deleteEmailCertConfirm");
      impact = bundle.getString("deleteEmailCertImpactDesc");
      break;
    case "orphan_tab":
      title = bundle.getString("deleteOrphanCertTitle");
      confirm = bundle.getString("deleteOrphanCertConfirm");
      impact = "";
      break;
    default:
      return;
  }

  document.title = title;

  setText("confirm", confirm);

  let box = document.getElementById("certlist");
  let certTreeItems = window.arguments[1];
  for (let certTreeItem of certTreeItems) {
    let listItem = document.createElement("richlistitem");
    let label = document.createElement("label");
    label.setAttribute("value", certTreeItemToString(certTreeItem));
    listItem.appendChild(label);
    box.appendChild(listItem);
  }

  setText("impact", impact);
}

/**
 * ondialogaccept() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogAccept() {
  let retVals = window.arguments[2];
  retVals.deleteConfirmed = true;
  return true;
}

/**
 * ondialogcancel() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogCancel() {
  let retVals = window.arguments[2];
  retVals.deleteConfirmed = false;
  return true;
}
