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

var toolkit;
var browser;
var dialog;

// dialog param block
var param;

function addTreeItem(num, modName, url)
{
  dump("Adding element " + num + " : " + name + "\n");
  var body = document.getElementById("theTreeBody");

  var newitem = document.createElement('treeitem');
  var newrow = document.createElement('treerow');
  
  newrow.setAttribute("rowNum", num);
  newrow.setAttribute("rowName", modName);

  var elem = document.createElement('treecell');
  elem.setAttribute("value", modName);
  newrow.appendChild(elem);

  elem = document.createElement('treecell');
  elem.setAttribute("value", url);
  newrow.appendChild(elem);

  newitem.appendChild(newrow);
  body.appendChild(newitem);
}


function onLoad() 
{
    var i = 0;
	var row = 0;
	var moduleName;
	var URL;
    var numberOfDialogTreeElements;
              doSetOKCancel(onOk, cancel);
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
	{
		dump( " error getting param block interface\n" );
		return;
	}
    param.SetInt(0, 1 ); /* Set the default return to Cancel */

	numberOfDialogTreeElements = param.GetInt(1);

    for (i=0; i < numberOfDialogTreeElements; i++)
	{
        moduleName = param.GetString(i);
		URL = param.GetString(++i);
        addTreeItem(row++, moduleName, URL);
	}
}

function onOk() 
{
   // set the okay button in the param block
   if (param)
	param.SetInt(0, 0 );

   window.close();
}

function cancel() 
{
	// set the cancel button in the param block
	if (param)
	  param.SetInt(0, 1 );
    
	window.close();
}

