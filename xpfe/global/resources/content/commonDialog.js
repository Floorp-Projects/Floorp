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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett  <alecf@netscape.com>
 *   Ben Goodger <ben@netscape.com>
 *   Blake Ross  <blakeross@telocity.com>
 *   Joe Hewitt <hewitt@netscape.com>
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

// parameters to gCommonDialogParam.Get() are defined in nsPIPromptService.idl
var gCommonDialogParam = 
  window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
  
function showControls()
{
  // This is called before onload fires, so we can't be certain that any elements
  // in the document have their bindings ready, so don't call any methods/properties
  // here on xul elements that come from xbl bindings.
  
  // show the required textboxes and set their initial values
  var nTextBoxes = gCommonDialogParam.GetInt(3);
  if (nTextBoxes == 2) {
    if (gCommonDialogParam.GetInt(4) == 1) {
      initTextbox("password1", 4, 6, false);
      initTextbox("password2", 5, 7, false);
    }
    else {
      initTextbox("login", 4, 6, false);
      initTextbox("password1", 5, 7, false);
    }
  } else if (nTextBoxes == 1) {
    if (gCommonDialogParam.GetInt(4) == 1)
      initTextbox("password1", -1, 6, true);
    else
      initTextbox("login", 4, 6, true);
  }
}

function setLabelForNode(aNode, aLabel, aIsLabelFlag)
{
  // This is for labels with possible accesskeys
  var accessKeyIndex;
  if (/^\&[^\&]/.test(aLabel)) { // access key is at the start
   accessKeyIndex = 0;
  } else {
    accessKeyIndex = aLabel.search(/[^\&]\&[^\&]/) + 1;
    if (accessKeyIndex == 0) {
      accessKeyIndex = -1; // magic value for no accesskey
    }
  }

  // If a character has an & before it, then it should become an accesskey
  if (accessKeyIndex >= 0 && accessKeyIndex < aLabel.length - 1) {
    // This will also cause the accesskey attribute to be set via xbl
    aNode.accessKey = aLabel.charAt(accessKeyIndex + 1);
    // Set the label to the string without the &
    aLabel = aLabel.substr(0, accessKeyIndex) + 
             aLabel.substr(accessKeyIndex + 1);
  }
  aLabel = aLabel.replace(/\&\&/g, "&");
  if (aIsLabelFlag) {    // Set text for <label> element
    aNode.setAttribute("value", aLabel);
  } else {    // Set text for other xul elements
    aNode.label = aLabel;
  }
}

function commonDialogOnLoad()
{
  // set the document title
  document.title = gCommonDialogParam.GetString(12);

  // set the number of command buttons
  var nButtons = gCommonDialogParam.GetInt(2);
  var dialog = document.documentElement;
  switch (nButtons) {
    case 1:
      dialog.getButton("cancel").hidden = true;
      break;
    case 4:
      dialog.getButton("extra2").hidden = false;
    case 3:
      dialog.getButton("extra1").hidden = false;
  }

  // display the main text
  var messageText = gCommonDialogParam.GetString(0);
  var messageParent = document.getElementById("info.box");
  var messageParagraphs = messageText.split("\n");

  for (var i = 0; i < messageParagraphs.length; i++) {
    var descriptionNode = document.createElement("description");
    var text = document.createTextNode(messageParagraphs[i]);
    descriptionNode.appendChild(text);
    messageParent.appendChild(descriptionNode);
  }

  setElementText("info.header", gCommonDialogParam.GetString(3), true);

  // set the icon
  var iconElement = document.getElementById("info.icon");
  var iconClass = gCommonDialogParam.GetString(2);
  if (!iconClass)
    iconClass = "message-icon";
  iconElement.setAttribute("class", iconElement.getAttribute("class") + " " + iconClass);

  switch (nButtons) {
    case 4:
      setLabelForNode(document.documentElement.getButton("extra2"), gCommonDialogParam.GetString(11));
      // fall through
    case 3:
      setLabelForNode(document.documentElement.getButton("extra1"), gCommonDialogParam.GetString(10));
      // fall through
    default:
    case 2:
      var string = gCommonDialogParam.GetString(9);
      if (string)
        setLabelForNode(document.documentElement.getButton("cancel"), string);
      // fall through
    case 1:
      string = gCommonDialogParam.GetString(8);
      if (string)
        setLabelForNode(document.documentElement.getButton("accept"), string);
      break;
  }

  // set default result to cancelled
  gCommonDialogParam.SetInt(0, 1); 

  // initialize the checkbox
  setCheckbox(gCommonDialogParam.GetString(1), gCommonDialogParam.GetInt(1));

  if (gCommonDialogParam.GetInt(3) == 0) // If no text fields
  {
    var dButton;
    var defaultButton = gCommonDialogParam.GetInt(5);
    switch (defaultButton) {
      case 3:
        dButton = document.documentElement.getButton("extra2");
        break;
      case 2:
        dButton = document.documentElement.getButton("extra1");
        break;
      case 1:
        dButton = document.documentElement.getButton("cancel");
        break;
      default:
      case 0:
        dButton = document.documentElement.getButton("accept");
        break;
    }
    // move the default attribute and focus from the accept button
    // to the one specified in the dialog params
    document.documentElement.getButton("accept").setAttribute("default",false);
    dButton.setAttribute("default", true);
    dButton.focus();
  }

  if (gCommonDialogParam.GetInt(6) != 0) // delay button enable
  {
    var delayInterval = 2000;
    try {
      var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                  .getService(Components.interfaces.nsIPrefBranch);
      delayInterval = prefs.getIntPref("security.dialog_enable_delay");
    } catch (e) {}

    document.documentElement.getButton("accept").disabled = true;
    document.documentElement.getButton("extra1").disabled = true;
    document.documentElement.getButton("extra2").disabled = true;

    setTimeout(commonDialogReenableButtons, delayInterval);
  }

  getAttention();
}

