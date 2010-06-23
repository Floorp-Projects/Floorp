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
 *   Justin Dolske <dolske@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

let gArgs = window.arguments[0].QueryInterface(Ci.nsIWritablePropertyBag2)
                               .QueryInterface(Ci.nsIWritablePropertyBag);

let promptType, numButtons, iconClass, soundID, hasInputField = true;


function earlyInit() {
    // This is called before onload fires, so we can't be certain that any elements
    // in the document have their bindings ready, so don't call any methods/properties
    // here on xul elements that come from xbl bindings.

    promptType = gArgs.getProperty("promptType");

    switch (promptType) {
      case "alert":
      case "alertCheck":
        hasInputField = false;
        numButtons    = 1;
        iconClass     = "alert-icon";
        soundID       = Ci.nsISound.EVENT_ALERT_DIALOG_OPEN;
        break;
      case "confirmCheck":
      case "confirm":
        hasInputField = false;
        numButtons    = 2;
        iconClass     = "question-icon";
        soundID       = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
        break;
      case "confirmEx":
        numButtons = 0;
        if (gArgs.hasKey("button0Label"))
            numButtons++;
        if (gArgs.hasKey("button1Label"))
            numButtons++;
        if (gArgs.hasKey("button2Label"))
            numButtons++;
        if (gArgs.hasKey("button3Label"))
            numButtons++;
        if (numButtons == 0)
            throw "A dialog with no buttons? Can not haz.";
        hasInputField = false;
        iconClass     = "question-icon";
        soundID       = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
        break;
      case "prompt":
        numButtons = 2;
        iconClass  = "question-icon";
        soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        initTextbox("login", gArgs.getProperty("value"));
        // Clear the label, since this isn't really a username prompt.
        document.getElementById("loginLabel").setAttribute("value", "");
        break;
      case "promptUserAndPass":
        numButtons = 2;
        iconClass  = "authentication-icon question-icon";
        soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        initTextbox("login",     gArgs.getProperty("user"));
        initTextbox("password1", gArgs.getProperty("pass"));
        break;
      case "promptPassword":
        numButtons = 2;
        iconClass  = "authentication-icon question-icon";
        soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        initTextbox("password1", gArgs.getProperty("pass"));
        // Clear the label, since the message presumably indicates its purpose.
        document.getElementById("password1Label").setAttribute("value", "");
        break;
      default:
        Cu.reportError("commonDialog opened for unknown type: " + promptType);
        window.close();
    }
}

function initTextbox(aName, aValue) {
    document.getElementById(aName + "Container").hidden = false;
    document.getElementById(aName + "Textbox").setAttribute("value", aValue);
}

function setLabelForNode(aNode, aLabel, aIsLabelFlag) {
    // This is for labels which may contain embedded access keys.
    // If we end in (&X) where X represents the access key, optionally preceded
    // by spaces and/or followed by the ':' character, store the access key and
    // remove the access key placeholder + leading spaces from the label.
    // Otherwise a character preceded by one but not two &s is the access key.
    // Store it and remove the &.

    // Note that if you change the following code, see the comment of
    // nsTextBoxFrame::UpdateAccessTitle.
    var accessKey = null;
    if (/ *\(\&([^&])\)(:)?$/.test(aLabel)) {
        aLabel = RegExp.leftContext + RegExp.$2;
        accessKey = RegExp.$1;
    } else if (/^(.*[^&])?\&(([^&]).*$)/.test(aLabel)) {
        aLabel = RegExp.$1 + RegExp.$2;
        accessKey = RegExp.$3;
    }

    // && is the magic sequence to embed an & in your label.
    aLabel = aLabel.replace(/\&\&/g, "&");
    if (aIsLabelFlag) {    // Set text for <label> element
        aNode.setAttribute("value", aLabel);
    } else {    // Set text for other xul elements
        aNode.label = aLabel;
    }

    // XXXjag bug 325251
    // Need to set this after aNode.setAttribute("value", aLabel);
    if (accessKey)
        aNode.accessKey = accessKey;
}

function softkbObserver(subject, topic, data) {
    let rect = JSON.parse(data);
    if (rect) {
        let height = rect.bottom - rect.top;
        let width  = rect.right - rect.left;
        let top    = (rect.top + (height - window.innerHeight) / 2);
        let left   = (rect.left + (width - window.innerWidth) / 2);
        window.moveTo(left, top);
    }
}

