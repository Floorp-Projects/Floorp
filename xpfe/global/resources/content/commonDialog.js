/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alec Flett  <alecf@netscape.com>
 *  Ben Goodger <ben@netscape.com>
 *  Blake Ross  <blakeross@telocity.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var gCommonDialogParam;

function commonDialogOnLoad()
{
  doSetOKCancel(commonDialogOnOK, commonDialogOnCancel, commonDialogOnButton2, commonDialogOnButton3);
  gCommonDialogParam = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);

  // display the main text
  var messageText = gCommonDialogParam.GetString(0);
  var messageParent = document.getElementById("info.box");
  var messageParagraphs = messageText.split("\n");
  var i;

  for (i = 0; i < messageParagraphs.length; i++) {
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
  var iconClass = gCommonDialogParam.GetString(2);
  if (!iconClass)
    iconClass = "message-icon";
  iconElement.setAttribute("class", iconElement.getAttribute("class") + " " + iconClass);

  // set the number of command buttons
  var nButtons = gCommonDialogParam.GetInt(2);
  if (nButtons == 1) hideElementById("cancel");
  switch (nButtons) {
    case 4:
      unHideElementByID("Button3");
      document.getElementById("Button3").label = gCommonDialogParam.GetString(11);
      // fall through
    case 3:
      unHideElementByID("Button2");
      document.getElementById("Button2").label = gCommonDialogParam.GetString(10);
      // fall through
    default:
    case 2:
      var string = gCommonDialogParam.GetString(8);
      if (string)
        document.getElementById("ok").label = string;
      // fall through
    case 1:
      string = gCommonDialogParam.GetString(9);
      if (string)
        document.getElementById("cancel").label = string;
      break;
  }

  // initialize the checkbox
  setCheckbox(gCommonDialogParam.GetString(1), gCommonDialogParam.GetInt(1));

  // initialize the edit fields
  var nEditFields = gCommonDialogParam.GetInt(3);

  switch (nEditFields) {
    case 2:
      var containerID, fieldID, labelID;

      if (gCommonDialogParam.GetInt(4) == 1) {
        // two password fields ('password' and 'retype password')
        var password2Container = document.getElementById("password2EditField");
        password2Container.removeAttribute("collapsed");
        var password2Field = document.getElementById("dialog.password2");
        password2Field.value = gCommonDialogParam.GetString(7);

        var password2Label = gCommonDialogParam.GetString(5);
        if (password2Label)
          setElementText("password2.text", password2Label);

        containerID = "password1EditField";
        fieldID = "dialog.password1";
        labelID = "password1.text";
      }
      else {
        // one login field and one password field
        var passwordContainer = document.getElementById("password1EditField");
        passwordContainer.removeAttribute("collapsed");
        var passwordField = document.getElementById("dialog.password1");
        passwordField.value = gCommonDialogParam.GetString(7);

        var passwordLabel = gCommonDialogParam.GetString(5);
        if (passwordLabel)
          setElementText("password1.text", passwordLabel);

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
  // preferred order is textbox1, textbox2, textbox 3, OK, cancel, button2, button3
  var visibilityList = ["loginEditField", "password1EditField", "password2EditField", "ok",
                         "cancel", "Button2", "Button3"]
  var focusList      = ["dialog.loginname", "dialog.password1", "dialog.password2", "ok",
                         "cancel", "Button2", "Button3"]

  for (i = 0; i < visibilityList.length; i++) {
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
    checkboxElement.label = aChkMsg;
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
  var editfield1Password = gCommonDialogParam.GetInt(4);
  if (numEditfields == 2) {
    var editField2;
    if (editfield1Password == 1)
      {
        // we had two password fields
        editField2 = document.getElementById("dialog.password2");
      }
    else
      {
        // we only had one password field (and one login field)
        editField2 = document.getElementById("dialog.password1");
      }
    gCommonDialogParam.SetString(7, editField2.value);
  }
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
