/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *  Alec Flett  <alecf@netscape.com>
 *  Ben Goodger <ben@netscape.com>
 *  Blake Ross  <blakeross@telocity.com>
 */

var gCommonDialogParam;

function commonDialogOnLoad()
{
  doSetOKCancel(commonDialogOnOK, commonDialogOnCancel, commonDialogOnButton2, commonDialogOnButton3);
  gCommonDialogParam = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
	
  // display the main text
  var messageText = gCommonDialogParam.GetString(0);
  var messageParent = document.getElementById("info.box");
  var messageParagraphs = messageText.split("\n");
  
  for (var i = 0; i < messageParagraphs.length; i++) {
    var htmlNode = document.createElement("html");
    //htmlNode.setAttribute("style", "max-width: 45em;");
    var text = document.createTextNode(messageParagraphs[i]);
    htmlNode.appendChild(text);
    messageParent.appendChild(htmlNode);
  }
  
  setElementText("info.header", gCommonDialogParam.GetString(3), true); 
    
  // set the window title
  window.title = gCommonDialogParam.GetString(12);
  
  // set the icon	
  var iconElement = document.getElementById("info.icon");
  var iconURL = gCommonDialogParam.GetString(2);
  if (iconURL)
    iconElement.setAttribute("src", iconURL);
  
  // set the number of command buttons
  var nButtons = gCommonDialogParam.GetInt(2);
  if (nButtons == 1) hideElementById("cancel");
  switch (nButtons) {
    case 4:
      unHideElementByID("Button3");
      setElementText("Button3", gCommonDialogParam.GetString(11));
      // fall through
    case 3:
      unHideElementByID("Button2");
      setElementText("Button2", gCommonDialogParam.GetString(10));
      // fall through
    default:
    case 2:
      var string = gCommonDialogParam.GetString(8);
      if (string)
        setElementText("ok", string);
      // fall through
    case 1:
      string = gCommonDialogParam.GetString(9);
      if (string)
        setElementText("cancel", string);
      break;
  }

  // initialize the checkbox
  setCheckbox(gCommonDialogParam.GetString(1), gCommonDialogParam.GetInt(1));
	
  // initialize the edit fields
  var nEditFields = gCommonDialogParam.GetInt(3);
  
  switch (nEditFields) {
    case 2:
      var password2Container = document.getElementById("password2EditField");
      password2Container.removeAttribute("collapsed");
      var password2Field = document.getElementById("dialog.password2");
      password2Field.value = gCommonDialogParam.GetString(7);
      
      var password2Label = gCommonDialogParam.GetString(5);
      if (password2Label)
        setElementText("password2.text", password2Label);
        
      var containerID, fieldID, labelID;
      if (gCommonDialogParam.GetInt(4) == 1) {
        // two password fields ('password' and 'retype password')
        containerID = "password1EditField";
        fieldID = "dialog.password1";
        labelID = "password1.text";
      }
      else {
        containerID = "loginEditField";
        fieldID = "dialog.loginname";
        labelID = "login.text";
      }
          
      unHideElementByID(containerID);
      var field = document.getElementById(fieldID);
      field.value = gCommonDialogParam.GetString(6);
          
      var label = gCommonDialogParam.GetString(4);
      if (label)
        setElementText(labelID, label);
      field.focus();
      break;
    case 1:
      var editFieldIsPassword = gCommonDialogParam.GetInt(4);
      if (editFieldIsPassword == 1) {
        containerID = "password1EditField";
        fieldID = "dialog.password1";
        setElementText("password1.text", ""); // hide the meaningless text
      }
      else {
        containerID = "loginEditField";
        fieldID = "dialog.loginname";
        setElementText("login.text", gCommonDialogParam.GetString(4));
      }
      
      unHideElementByID(containerID);
      field = document.getElementById(fieldID);
      field.value = gCommonDialogParam.GetString(6);
      field.focus();
      break;
  }	

  // set the pressed button to cancel to handle the case where the close box is pressed
  gCommonDialogParam.SetInt(0, 1);

  // set default focus
  // preferred order is textfield1, textfield2, textfield 3, OK, cancel, button2, button3
  var visibilityList = ["loginEditField", "password1EditField", "password2EditField", "ok",
                         "cancel", "Button2", "Button3"]
  var focusList      = ["dialog.loginname", "dialog.password1", "dialog.password2", "ok",
                         "cancel", "Button2", "Button3"]

  for (var i = 0; i < visibilityList.length; i++) {
    if (isVisible(visibilityList[i])) {
      document.getElementById(focusList[i]).focus();
      break;
    }
  }
}

function setCheckbox(aChkMsg, aChkValue)
{
  if (aChkMsg) {	
    var checkboxElement = document.getElementById("checkbox");
    unHideElementByID("checkboxContainer");
    checkboxElement.setAttribute("value", aChkMsg);
    checkboxElement.checked = aChkValue > 0 ? true : false;
  }
}

function unHideElementByID(aElementID)
{
  var element = document.getElementById(aElementID);
  element.removeAttribute("collapsed");
}

function hideElementById(aElementID)
{
  var element = document.getElementById(aElementID)
  element.setAttribute("collapsed", "true");
}

function isVisible(aElementId)
{
  var element = document.getElementById(aElementId);
  if (element) {
    // XXX check for display:none when getComputedStyle() works properly
    return !(element.getAttribute("collapsed") || element.getAttribute("hidden"));
  }
  return false;   // doesn't exist, so not visible
}

function onCheckboxClick(aCheckboxElement)
{
  gCommonDialogParam.SetInt(1, aCheckboxElement.checked);
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
  gCommonDialogParam.SetInt(0, 0);
  var numEditfields = gCommonDialogParam.GetInt(3);
  if (numEditfields == 2) {
    var editField2 = document.getElementById("dialog.password2");
    gCommonDialogParam.SetString(7, editField2.value);
  }
  var editfield1Password = gCommonDialogParam.GetInt(4);
  var editField1 = editfield1Password == 1 ? document.getElementById("dialog.password1") :
                                             document.getElementById("dialog.loginname");
  gCommonDialogParam.SetString(6, editField1.value);
  return true;
}

function commonDialogOnCancel()
{
  gCommonDialogParam.SetInt(0, 1);
  return true;
}

function commonDialogOnButton2()
{
  gCommonDialogParam.SetInt(0, 2);
  return true;
}

function commonDialogOnButton3()
{
  gCommonDialogParam.SetInt(0, 3);
  return true;
}
