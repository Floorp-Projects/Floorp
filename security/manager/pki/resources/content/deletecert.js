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

/**
 * Returns the element to represent the given nsICertTreeItem.
 * @param {nsICertTreeItem} certTreeItem
 *        The item to represent.
 * @returns {Element}
 *          A element of each cert tree item.
 */
function getLabelForCertTreeItem(certTreeItem) {
  let element = document.createXULElement("label");
  let cert = certTreeItem.cert;
  if (!cert) {
    element.setAttribute("value", certTreeItem.hostPort);
    return element;
  }

  const attributes = [
    cert.commonName,
    cert.organizationalUnit,
    cert.organization,
    cert.subjectName,
  ];
  for (let attribute of attributes) {
    if (attribute) {
      element.setAttribute("value", attribute);
      return element;
    }
  }

  document.l10n.setAttributes(element, "cert-with-serial", { serialNumber: cert.serialNumber});
  return element;
}

/**
 * onload() handler.
 */
function onLoad() {
  let typeFlag = window.arguments[0];
  let confirm = document.getElementById("confirm");
  let impact = document.getElementById("impact");
  let prefixForType;
  switch (typeFlag) {
    case "mine_tab":
      prefixForType = "delete-user-cert-";
      break;
    case "websites_tab":
      prefixForType = "delete-ssl-cert-";
      break;
    case "ca_tab":
      prefixForType = "delete-ca-cert-";
      break;
    case "others_tab":
      prefixForType = "delete-email-cert-";
      break;
    default:
      return;
  }

  document.l10n.setAttributes(document.documentElement, prefixForType + "title");
  document.l10n.setAttributes(confirm, prefixForType + "confirm");
  document.l10n.setAttributes(impact, prefixForType + "impact");

  document.addEventListener("dialogaccept", onDialogAccept);
  document.addEventListener("dialogcancel", onDialogCancel);

  let box = document.getElementById("certlist");
  let certTreeItems = window.arguments[1];
  for (let certTreeItem of certTreeItems) {
    let listItem = document.createXULElement("richlistitem");
    let label = getLabelForCertTreeItem(certTreeItem);
    listItem.appendChild(label);
    box.appendChild(listItem);
  }
}

/**
 * ondialogaccept() handler.
 */
function onDialogAccept() {
  let retVals = window.arguments[2];
  retVals.deleteConfirmed = true;
}

/**
 * ondialogcancel() handler.
 */
function onDialogCancel() {
  let retVals = window.arguments[2];
  retVals.deleteConfirmed = false;
}