function commonDialogReenableButtons()
{
  document.documentElement.getButton("accept").disabled = false;
  document.documentElement.getButton("extra1").disabled = false;
  document.documentElement.getButton("extra2").disabled = false;
}

function initTextbox(aName, aLabelIndex, aValueIndex, aAlwaysLabel)
{
  unHideElementById(aName+"Container");

  var label = aLabelIndex < 0 ? "" : gCommonDialogParam.GetString(aLabelIndex);
  if (label || aAlwaysLabel && !label)
    setElementText(aName+"Label", label);
    
  var value = aValueIndex < 0 ? "" : gCommonDialogParam.GetString(aValueIndex);
  var textbox = document.getElementById(aName + "Textbox");
  textbox.setAttribute("value", value);
}

function setElementText(aElementID, aValue, aChildNodeFlag)
{
  var element = document.getElementById(aElementID);
  if (!aChildNodeFlag && element) {
    setLabelForNode(element, aValue, true);
  } else if (aChildNodeFlag && element) {
    element.appendChild(document.createTextNode(aValue));
  }
}

function setCheckbox(aChkMsg, aChkValue)
{
  if (aChkMsg) {
    // XXX Would love to use hidden instead of collapsed, but the checkbox
    // fails to size itself properly when I do this.
    document.getElementById("checkboxContainer").removeAttribute("collapsed");
    
    var checkboxElement = document.getElementById("checkbox");
    setLabelForNode(checkboxElement, aChkMsg);
    checkboxElement.checked = aChkValue > 0;
  }
}

function unHideElementById(aElementID)
{
  var element = document.getElementById(aElementID);
  element.hidden = false;
}

function hideElementById(aElementID)
{
  var element = document.getElementById(aElementID)
  element.hidden = true;
}

function isVisible(aElementId)
{
  return document.getElementById(aElementId).hasAttribute("hidden");
}

function onCheckboxClick(aCheckboxElement)
{
  gCommonDialogParam.SetInt(1, aCheckboxElement.checked);
}

function commonDialogOnAccept()
{
  gCommonDialogParam.SetInt(0, 0); // say that ok was pressed

  var numTextBoxes = gCommonDialogParam.GetInt(3);
  var textboxIsPassword1 = gCommonDialogParam.GetInt(4) == 1;
  
  if (numTextBoxes >= 1) {
    var editField1;
    if (textboxIsPassword1)
      editField1 = document.getElementById("password1Textbox");
    else
      editField1 = document.getElementById("loginTextbox");
    gCommonDialogParam.SetString(6, editField1.value);
  }

  if (numTextBoxes == 2) {
    var editField2;
    if (textboxIsPassword1)
      // we had two password fields
      editField2 = document.getElementById("password2Textbox");
    else
      // we only had one password field (and one login field)
      editField2 = document.getElementById("password1Textbox");
    gCommonDialogParam.SetString(7, editField2.value);
  }
}

function commonDialogOnExtra1()
{
  gCommonDialogParam.SetInt(0, 2);
  window.close();
}

function commonDialogOnExtra2()
{
  gCommonDialogParam.SetInt(0, 3);
  window.close();
}
