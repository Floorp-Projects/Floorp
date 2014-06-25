/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var dialogParams;

function onLoad()
{
    dialogParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    var selectElement = document.getElementById("tokens");
    var aCount = dialogParams.GetInt(0);
    for (var i=0; i < aCount; i++) {
        var menuItemNode = document.createElement("menuitem");
        var token = dialogParams.GetString(i);
        menuItemNode.setAttribute("value", token);
        menuItemNode.setAttribute("label", token);
        selectElement.firstChild.appendChild(menuItemNode);
        if (i == 0) {
            selectElement.selectedItem = menuItemNode;
        }
    }
}

function doOK()
{
  var tokenList = document.getElementById("tokens");
  var token = tokenList.value;
  dialogParams.SetInt(0,1);
  dialogParams.SetString(0, token);
  return true;
}

function doCancel()
{
  dialogParams.SetInt(0,0);
  return true;
}
