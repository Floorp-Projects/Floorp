/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * David Drinan.
 */


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
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(0,0);
  window.close();
}
