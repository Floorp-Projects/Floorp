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
var itemCount = 0;

function onLoad()
{
    var cn;
    var org;
    var issuer;

    dialogParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    cn = dialogParams.GetString(0);
    org = dialogParams.GetString(1);
    issuer = dialogParams.GetString(2);

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
    itemCount = dialogParams.GetInt(0);
    for (var i=0; i < itemCount; i++) {
        var menuItemNode = document.createElement("menuitem");
        var nick = dialogParams.GetString(i+3);
        menuItemNode.setAttribute("value", i);
        menuItemNode.setAttribute("label", nick); // this is displayed
        selectElement.firstChild.appendChild(menuItemNode);
        if (i == 0) {
            selectElement.selectedItem = menuItemNode;
        }
    }

    setDetails();
}

function setDetails()
{
  var index = parseInt(document.getElementById("nicknames").value);
  var details = dialogParams.GetString(index+itemCount+3);
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
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(0,0);
  window.close();
}
