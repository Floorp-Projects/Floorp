/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

var dialogParams;

function onLoad() {
  dialogParams = window.arguments[0].QueryInterface(Ci.nsIDialogParamBlock);
  let selectElement = document.getElementById("tokens");
  let count = dialogParams.GetInt(0);
  for (let i = 0; i < count; i++) {
    let menuItemNode = document.createElement("menuitem");
    let token = dialogParams.GetString(i);
    menuItemNode.setAttribute("value", token);
    menuItemNode.setAttribute("label", token);
    selectElement.menupopup.appendChild(menuItemNode);
    if (i == 0) {
      selectElement.selectedItem = menuItemNode;
    }
  }
  document.addEventListener("dialogaccept", doOK);
  document.addEventListener("dialogcancel", doCancel);
}

function doOK() {
  let tokenList = document.getElementById("tokens");
  // Signal that the user accepted.
  dialogParams.SetInt(0, 1);
  // Signal the name of the token the user chose.
  dialogParams.SetString(0, tokenList.value);
}

function doCancel() {
  dialogParams.SetInt(0, 0); // Signal that the user cancelled.
}
