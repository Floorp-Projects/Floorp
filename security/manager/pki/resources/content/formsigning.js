/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  window.sizeToContent();
  doSetOKCancel(doOK, doCancel, null, null);
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
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(0, 0);
  window.close();
}