function commonDialogOnLoad() {
    // limit the dialog to the screen width
    document.getElementById("filler").maxWidth = screen.availWidth;

    // set the document title
    let title = gArgs.getProperty("title");
#ifdef XP_MACOSX
    document.getElementById("info.title").appendChild(document.createTextNode(title));
#else
    document.title = title;
#endif

    Services.obs.addObserver(softkbObserver, "softkb-change", false);

    // Set button visibility
    let dialog = document.documentElement;
    switch (numButtons) {
      case 1:
        dialog.getButton("cancel").hidden = true;
        break;
      case 4:
        dialog.getButton("extra2").hidden = false;
      case 3:
        dialog.getButton("extra1").hidden = false;
    }

    // Set button labels
    switch (numButtons) {
      case 4:
        setLabelForNode(document.documentElement.getButton("extra2"), gArgs.getProperty("button3Label"));
        // fall through
      case 3:
        setLabelForNode(document.documentElement.getButton("extra1"), gArgs.getProperty("button2Label"));
        // fall through
      default:
      case 2:
        if (gArgs.hasKey("button1Label"))
            setLabelForNode(document.documentElement.getButton("cancel"), gArgs.getProperty("button1Label"));
        // fall through
      case 1:
        if (gArgs.hasKey("button0Label"))
            setLabelForNode(document.documentElement.getButton("accept"), gArgs.getProperty("button0Label"));
        break;
    }

    // display the main text
    // Bug 317334 - crop string length as a workaround.
    let croppedMessage = gArgs.getProperty("text").substr(0, 10000);
    document.getElementById("info.body").appendChild(document.createTextNode(croppedMessage));

    if (gArgs.hasKey("checkLabel")) {
        let label = gArgs.getProperty("checkLabel")
        // Only show the checkbox if label has a value.
        if (label) {
            document.getElementById("checkboxContainer").hidden = false;
            let checkboxElement = document.getElementById("checkbox");
            setLabelForNode(checkboxElement, label);
            checkboxElement.checked = gArgs.getProperty("checked");
        }
    }

    // set the icon
    document.getElementById("info.icon").className += " " + iconClass;

    // set default result to cancelled
    gArgs.setProperty("ok", false);
    gArgs.setProperty("buttonNumClicked", 1);


    // If there are no input fields on the dialog, select the default button.
    // Otherwise, select the appropriate input field.
    if (!hasInputField) {
        let dlgButtons = ['accept', 'cancel', 'extra1', 'extra2'];

        // Set the default button and focus it on non-OS X systems
        let b = 0;
        if (gArgs.hasKey("defaultButtonNum"))
            b = gArgs.getProperty("defaultButtonNum");
        let dButton = dlgButtons[b];
        // XXX shouldn't we set the default even when a textbox is focused?
        document.documentElement.defaultButton = dButton;
#ifndef XP_MACOSX
        document.documentElement.getButton(dButton).focus();
#endif
    } else {
        if (promptType == "promptPassword")
            document.getElementById("password1Textbox").select();
        else
            document.getElementById("loginTextbox").select();
    }

    if (gArgs.hasKey("enableDelay") && gArgs.getProperty("enableDelay")) {
        let delayInterval = Services.prefs.getIntPref("security.dialog_enable_delay");

        document.documentElement.getButton("accept").disabled = true;
        document.documentElement.getButton("extra1").disabled = true;
        document.documentElement.getButton("extra2").disabled = true;

        setTimeout(commonDialogReenableButtons, delayInterval);

        addEventListener("blur", commonDialogBlur, false);
        addEventListener("focus", commonDialogFocus, false);
    }

    window.getAttention();

    // play sound
    try {
        if (soundID) {
            Cc["@mozilla.org/sound;1"].
            createInstance(Ci.nsISound).
            playEventSound(soundID);
        }
    } catch (e) { }

    Services.obs.notifyObservers(window, "common-dialog-loaded", null);
}

function commonDialogOnUnload() {
    Services.obs.removeObserver(softkbObserver, "softkb-change");
}


var gDelayExpired = false;
var gBlurred = false;

function commonDialogBlur(aEvent) {
    if (aEvent.target != document)
        return;
    gBlurred = true;
    document.documentElement.getButton("accept").disabled = true;
    document.documentElement.getButton("extra1").disabled = true;
    document.documentElement.getButton("extra2").disabled = true;
}

function commonDialogFocus(aEvent) {
    if (aEvent.target != document)
        return;
    gBlurred = false;
    // When refocusing the window, don't enable the buttons unless the countdown
    // delay has expired.
    if (gDelayExpired) {
        let script;
        script  = "document.documentElement.getButton('accept').disabled = false; ";
        script += "document.documentElement.getButton('extra1').disabled = false; ";
        script += "document.documentElement.getButton('extra2').disabled = false;";
        setTimeout(script, 250);
    }
}

function commonDialogReenableButtons() {
    // Don't automatically enable the buttons if we're not in the foreground
    if (!gBlurred) {
        document.documentElement.getButton("accept").disabled = false;
        document.documentElement.getButton("extra1").disabled = false;
        document.documentElement.getButton("extra2").disabled = false;
    }
    gDelayExpired = true;
}

function onCheckboxClick(aCheckboxElement) {
    gArgs.setProperty("checked", aCheckboxElement.checked);
}

function commonDialogOnAccept() {
    gArgs.setProperty("ok", true);
    gArgs.setProperty("buttonNumClicked", 0);

    let username = document.getElementById("loginTextbox").value;
    let password = document.getElementById("password1Textbox").value;

    // Return textfield values
    switch (promptType) {
      case "prompt":
        gArgs.setProperty("value", username);
        break;
      case "promptUserAndPass":
        gArgs.setProperty("user", username);
        gArgs.setProperty("pass", password);
        break;
      case "promptPassword":
        gArgs.setProperty("pass", password);
        break;
    }
}

function commonDialogOnExtra1() {
    // .setProperty("ok", true)?
    gArgs.setProperty("buttonNumClicked", 2);
    window.close();
}

function commonDialogOnExtra2() {
    // .setProperty("ok", true)?
    gArgs.setProperty("buttonNumClicked", 3);
    window.close();
}
