/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 */

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

  gParam.SetInt(0, 1); /* Set the default return to Cancel */

  numberOfDialogTreeElements = gParam.GetInt(1);

  for (var i=0; i < numberOfDialogTreeElements; i++)
  {
    moduleName = gParam.GetString(i);
    URL = gParam.GetString(++i);
    IconURL = gParam.GetString(++i); // Advance the enumeration, parameter is unused just now.
    certName = gParam.GetString(++i);

    addTreeItem(row++, moduleName, URL, certName);
  }
}

function onOk()
{
   // set the okay button in the param block
   if (gParam)
     gParam.SetInt(0, 0 );

  return true;
}

function onCancel()
{
    // set the cancel button in the param block
    if (gParam)
      gParam.SetInt(0, 1 );

  return true;
}

