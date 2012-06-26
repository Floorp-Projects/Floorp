// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const kXULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kDialog = "dialog";

/**
 * This dialog can be initialized from parameters supplied via window.arguments
 * or it can be used to display blocklist notification and blocklist blocked
 * installs via nsIDialogParamBlock as is done by nsIExtensionManager.
 *
 * When using this dialog with window.arguments it must be opened modally, the
 * caller can inspect the user action after the dialog closes by inspecting the
 * value of the |result| parameter on this object which is set to the dlgtype
 * of the button used to close the dialog.
 * 
 * window.arguments[0] is an array of strings to display in the tree. If the
 * array is empty the tree will not be displayed.
 * window.arguments[1] a JS Object with the following properties:
 * 
 * title:    A title string, to be displayed in the title bar of the dialog.
 * message1: A message string, displayed above the addon list
 * message2: A message string, displayed below the addon list
 * message3: A bolded message string, displayed below the addon list
 * moreInfoURL: An url for displaying more information
 * iconClass  : Can be one of the following values (default is alert-icon)
 *              alert-icon, error-icon, or question-icon
 *
 * If no value is supplied for message1, message2, message3, or moreInfoURL,
 * the element is not displayed.
 *
 * buttons: {
 *  accept: { label: "A Label for the Accept button",
 *            focused: true },
 *  cancel: { label: "A Label for the Cancel button" },
 *  ...
 * },
 *
 * result:  The dlgtype of button that was used to dismiss the dialog. 
 */

"use strict";

var gButtons = { };

function init() {
  var de = document.documentElement;
  var items = [];
  if (window.arguments[0] instanceof Components.interfaces.nsIDialogParamBlock) {
    // This is a warning about a blocklisted item the user is trying to install
    var args = window.arguments[0];
    var softblocked = args.GetInt(0) == 1 ? true : false;

    var extensionsBundle = document.getElementById("extensionsBundle");
    try {
      var formatter = Components.classes["@mozilla.org/toolkit/URLFormatterService;1"]
                                .getService(Components.interfaces.nsIURLFormatter);
      var url = formatter.formatURLPref("extensions.blocklist.detailsURL");
    }
    catch (e) { }

    var params = {
      moreInfoURL: url,
    };

    if (softblocked) {
      params.title = extensionsBundle.getString("softBlockedInstallTitle");
      params.message1 = extensionsBundle.getFormattedString("softBlockedInstallMsg",
                                                           [args.GetString(0)]);
      var accept = de.getButton("accept");
      accept.label = extensionsBundle.getString("softBlockedInstallAcceptLabel");
      accept.accessKey = extensionsBundle.getString("softBlockedInstallAcceptKey");
      de.getButton("cancel").focus();
      document.addEventListener("dialogaccept", allowInstall, false);
    }
    else {
      params.title = extensionsBundle.getString("blocklistedInstallTitle2");
      params.message1 = extensionsBundle.getFormattedString("blocklistedInstallMsg2",
                                                           [args.GetString(0)]);
      de.buttons = "accept";
      de.getButton("accept").focus();
    }
  }
  else {
    items = window.arguments[0];
    params = window.arguments[1];
  }

  var addons = document.getElementById("addonsChildren");
  if (items.length > 0)
    document.getElementById("addonsTree").hidden = false;

  // Fill the addons list
  for (var item of items) {
    var treeitem = document.createElementNS(kXULNS, "treeitem");
    var treerow  = document.createElementNS(kXULNS, "treerow");
    var treecell = document.createElementNS(kXULNS, "treecell");
    treecell.setAttribute("label", item);
    treerow.appendChild(treecell);
    treeitem.appendChild(treerow);
    addons.appendChild(treeitem);
  }

  // Set the messages
  var messages = ["message1", "message2", "message3"];
  for (let messageEntry of messages) {
    if (messageEntry in params) {
      var message = document.getElementById(messageEntry);
      message.hidden = false;
      message.appendChild(document.createTextNode(params[messageEntry]));
    }
  }
  
  document.getElementById("infoIcon").className =
    params["iconClass"] ? "spaced " + params["iconClass"] : "spaced alert-icon";

  if ("moreInfoURL" in params && params["moreInfoURL"]) {
    message = document.getElementById("moreInfo");
    message.value = extensionsBundle.getString("moreInfoText");
    message.setAttribute("href", params["moreInfoURL"]);
    document.getElementById("moreInfoBox").hidden = false;
  }

  // Set the window title
  if ("title" in params)
    document.title = params.title;

  // Set up the buttons
  if ("buttons" in params) {
    gButtons = params.buttons;
    var buttonString = "";
    for (var buttonType in gButtons)
      buttonString += "," + buttonType;
    de.buttons = buttonString.substr(1);
    for (buttonType in gButtons) {
      var button = de.getButton(buttonType);
      button.label = gButtons[buttonType].label;
      if (gButtons[buttonType].focused)
        button.focus();
      document.addEventListener(kDialog + buttonType, handleButtonCommand, true);
    }
  }
}

function shutdown() {
  for (var buttonType in gButtons)
    document.removeEventListener(kDialog + buttonType, handleButtonCommand, true);
}

function allowInstall() {
  var args = window.arguments[0];
  args.SetInt(1, 1);
}

/**
 * Watch for the user hitting one of the buttons to dismiss the dialog
 * and report the result back to the caller through the |result| property on
 * the arguments object.
 */
function handleButtonCommand(event) {
  window.arguments[1].result = event.type.substr(kDialog.length);
}
