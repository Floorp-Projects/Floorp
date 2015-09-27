/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["CommonDialog"];

const Ci = Components.interfaces;
const Cr = Components.results;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");


this.CommonDialog = function CommonDialog(args, ui) {
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
        if (icon)
            this.iconClass.forEach(function(el,idx,arr) icon.classList.add(el));

        // set default result to cancelled
        this.args.ok = false;
        this.args.buttonNumClicked = 1;


        // Set the default button
        let b = (this.args.defaultButtonNum || 0);
        let button = this.ui["button" + b];

        if (xulDialog)
            xulDialog.defaultButton = ['accept', 'cancel', 'extra1', 'extra2'][b];
        else
            button.setAttribute("default", "true");

        // Set default focus / selection.
        this.setDefaultFocus(true);

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
        if (/ *\(\&([^&])\)(:?)$/.test(aLabel)) {
            aLabel = RegExp.leftContext + RegExp.$2;
            accessKey = RegExp.$1;
        } else if (/^([^&]*)\&(([^&]).*$)/.test(aLabel)) {
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
        this.ui[aName + "Textbox"].setAttribute("value",
                                                aValue !== null ? aValue : "");
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

    setDefaultFocus : function(isInitialLoad) {
        let b = (this.args.defaultButtonNum || 0);
        let button = this.ui["button" + b];

        if (!this.hasInputField) {
            let isOSX = ("nsILocalFileMac" in Components.interfaces);
            if (isOSX)
                this.ui.infoBody.focus();
            else
                button.focus();
        } else {
            // When the prompt is initialized, focus and select the textbox
            // contents. Afterwards, only focus the textbox.
            if (this.args.promptType == "promptPassword") {
                if (isInitialLoad)
                    this.ui.password1Textbox.select();
                else
                    this.ui.password1Textbox.focus();
            } else {
                if (isInitialLoad)
                    this.ui.loginTextbox.select();
                else
                    this.ui.loginTextbox.focus();
            }
        }
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
