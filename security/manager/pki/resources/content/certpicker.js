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
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Kai Engert <kaie@netscape.com>
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

  var pickerTitle = dialogParams.GetString(1);
  var mainwin = document.getElementById("certPicker");
  mainwin.setAttribute("title", pickerTitle);

  var pickerInfo = dialogParams.GetString(2);
  setText("pickerInfo", pickerInfo);

  var selectElement = document.getElementById("nicknames");
  itemCount = dialogParams.GetInt(1);

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

  dialogParams.SetInt(1,0); // set cancel return value
  setDetails();
}

function setDetails()
{
  var index = parseInt(document.getElementById("nicknames").value);
  details = dialogParams.GetString(index+itemCount+3);
  document.getElementById("details").value = details;
}

function onCertSelected()
{
  setDetails();
}

function doOK()
{
  dialogParams.SetInt(1,1);
  var index = parseInt(document.getElementById("nicknames").value);
  dialogParams.SetInt(2, index);
  window.close();
}

function doCancel()
{
  dialogParams.SetInt(1,0);
  window.close();
}
