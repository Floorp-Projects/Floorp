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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Javier Delgadillo <javi@netscape.com>
 */


const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var dialogParams;

function onLoad()
{
    var cn;
    var org;
    var issuer;

    dialogParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    cn = dialogParams.GetString(1);
    org = dialogParams.GetString(2);
    issuer = dialogParams.GetString(3);

    var bundle = srGetStrBundle("chrome://pippki/locale/pippki.properties");
    var message1 = bundle.formatStringFromName("clientAuthMessage1", 
                                             [org],
                                             1);
    var message2 = bundle.formatStringFromName("clientAuthMessage2",
                                             [issuer],
                                             1);
    setText("hostname", cn);
    setText("organization", message1);
    setText("issuer", message2);

    var selectElement = document.getElementById("nicknames");
    for (var i=0; i < dialogParams.GetInt(1); i++) {
        var menuItemNode = document.createElement("menuitem");
        var nick = dialogParams.GetString(i+4);
        menuItemNode.setAttribute("value", nick);
        selectElement.firstChild.appendChild(menuItemNode);
        if (i == 0) {
            selectElement.selectedItem = menuItemNode;
        }
    }
}

function doOK()
{
  var nicknameList = document.getElementById("nicknames");
  var nickname = nicknameList.value;
  dialogParams.SetInt(1,1);
  dialogParams.SetString(1, nickname);
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(1,0);
  window.close();
}
