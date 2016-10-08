/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

/**
 * Param block to get passed in args and to set return values to.
 * @type nsIDialogParamBlock
 */
var gParams;

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

function setWindowName()
{
  gParams = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);

  let typeFlag = gParams.GetString(0);
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
  for (let x = 0; x < gParams.objects.length; x++) {
    let listItem = document.createElement("richlistitem");
    let label = document.createElement("label");
    let certTreeItem = gParams.objects.queryElementAt(x, Ci.nsICertTreeItem);
    label.setAttribute("value", certTreeItemToString(certTreeItem));
    listItem.appendChild(label);
    box.appendChild(listItem);
  }

  setText("impact", impact);
}

function doOK()
{
  gParams.SetInt(1, 1); // means OK
  return true;
}

function doCancel()
{
  gParams.SetInt(1, 0); // means CANCEL
  return true;
}
