/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["CommonDialog"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "EnableDelayHelper",
  "resource://gre/modules/SharedPromptUtils.jsm"
);

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

function CommonDialog(args, ui) {
  this.args = args;
  this.ui = ui;
  this.initialFocusPromise = new Promise(resolve => {
    this.initialFocusResolver = resolve;
  });
}

CommonDialog.prototype = {
  args: null,
  ui: null,

  hasInputField: true,
  numButtons: undefined,
  iconClass: undefined,
  soundID: undefined,
  focusTimer: null,
  initialFocusPromise: null,
  initialFocusResolver: null,

  /**
   * @param [commonDialogEl] - Dialog element from commonDialog.xhtml,
   * null for TabModalPrompts.
   */
  async onLoad(commonDialogEl = null) {
    let isEmbedded = !!commonDialogEl?.ownerGlobal.docShell.chromeEventHandler;

    switch (this.args.promptType) {
      case "alert":
      case "alertCheck":
        this.hasInputField = false;
        this.numButtons = 1;
        this.iconClass = ["alert-icon"];
        this.soundID = Ci.nsISound.EVENT_ALERT_DIALOG_OPEN;
        break;
      case "confirmCheck":
      case "confirm":
        this.hasInputField = false;
        this.numButtons = 2;
        this.iconClass = ["question-icon"];
        this.soundID = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
        break;
      case "confirmEx":
        var numButtons = 0;
        if (this.args.button0Label) {
          numButtons++;
        }
        if (this.args.button1Label) {
          numButtons++;
        }
        if (this.args.button2Label) {
          numButtons++;
        }
        if (this.args.button3Label) {
          numButtons++;
        }
        if (numButtons == 0) {
          throw new Error("A dialog with no buttons? Can not haz.");
        }
        this.numButtons = numButtons;
        this.hasInputField = false;
        this.iconClass = ["question-icon"];
        this.soundID = Ci.nsISound.EVENT_CONFIRM_DIALOG_OPEN;
        break;
      case "prompt":
        this.numButtons = 2;
        this.iconClass = ["question-icon"];
        this.soundID = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        this.initTextbox("login", this.args.value);
        // Clear the label, since this isn't really a username prompt.
        this.ui.loginLabel.setAttribute("value", "");
        // Ensure the labeling for the prompt is correct.
        this.ui.loginTextbox.setAttribute("aria-labelledby", "infoBody");
        break;
      case "promptUserAndPass":
        this.numButtons = 2;
        this.iconClass = ["authentication-icon", "question-icon"];
        this.soundID = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        this.initTextbox("login", this.args.user);
        this.initTextbox("password1", this.args.pass);
        break;
      case "promptPassword":
        this.numButtons = 2;
        this.iconClass = ["authentication-icon", "question-icon"];
        this.soundID = Ci.nsISound.EVENT_PROMPT_DIALOG_OPEN;
        this.initTextbox("password1", this.args.pass);
        // Clear the label, since the message presumably indicates its purpose.
        this.ui.password1Label.setAttribute("value", "");
        break;
      default:
        Cu.reportError(
          "commonDialog opened for unknown type: " + this.args.promptType
        );
        throw new Error("unknown dialog type");
    }

    if (commonDialogEl) {
      commonDialogEl.setAttribute(
        "windowtype",
        "prompt:" + this.args.promptType
      );
    }

    // set the document title
    let title = this.args.title;
    let infoTitle = this.ui.infoTitle;
    infoTitle.appendChild(infoTitle.ownerDocument.createTextNode(title));

    // Specific check to prevent showing the title on the old content prompts for macOS.
    // This should be removed when the old content prompts are removed.
    let contentSubDialogPromptEnabled = Services.prefs.getBoolPref(
      "prompts.contentPromptSubDialog"
    );
    let isOldContentPrompt =
      !contentSubDialogPromptEnabled &&
      this.args.modalType == Ci.nsIPrompt.MODAL_TYPE_CONTENT;

    // After making these preventative checks, we can determine to show it if we're on
    // macOS (where there is no titlebar) or if the prompt is a common dialog document
    // and has been embedded (has a chromeEventHandler).
    infoTitle.hidden =
      isOldContentPrompt || !(AppConstants.platform === "macosx" || isEmbedded);

    if (commonDialogEl) {
      commonDialogEl.ownerDocument.title = title;
    }

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
        if (this.args.button1Label) {
          this.setLabelForNode(this.ui.button1, this.args.button1Label);
        }
        break;

      case 1:
        this.ui.button1.hidden = true;
        break;
    }
    // Defaults to a visible "ok" button
    if (this.args.button0Label) {
      this.setLabelForNode(this.ui.button0, this.args.button0Label);
    }

    // display the main text
    let croppedMessage = "";
    if (this.args.text) {
      // Bug 317334 - crop string length as a workaround.
      croppedMessage = this.args.text.substr(0, 10000);
      // TabModalPrompts don't have an infoRow to hide / not hide here, so
      // guard on that here so long as they are in use.
      if (this.ui.infoRow) {
        this.ui.infoRow.hidden = false;
      }
    }
    let infoBody = this.ui.infoBody;
    infoBody.appendChild(infoBody.ownerDocument.createTextNode(croppedMessage));

    let label = this.args.checkLabel;
    if (label) {
      // Only show the checkbox if label has a value.
      this.ui.checkboxContainer.hidden = false;
      this.ui.checkboxContainer.clientTop; // style flush to assure binding is attached
      this.setLabelForNode(this.ui.checkbox, label);
      this.ui.checkbox.checked = this.args.checked;
    }

    // set the icon
    let icon = this.ui.infoIcon;
    if (icon) {
      this.iconClass.forEach((el, idx, arr) => icon.classList.add(el));
    }

    // set default result to cancelled
    this.args.ok = false;
    this.args.buttonNumClicked = 1;

    // Set the default button
    let b = this.args.defaultButtonNum || 0;
    let button = this.ui["button" + b];

    if (commonDialogEl) {
      commonDialogEl.defaultButton = ["accept", "cancel", "extra1", "extra2"][
        b
      ];
    } else {
      button.setAttribute("default", "true");
    }

    if (!isEmbedded && !this.ui.promptContainer?.hidden) {
      // Set default focus and select textbox contents if applicable. If we're
      // embedded SubDialogManager will call setDefaultFocus for us.
      this.setDefaultFocus(true);
    }

    if (this.args.enableDelay) {
      this.delayHelper = new EnableDelayHelper({
        disableDialog: () => this.setButtonsEnabledState(false),
        enableDialog: () => this.setButtonsEnabledState(true),
        focusTarget: this.ui.focusTarget,
      });
    }

    // Play a sound (unless we're showing a content prompt -- don't want those
    //               to feel like OS prompts).
    try {
      if (commonDialogEl && this.soundID && !this.args.openedWithTabDialog) {
        Cc["@mozilla.org/sound;1"]
          .createInstance(Ci.nsISound)
          .playEventSound(this.soundID);
      }
    } catch (e) {
      Cu.reportError("Couldn't play common dialog event sound: " + e);
    }

    if (commonDialogEl) {
      if (isEmbedded) {
        // If we delayed default focus above, wait for it to be ready before
        // sending the notification.
        await this.initialFocusPromise;
      }
      Services.obs.notifyObservers(this.ui.prompt, "common-dialog-loaded");
    } else {
      // ui.promptContainer is the <tabmodalprompt> element.
      Services.obs.notifyObservers(
        this.ui.promptContainer,
        "tabmodal-dialog-loaded"
      );
    }
  },

  setLabelForNode(aNode, aLabel) {
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
    if (accessKey) {
      aNode.accessKey = accessKey;
    }
  },

  initTextbox(aName, aValue) {
    this.ui[aName + "Container"].hidden = false;
    this.ui[aName + "Textbox"].setAttribute(
      "value",
      aValue !== null ? aValue : ""
    );
  },

  setButtonsEnabledState(enabled) {
    this.ui.button0.disabled = !enabled;
    // button1 (cancel) remains enabled.
    this.ui.button2.disabled = !enabled;
    this.ui.button3.disabled = !enabled;
  },

  setDefaultFocus(isInitialLoad) {
    let b = this.args.defaultButtonNum || 0;
    let button = this.ui["button" + b];

    if (!this.hasInputField) {
      let isOSX = "nsILocalFileMac" in Ci;
      if (isOSX) {
        this.ui.infoBody.focus();
      } else {
        button.focus({ preventFocusRing: true });
      }
    } else if (this.args.promptType == "promptPassword") {
      // When the prompt is initialized, focus and select the textbox
      // contents. Afterwards, only focus the textbox.
      if (isInitialLoad) {
        this.ui.password1Textbox.select();
      } else {
        this.ui.password1Textbox.focus();
      }
    } else if (isInitialLoad) {
      this.ui.loginTextbox.select();
    } else {
      this.ui.loginTextbox.focus();
    }

    if (isInitialLoad) {
      this.initialFocusResolver();
    }
  },

  onCheckbox() {
    this.args.checked = this.ui.checkbox.checked;
  },

  onButton0() {
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

  onButton1() {
    this.args.promptActive = false;
    this.args.buttonNumClicked = 1;
  },

  onButton2() {
    this.args.promptActive = false;
    this.args.buttonNumClicked = 2;
  },

  onButton3() {
    this.args.promptActive = false;
    this.args.buttonNumClicked = 3;
  },

  abortPrompt() {
    this.args.promptActive = false;
    this.args.promptAborted = true;
  },
};
