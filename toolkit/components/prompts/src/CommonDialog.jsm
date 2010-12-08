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
 * The Original Code is CommonDialog.jsm code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var EXPORTED_SYMBOLS = ["CommonDialog"];

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");


function CommonDialog(args, ui) {
    this.args = args;
    this.ui   = ui;
}

CommonDialog.prototype = {
    args : null,
    ui   : null,

    hasInputField : true,
    numButtons    : undefined,
    iconClass     : undefined,
    soundID       : undefined,
    focusTimer    : null,

    onLoad : function(xulDialog) {
        switch (this.args.promptType) {
          case "alert":
          case "alertCheck":
            this.hasInputField = false;
            this.numButtons    = 1;
            this.iconClass     = ["alert-icon"];
            this.soundID       = Ci.nsISound.EVENT_ALERT_DIALOG_OPEN;
            break;
          case "confirmCheck":
          case "confirm":
            this.hasInputField = false;
            this.numButtons    = 2;
            this.iconClass     = ["question-icon"];
            this.soundID       = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
            break;
          case "confirmEx":
            var numButtons = 0;
            if (this.args.button0Label)
                numButtons++;
            if (this.args.button1Label)
                numButtons++;
            if (this.args.button2Label)
                numButtons++;
            if (this.args.button3Label)
                numButtons++;
            if (numButtons == 0)
                throw "A dialog with no buttons? Can not haz.";
            this.numButtons    = numButtons;
            this.hasInputField = false;
            this.iconClass     = ["question-icon"];
            this.soundID       = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
            break;
          case "prompt":
            this.numButtons = 2;
            this.iconClass  = ["question-icon"];
            this.soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
            this.initTextbox("login", this.args.value);
            // Clear the label, since this isn't really a username prompt.
            this.ui.loginLabel.setAttribute("value", "");
            break;
          case "promptUserAndPass":
            this.numButtons = 2;
            this.iconClass  = ["authentication-icon", "question-icon"];
            this.soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
            this.initTextbox("login",     this.args.user);
            this.initTextbox("password1", this.args.pass);
            break;
          case "promptPassword":
            this.numButtons = 2;
            this.iconClass  = ["authentication-icon", "question-icon"];
            this.soundID    = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
            this.initTextbox("password1", this.args.pass);
            // Clear the label, since the message presumably indicates its purpose.
            this.ui.password1Label.setAttribute("value", "");
            break;
          default:
            Cu.reportError("commonDialog opened for unknown type: " + this.args.promptType);
            throw "unknown dialog type";
        }

        // set the document title
        let title = this.args.title;
        // OS X doesn't have a title on modal dialogs, this is hidden on other platforms.
        let infoTitle = this.ui.infoTitle;
        infoTitle.appendChild(infoTitle.ownerDocument.createTextNode(title));
        if (xulDialog)
            xulDialog.ownerDocument.title = title;

        // Set button labels and visibility
        //
        // This assumes that button0 defaults to a visible "ok" button, and
        // button1 defaults to a visible "cancel" button. The other 2 buttons
        // have no default labels (and are hidden).
        switch (this.numButtons) {
          case 4:
            this.setLabelForNode(this.ui.button3, this.args.button3Label);
            this.ui.button3.hidden = false;
            // fall through
          case 3:
            this.setLabelForNode(this.ui.button2, this.args.button2Label);
            this.ui.button2.hidden = false;
            // fall through
          case 2:
            // Defaults to a visible "cancel" button
            if (this.args.button1Label)
                this.setLabelForNode(this.ui.button1, this.args.button1Label);
            break;

          case 1:
            this.ui.button1.hidden = true;
            break;
        }
        // Defaults to a visible "ok" button
        if (this.args.button0Label)
            this.setLabelForNode(this.ui.button0, this.args.button0Label);

        // display the main text
        // Bug 317334 - crop string length as a workaround.
        let croppedMessage = this.args.text.substr(0, 10000);
        let infoBody = this.ui.infoBody;
        infoBody.appendChild(infoBody.ownerDocument.createTextNode(croppedMessage));

        let label = this.args.checkLabel;
        if (label) {
            // Only show the checkbox if label has a value.
            this.ui.checkboxContainer.hidden = false;
            this.setLabelForNode(this.ui.checkbox, label);
            this.ui.checkbox.checked = this.args.checked;
        }

        // set the icon
        let icon = this.ui.infoIcon;
        this.iconClass.forEach(function(el,idx,arr) icon.classList.add(el));

        // set default result to cancelled
        this.args.ok = false;
        this.args.buttonNumClicked = 1;


        // If there are no input fields on the dialog, select the default button.
        // Otherwise, select the appropriate input field.
        // XXX shouldn't we set an unfocused default even when a textbox is focused?
        if (!this.hasInputField) {
            // Set the default button (and focus it on non-OS X systems)
            let b = 0;
            if (this.args.defaultButtonNum)
                b = this.args.defaultButtonNum;
            let button = this.ui["button" + b];
            if (xulDialog) {
                xulDialog.defaultButton = ['accept', 'cancel', 'extra1', 'extra2'][b];
                let isOSX = ("nsILocalFileMac" in Components.interfaces);
                if (!isOSX)
                    button.focus();
            } else {
                button.setAttribute("default", "true");
                button.focus();
            }

        } else {
            if (this.args.promptType == "promptPassword")
                this.ui.password1Textbox.select();
            else
                this.ui.loginTextbox.select();
        }

        if (this.args.enableDelay) {
            this.setButtonsEnabledState(false);
            // Use a longer, pref-controlled delay when the dialog is first opened.
            let delayTime = Services.prefs.getIntPref("security.dialog_enable_delay");
            this.startOnFocusDelay(delayTime);
            let self = this;
            this.ui.focusTarget.addEventListener("blur",  function(e) { self.onBlur(e);  }, false);
            this.ui.focusTarget.addEventListener("focus", function(e) { self.onFocus(e); }, false);
        }

        // Play a sound (unless we're tab-modal -- don't want those to feel like OS prompts).
        try {
            if (xulDialog && this.soundID) {
                Cc["@mozilla.org/sound;1"].
                createInstance(Ci.nsISound).
                playEventSound(this.soundID);
            }
        } catch (e) {
            Cu.reportError("Couldn't play common dialog event sound: " + e);
        }

        let topic = "common-dialog-loaded";
        if (!xulDialog)
            topic = "tabmodal-dialog-loaded";
        Services.obs.notifyObservers(this.ui.prompt, topic, null);
    },

    setLabelForNode: function(aNode, aLabel) {
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
        aNode.label = aLabel;

        // XXXjag bug 325251
        // Need to set this after aNode.setAttribute("value", aLabel);
        if (accessKey)
            aNode.accessKey = accessKey;
    },


    initTextbox : function (aName, aValue) {
        this.ui[aName + "Container"].hidden = false;
        this.ui[aName + "Textbox"].setAttribute("value", aValue);
    },

    setButtonsEnabledState : function(enabled) {
        this.ui.button0.disabled = !enabled;
        // button1 (cancel) remains enabled.
        this.ui.button2.disabled = !enabled;
        this.ui.button3.disabled = !enabled;
    },

    onBlur : function (aEvent) {
        if (aEvent.target != this.ui.focusTarget)
            return;
        this.setButtonsEnabledState(false);

        // If we blur while waiting to enable the buttons, just cancel the
        // timer to ensure the delay doesn't fire while not focused.
        if (this.focusTimer) {
            this.focusTimer.cancel();
            this.focusTimer = null;
        }
    },

    onFocus : function (aEvent) {
        if (aEvent.target != this.ui.focusTarget)
            return;
        this.startOnFocusDelay();
    },

    startOnFocusDelay : function(delayTime) {
        // Shouldn't already have a timer, but just in case...
        if (this.focusTimer)
            return;
        // If no delay specified, use 250ms. (This is the normal case for when
        // after the dialog has been opened and focus shifts.)
        if (!delayTime)
            delayTime = 250;
        let self = this;
        this.focusTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this.focusTimer.initWithCallback(function() { self.onFocusTimeout(); },
                                         delayTime, Ci.nsITimer.TYPE_ONE_SHOT);
    },

    onFocusTimeout : function() {
        this.focusTimer = null;
        this.setButtonsEnabledState(true);
    },

    onCheckbox : function() {
        this.args.checked = this.ui.checkbox.checked;
    },

    onButton0 : function() {
        this.args.promptActive = false;
        this.args.ok = true;
        this.args.buttonNumClicked = 0;

        let username = this.ui.loginTextbox.value;
        let password = this.ui.password1Textbox.value;

        // Return textfield values
        switch (this.args.promptType) {
          case "prompt":
            this.args.value = username;
            break;
          case "promptUserAndPass":
            this.args.user = username;
            this.args.pass = password;
            break;
          case "promptPassword":
            this.args.pass = password;
            break;
        }
    },

    onButton1 : function() {
        this.args.promptActive = false;
        this.args.buttonNumClicked = 1;
    },

    onButton2 : function() {
        this.args.promptActive = false;
        this.args.buttonNumClicked = 2;
    },

    onButton3 : function() {
        this.args.promptActive = false;
        this.args.buttonNumClicked = 3;
    },

    abortPrompt : function() {
        this.args.promptActive = false;
        this.args.promptAborted = true;
    },

};
