/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Don Bragg <dbragg@netscape.com
 */

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"; 
var param;

function OnLoad()
{
  param = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
	
  // display the main text
  var messageText = param.GetString(0);
  var messageParent = document.getElementById("info.box");
  
  // set the pressed button to cancel to handle the case where the close box is pressed
  param.SetInt(0, 3);

}


function OnRetry()
{
	param.SetInt(0, 0 );
    window.close();
}

function OnCreateNew()
{
	param.SetInt(0, 1);
	window.close();
}

function OnCancel()
{
    param.SetInt(0, 2);
	window.close();
}



