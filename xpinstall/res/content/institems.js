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

function addTreeItem(num, aName, aUrl, aCertName)
{
  // first column is the package name
  var item = document.createElement("description");
  item.setAttribute("value", aName);
  item.setAttribute("tooltiptext", aUrl);
  item.setAttribute("class", "confirmName");

  // second column is for the cert name
  var certName = document.createElement("description");
  if (aCertName == "")
    certName.setAttribute("value", "Unsigned"); // i18n!
  else
    certName.setAttribute("value", aCertName);

  // third column is the host serving the file
  var urltext = aUrl.replace(/^([^:]*:\/*[^\/]+).*/, "$1");
  var url = document.createElement('description');
  url.setAttribute("value", aUrl);
  url.setAttribute("tooltiptext", aUrl);
  url.setAttribute("class", "confirmURL");
  url.setAttribute("crop", "end");

  // create row and add it to the grid
  var row  = document.createElement("row");
  row.appendChild(item);
  row.appendChild(certName);
  row.appendChild(url);

  document.getElementById("xpirows").appendChild(row);
}


function onLoad()
{
  var row = 0;
  var moduleName, URL, certName, numberOfDialogTreeElements;

  doSetOKCancel(onOk, onCancel);

  gParam = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);

  gParam.SetInt(0, 1 ); /* Set the default return to Cancel */

  numberOfDialogTreeElements = gParam.GetInt(1);

  for (var i=0; i < numberOfDialogTreeElements; i++)
  {
    moduleName = gParam.GetString(i);
    URL = gParam.GetString(++i);
    certName = gParam.GetString(++i);

    addTreeItem(row++, moduleName, URL, certName);
  }

  var okText = document.getElementById("xpinstallBundle").getString("OK");
  var okButton = document.getElementById("ok")
  okButton.label = okText;
  okButton.setAttribute("default",false);

  var cancelButton = document.getElementById("cancel")
  cancelButton.focus();
  cancelButton.setAttribute("default",true);
}

function onOk()
{
   // set the okay button in the param block
   if (gParam)
     gParam.SetInt(0, 0 );

   window.close();
}

function onCancel()
{
    // set the cancel button in the param block
    if (gParam)
      gParam.SetInt(0, 1 );

    window.close();
}

