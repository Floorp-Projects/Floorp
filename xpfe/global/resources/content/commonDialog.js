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
 *  Alec Flett <alecf@netscape.com>
 *  Ben Goodger <ben@netscape.com>
 */

var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"; 
 
function commonDialogOnLoad()
{
  doSetOKCancel(commonDialogOnOK, commonDialogOnCancel, commonDialogOnButton2, commonDialogOnButton3);
  param = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
	
  // display the main text
	var messageText = param.GetString(0);
  var messageParent = document.getElementById("info.box");
  var messageParagraphs = messageText.split("\n");
  
  for (var i = 0; i < messageParagraphs.length; i++)
    {
      var htmlNode = document.createElement("html");
      //htmlNode.setAttribute("style", "max-width: 45em;");
      var text = document.createTextNode(messageParagraphs[i]);
      htmlNode.appendChild(text);
      messageParent.appendChild(htmlNode);
    }
  
  setElementText("info.header", param.GetString(3), true); 
    
  // set the window title
  window.title = param.GetString(12);
  
  // set the icon	
  var iconElement = document.getElementById("info.icon");
  var iconURL = param.GetString(2);
  if (iconURL) iconElement.setAttribute("src", iconURL);
  
  // set the number of command buttons
  var nButtons = param.GetInt(2);
  if (nButtons == 1) hideElementById("cancel");
  switch (nButtons)
    {
      case 4:
        unHideElementByID("Button3");
        setElementText("Button3", param.GetString(11));
        // fall through
      case 3:
        unHideElementByID("Button2");
        setElementText("Button2", param.GetString(10));
        // fall through
      default:
      case 2:
        var string = param.GetString(8);
        if (string) setElementText("ok", string);
        // fall through
      case 1:
        string = param.GetString(9);
        if (string) setElementText("cancel", string);
        break;
    }

  // initialize the checkbox
  setCheckbox(param.GetString(1), param.GetInt(1));
	
  // initialize the edit fields
	var nEditFields = param.GetInt(3);
  switch (nEditFields)
    {
      case 2:
        var password2Container = document.getElementById("password2EditField");
        password2Container.removeAttribute("hidden");
        var password2Field = document.getElementById("dialog.password2");
        password2Field.value = param.GetString(7);
        
        var password2Label = param.GetString(5);
        if (password2Label) setElementText("password2.text", password2Label);
        
        var containerID, fieldID, labelID;
        if (param.GetInt(4) == 1)
          {
            // two password fields ('password' and 'retype password')
            containerID = "password1EditField";
            fieldID = "dialog.password1";
            labelID = "password1.text";
          }
        else
          {
            containerID = "loginEditField";
            fieldID = "dialog.loginname";
            labelID = "login.text";
          }
          
        unHideElementByID(containerID);
        var field = document.getElementById(fieldID);
        field.value = param.GetString(6);
            
        var label = param.GetString(4);
        if (label) setElementText(labelID, label);
        field.focusTextField();

        break;
    case 1:
      var editFieldIsPassword = param.GetInt(4);
      var containerID, fieldID;
      if (editFieldIsPassword == 1) 
        {
          containerID = "password1EditField";
          fieldID = "dialog.password1";
          setElementText("password1.text", ""); // hide the meaningless text
        }
      else
        {
          containerID = "loginEditField";
          fieldID = "dialog.loginname";
          setElementText("login.text", param.GetString(4));
        }
      
      unHideElementByID(containerID);
      var field = document.getElementById(fieldID);
      field.value = param.GetString(6);
      field.focusTextField();
      break;
  }	

	// set the pressed button to cancel to handle the case where the close box is pressed
	param.SetInt(0, 1);

}

function setCheckbox (aChkMsg, aChkValue)
{
  if (aChkMsg)
  {	
    var checkboxElement = document.getElementById("checkbox");
    unHideElementByID("checkboxContainer");
    checkboxElement.setAttribute("value", aChkMsg);
    checkboxElement.checked = aChkValue > 0 ? true : false;
  }
}

function unHideElementByID (aElementID)
{
  var element = document.getElementById(aElementID);
  element.removeAttribute("hidden");
}

function hideElementById (aElementID)
{
  var element = document.getElementById(aElementID)
  element.setAttribute("hidden", "true");
}

function onCheckboxClick(aCheckboxElement)
{
  param.SetInt(1, aCheckboxElement.checked);
}

function setElementText(aElementID, aValue, aChildNodeFlag)
{
	var element = document.getElementById(aElementID);
  if (!aChildNodeFlag && element)
    element.setAttribute("value", aValue);
  else if (aChildNodeFlag && element)
    element.appendChild(document.createTextNode(aValue));
}


function commonDialogOnOK()
{
	param.SetInt(0, 0 );
	var numEditfields = param.GetInt( 3 );
	if (numEditfields == 2) 
    {
      var editField2 = document.getElementById("dialog.password2");
      param.SetString(7, editField2.value);
    }
  var editfield1Password = param.GetInt(4);
  var editField1 = editfield1Password == 1 ? document.getElementById("dialog.password1") :
                                             document.getElementById("dialog.loginname");
  param.SetString(6, editField1.value);
  return true;
}

function commonDialogOnCancel()
{
	param.SetInt(0, 1);
	return true;
}

function commonDialogOnButton2()
{
	param.SetInt(0, 2);
	return true;
}

function commonDialogOnButton3()
{
	param.SetInt(0, 3);
	return true;
}

