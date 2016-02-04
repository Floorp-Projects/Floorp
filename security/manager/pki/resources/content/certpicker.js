/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var dialogParams;
var itemCount = 0;

function onLoad()
{
  dialogParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);

  var selectElement = document.getElementById("nicknames");
  itemCount = dialogParams.GetInt(0);

  var selIndex = dialogParams.GetInt(1);
  if (selIndex < 0) {
    selIndex = 0;
  }

  for (let i = 0; i < itemCount; i++) {
    let menuItemNode = document.createElement("menuitem");
    let nick = dialogParams.GetString(i);
    menuItemNode.setAttribute("value", i);
    menuItemNode.setAttribute("label", nick); // This is displayed.
    selectElement.firstChild.appendChild(menuItemNode);

    if (selIndex == i) {
      selectElement.selectedItem = menuItemNode;
    }
  }

  dialogParams.SetInt(0, 0); // Set cancel return value.
  setDetails();
}

function setDetails()
{
  var selItem = document.getElementById("nicknames").value;
  if (!selItem.length)
    return;

  var index = parseInt(selItem);
  var details = dialogParams.GetString(index+itemCount);
  document.getElementById("details").value = details;
}

function onCertSelected()
{
  setDetails();
}

function doOK()
{
  dialogParams.SetInt(0,1);
  var index = parseInt(document.getElementById("nicknames").value);
  dialogParams.SetInt(1, index);
  return true;
}

function doCancel()
{
  dialogParams.SetInt(0,0);
  return true;
}
