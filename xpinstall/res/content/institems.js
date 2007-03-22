/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// dialog param block
var gParam;
var gBundle;

function addTreeItem(num, aName, aUrl, aCertName)
{
  // first column is the package name
  var item = document.createElement("description");
  item.setAttribute("value", aName);
  item.setAttribute("tooltiptext", aName);
  item.setAttribute("class", "confirmName");
  item.setAttribute("crop", "center");

  // second column is for the cert name
  var certName = document.createElement("description");
  var certNameValue = aCertName ? aCertName : gBundle.getString("Unsigned");
  certName.setAttribute("value", certNameValue);
  certName.setAttribute("tooltiptext", certNameValue);
  certName.setAttribute("crop", "center");

  // third column is the host serving the file
  var urltext = aUrl.replace(/^([^:]*:\/*[^\/]+).*/, "$1");
  var url = document.createElement("description");
  url.setAttribute("value", aUrl);
  url.setAttribute("tooltiptext", aUrl);
  url.setAttribute("class", "confirmURL");
  url.setAttribute("crop", "center");

  // create row and add it to the grid
  var row = document.createElement("row");
  row.appendChild(item);
  row.appendChild(certName);
  row.appendChild(url);

  document.getElementById("xpirows").appendChild(row);
}

function onLoad()
{
  var row = 0;
  var moduleName, URL, IconURL, certName, numberOfDialogTreeElements;

  gBundle = document.getElementById("xpinstallBundle");
  gParam = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);

  gParam.SetInt(0, 1); // Set the default return to Cancel

  numberOfDialogTreeElements = gParam.GetInt(1);

  for (var i=0; i < numberOfDialogTreeElements; i++)
  {
    moduleName = gParam.GetString(i);
    URL = gParam.GetString(++i);
    IconURL = gParam.GetString(++i); // Advance the enumeration, parameter is unused just now.
    certName = gParam.GetString(++i);

    addTreeItem(row++, moduleName, URL, certName);
  }

  // Move default+focus from |accept| to |cancel| button.
  var aButton = document.documentElement.getButton("accept");
  aButton.setAttribute("default", false);
  aButton.setAttribute("label", gBundle.getString("OK"));
  aButton.setAttribute("disabled", true);

  aButton = document.documentElement.getButton("cancel");
  aButton.focus();
  aButton.setAttribute("default", true);

  // start timer to re-enable buttons
  var delayInterval = 2000;
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                .getService(Components.interfaces.nsIPrefBranch);
    delayInterval = prefs.getIntPref("security.dialog_enable_delay");
  } catch (e) {}
  setTimeout(reenableInstallButtons, delayInterval);
}

function reenableInstallButtons()
{
    document.documentElement.getButton("accept").setAttribute("disabled", false);
}

function onAccept()
{
  // set the accept button in the param block
  if (gParam)
    gParam.SetInt(0, 0);

  return true;
}

function onCancel()
{
  // set the cancel button in the param block
  if (gParam)
    gParam.SetInt(0, 1);

  return true;
}
