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

  var hostname = dialogParams.GetString(0);
  var bundle = document.getElementById("pippki_bundle");
  var intro = bundle.getFormattedString("formSigningIntro", [hostname]);
  setText("sign.intro", intro);

  document.getElementById("sign.text").value = dialogParams.GetString(1);

  var selectElement = document.getElementById("nicknames");
  itemCount = dialogParams.GetInt(0);
  for (var index = 0; index < itemCount; ++index) {
    var menuItemNode = document.createElement("menuitem");
    var nick = dialogParams.GetString(2 + 2 * index);
    menuItemNode.setAttribute("value", index);
    menuItemNode.setAttribute("label", nick); // this is displayed
    selectElement.firstChild.appendChild(menuItemNode);
    if (index == 0) {
      selectElement.selectedItem = menuItemNode;
    }
  }
  setDetails();
  document.getElementById("pw").focus();
}

function setDetails()
{
  var index = parseInt(document.getElementById("nicknames").value);
  if (index == "NaN")
    return;
  
  var details = dialogParams.GetString(2 + (2 * index + 1));
  document.getElementById("certdetails").value = details;
}

function onCertSelected()
{
  setDetails();
}

function doOK()
{
  dialogParams.SetInt(0, 1);
  var index = parseInt(document.getElementById("nicknames").value);
  dialogParams.SetInt(1, index);
  var password = document.getElementById("pw").value;
  dialogParams.SetString(0, password);
  return true;
}

function doCancel()
{
  dialogParams.SetInt(0, 0);
  return true;
}
