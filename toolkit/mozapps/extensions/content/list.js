# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is the Extension List UI.
# 
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   Ben Goodger <ben@mozilla.org>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kDialog = "dialog";

/**
 * Initialize the dialog from the parameters supplied via window.arguments
 * 
 * This dialog must be opened modally, the caller can inspect the user action
 * after the dialog closes by inspecting the value of the |result| parameter
 * on this object which is set to the dlgtype of the button used to close
 * the dialog.
 * 
 * window.arguments[0] is an array of strings to display
 * window.arguments[1] a JS Object with the following properties:
 * 
 * title:    A title string, to be displayed in the title bar of the dialog.
 * message1: A message string, displayed above the addon list
 * message2: A message string, displayed below the addon list
 * message3: A bolded message string, displayed below the addon list
 *           If no value is supplied for the message, it is not displayed.
 * buttons: {
 *  accept: { label: "A Label for the Accept button",
 *            focused: true },
 *  cancel: { label: "A Label for the Cancel button" },
 *  ...
 * },
 *
 * result:  The dlgtype of button that was used to dismiss the dialog. 
 */
function init() {
  // Fill the addons list
  var items = window.arguments[0];
  var addons = document.getElementById("addonsChildren");
  for (var i = 0; i < items.length; ++i) {
    var treeitem = document.createElementNS(kXULNS, "treeitem");
    var treerow  = document.createElementNS(kXULNS, "treerow");
    var treecell = document.createElementNS(kXULNS, "treecell");
    treecell.setAttribute("label", items[i]);
    treerow.appendChild(treecell);
    treeitem.appendChild(treerow);
    addons.appendChild(treeitem);
  }
  
  var de = document.documentElement;
  var params = window.arguments[1];

  // Set the messages
  var messages = ["message1", "message2", "message3"];
  for (var i = 0; i < messages.length; ++i) {
    if (messages[i] in params) {
      var message = document.getElementById(messages[i]);
      message.hidden = false;
      message.appendChild(document.createTextNode(params[messages[i]]));
    }
  }
  
  // Set the window title
  if ("title" in params)
    document.title = params.title;

  // Set up the buttons
  if ("buttons" in params) {
    var buttons = params.buttons;
    var buttonString = "";
    for (var buttonType in buttons)
      buttonString += "," + buttonType;
    dump("*** de.toSource = " + de.localName + "\n");
    de.buttons = buttonString.substr(1);
    for (buttonType in buttons) {
      var button = de.getButton(buttonType);
      button.label = buttons[buttonType].label;
      if (buttons[buttonType].focused)
        button.focus();
      document.addEventListener(kDialog + buttonType, handleButtonCommand, false);
    }
  }
}

/**
 * Watch for the user hitting one of the buttons to dismiss the dialog
 * and report the result back to the caller through the |result| property on
 * the arguments object.
 */
function handleButtonCommand(event) {
  window.arguments[1].result = event.type.substr(kDialog.length);
}
