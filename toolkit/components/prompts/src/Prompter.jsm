/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
// This is redefined below, for strange and unfortunate reasons.
var { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

const {
  MODAL_TYPE_TAB,
  MODAL_TYPE_CONTENT,
  MODAL_TYPE_WINDOW,
  MODAL_TYPE_INTERNAL_WINDOW,
} = Ci.nsIPrompt;

const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";
const SELECT_DIALOG = "chrome://global/content/selectDialog.xhtml";

function Prompter() {
  // Note that EmbedPrompter clones this implementation.
}

/**
 * Implements nsIPromptService and nsIPromptFactory
 * @class Prompter
 */
Prompter.prototype = {
  classID: Components.ID("{1c978d25-b37f-43a8-a2d6-0c7a239ead87}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIPromptFactory",
    "nsIPromptService",
  ]),

  /* ----------  private members  ---------- */

  pickPrompter(options) {
    return new ModalPrompter(options);
  },

  /* ----------  nsIPromptFactory  ---------- */

  getPrompt(domWin, iid) {
    // This is still kind of dumb; the C++ code delegated to login manager
    // here, which in turn calls back into us via nsIPromptService.
    if (iid.equals(Ci.nsIAuthPrompt2) || iid.equals(Ci.nsIAuthPrompt)) {
      try {
        let pwmgr = Cc[
          "@mozilla.org/passwordmanager/authpromptfactory;1"
        ].getService(Ci.nsIPromptFactory);
        return pwmgr.getPrompt(domWin, iid);
      } catch (e) {
        Cu.reportError(
          "nsPrompter: Delegation to password manager failed: " + e
        );
      }
    }

    let p = new ModalPrompter({ domWin });
    p.QueryInterface(iid);
    return p;
  },

  /* ----------  nsIPromptService  ---------- */

  /**
   * Puts up an alert dialog with an OK button.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   */
  alert(domWin, title, text) {
    let p = this.pickPrompter({ domWin });
    p.alert(title, text);
  },

  /**
   * Puts up an alert dialog with an OK button.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   */
  alertBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    p.alert(...promptArgs);
  },

  /**
   * Puts up an alert dialog with an OK button.
   *
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @returns {Promise} A promise which resolves when the prompt is dismissed.
   */
  asyncAlert(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.alert(...promptArgs);
  },

  /**
   * Puts up an alert dialog with an OK button and a labeled checkbox.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   */
  alertCheck(domWin, title, text, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    p.alertCheck(title, text, checkLabel, checkValue);
  },

  /**
   * Puts up an alert dialog with an OK button and a labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   */
  alertCheckBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    p.alertCheck(...promptArgs);
  },

  /**
   * Puts up an alert dialog with an OK button and a labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @returns {Promise<nsIPropertyBag<{ checked: Boolean }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncAlertCheck(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.alertCheck(...promptArgs);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  confirm(domWin, title, text) {
    let p = this.pickPrompter({ domWin });
    return p.confirm(title, text);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  confirmBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirm(...promptArgs);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncConfirm(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.confirm(...promptArgs);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons and a labeled checkbox.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   */
  confirmCheck(domWin, title, text, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.confirmCheck(title, text, checkLabel, checkValue);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons and a labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel
   */
  confirmCheckBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirmCheck(...promptArgs);
  },

  /**
   * Puts up a dialog with OK and Cancel buttons and a labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} checkLabel - Text to appear with the checkbox.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, checked: Boolean }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncConfirmCheck(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.confirmCheck(...promptArgs);
  },

  /**
   * Puts up a dialog with up to 3 buttons and an optional, labeled checkbox.
   *
   * Buttons are numbered 0 - 2. Button 0 is the default button unless one of
   * the Button Default Flags is specified.
   *
   * A button may use a predefined title, specified by one of the Button Title
   * Flags values.  Each title value can be multiplied by a position value to
   * assign the title to a particular button.  If BUTTON_TITLE_IS_STRING is
   * used for a button, the string parameter for that button will be used.  If
   * the value for a button position is zero, the button will not be shown.
   *
   * In general, flags is constructed per the following example:
   *
   * flags = (BUTTON_POS_0) * (BUTTON_TITLE_AAA) +
   *         (BUTTON_POS_1) * (BUTTON_TITLE_BBB) +
   *         BUTTON_POS_1_DEFAULT;
   *
   * where "AAA" and "BBB" correspond to one of the button titles.
   *
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Number} flags - A combination of Button Flags.
   * @param {String} button0 - Used when button 0 uses TITLE_IS_STRING.
   * @param {String} button1 - Used when button 1 uses TITLE_IS_STRING.
   * @param {String} button2 - Used when button 2 uses TITLE_IS_STRING.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        Null if no checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method
   *        is called and the final checked state after this method returns.
   * @returns {Number} The index of the button pressed.
   */
  confirmEx(
    domWin,
    title,
    text,
    flags,
    button0,
    button1,
    button2,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.confirmEx(
      title,
      text,
      flags,
      button0,
      button1,
      button2,
      checkLabel,
      checkValue
    );
  },

  /**
   * Puts up a dialog with up to 3 buttons and an optional, labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Number} flags - A combination of Button Flags.
   * @param {String} button0 - Used when button 0 uses TITLE_IS_STRING.
   * @param {String} button1 - Used when button 1 uses TITLE_IS_STRING.
   * @param {String} button2 - Used when button 2 uses TITLE_IS_STRING.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        Null if no checkbox.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Number} The index of the button pressed.
   */
  confirmExBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.confirmEx(...promptArgs);
  },

  /**
   * Puts up a dialog with up to 3 buttons and an optional, labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Number} flags - A combination of Button Flags.
   * @param {String} button0 - Used when button 0 uses TITLE_IS_STRING.
   * @param {String} button1 - Used when button 1 uses TITLE_IS_STRING.
   * @param {String} button2 - Used when button 2 uses TITLE_IS_STRING.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        Null if no checkbox.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @param {Object} [extraArgs] - Extra arguments for the prompt metadata.
   * @returns {Promise<nsIPropertyBag<{ buttonNumClicked: Number, checked: Boolean }>>}
   */
  asyncConfirmEx(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.confirmEx(...promptArgs);
  },

  /**
   * Puts up a dialog with an edit field and an optional, labeled checkbox.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} value - Contains the default value for the dialog field
   *        when this method is called (null value is ok).  Upon return, if
   *        the user pressed OK, then this parameter contains a newly
   *        allocated string value.
   *        Otherwise, the parameter's value is unmodified.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  prompt(domWin, title, text, value, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_prompt(title, text, value, checkLabel, checkValue);
  },

  /**
   * Puts up a dialog with an edit field and an optional, labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} value - Contains the default value for the dialog field
   *        when this method is called (null value is ok).  Upon return, if
   *        the user pressed OK, then this parameter contains a newly
   *        allocated string value.
   *        Otherwise, the parameter's value is unmodified.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_prompt(...promptArgs);
  },

  /**
   * Puts up a dialog with an edit field and an optional, labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} value - The default value for the dialog text field.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, checked: Boolean, value: String }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPrompt(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.nsIPrompt_prompt(...promptArgs);
  },

  /**
   * Puts up a dialog with an edit field and a password field.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} user - Contains the default value for the username
   *        field when this method is called (null value is ok).
   *        Upon return, if the user pressed OK, then this parameter contains
   *        a newly allocated string value. Otherwise, the parameter's value
   *        is unmodified.
   * @param {Object} pass - Contains the default value for the password field
   *        when this method is called (null value is ok). Upon return, if the
   *        user pressed OK, this parameter contains a newly allocated string
   *        value. Otherwise, the parameter's value is unmodified.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptUsernameAndPassword(domWin, title, text, user, pass) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptUsernameAndPassword(null, title, text, user, pass);
  },

  /**
   * Puts up a dialog with an edit field and a password field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} user - Contains the default value for the username
   *        field when this method is called (null value is ok).
   *        Upon return, if the user pressed OK, then this parameter contains
   *        a newly allocated string value. Otherwise, the parameter's value
   *        is unmodified.
   * @param {Object} pass - Contains the default value for the password field
   *        when this method is called (null value is ok). Upon return, if the
   *        user pressed OK, this parameter contains a newly allocated string
   *        value. Otherwise, the parameter's value is unmodified.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptUsernameAndPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptUsernameAndPassword(null, ...promptArgs);
  },

  /**
   * Puts up a dialog with an edit field and a password field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} user - Default value for the username field.
   * @param {String} pass - Contains the default value for the password field.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, user: String, pass: String }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPromptUsernameAndPassword(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.nsIPrompt_promptUsernameAndPassword(null, ...promptArgs);
  },

  /**
   * Puts up a dialog with a password field.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} pass - Contains the default value for the password field
   *        when this method is called (null value is ok). Upon return, if the
   *        user pressed OK, this parameter contains a newly allocated string
   *        value. Otherwise, the parameter's value is unmodified.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptPassword(domWin, title, text, pass) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptPassword(
      null, // no channel.
      title,
      text,
      pass
    );
  },

  /**
   * Puts up a dialog with a password field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} pass - Contains the default value for the password field
   *        when this method is called (null value is ok). Upon return, if the
   *        user pressed OK, this parameter contains a newly allocated string
   *        value. Otherwise, the parameter's value is unmodified.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptPassword(null, ...promptArgs);
  },

  /**
   * Puts up a dialog with a password field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} pass - Contains the default value for the password field.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, pass: String }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPromptPassword(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.nsIPrompt_promptPassword(null, ...promptArgs);
  },

  /**
   * Puts up a dialog box which has a list box of strings from which the user
   * may make a single selection.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String[]} list - The list of strings to display.
   * @param {Object} selected - Contains the index of the selected item in the
   *        list when this method returns true.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  select(domWin, title, text, list, selected) {
    let p = this.pickPrompter({ domWin });
    return p.select(title, text, list, selected);
  },

  /**
   * Puts up a dialog box which has a list box of strings from which the user
   * may make a single selection.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String[]} list - The list of strings to display.
   * @param {Object} selected - Contains the index of the selected item in the
   *        list when this method returns true.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  selectBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.select(...promptArgs);
  },

  /**
   * Puts up a dialog box which has a list box of strings from which the user
   * may make a single selection.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String[]} list - The list of strings to display.
   * @returns {Promise<nsIPropertyBag<{ selected: Number, ok: Boolean  }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncSelect(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.select(...promptArgs);
  },

  /**
   * Requests a username and a password. Shows a dialog with username and
   * password field, depending on flags also a domain field.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {nsIChannel} channel - The channel that requires authentication.
   * @param {Number} level - Security level of the credential transmission.
   *        Any of nsIAuthPrompt2.<LEVEL_NONE|LEVEL_PW_ENCRYPTED|LEVEL_SECURE>
   * @param {nsIAuthInformation} authInfo - Authentication information object.
   * @returns {Boolean}
   *          true: Authentication can proceed using the values
   *          in the authInfo object.
   *          false: Authentication should be cancelled, usually because the
   *          user did not provide username/password.
   */
  promptAuth(domWin, channel, level, authInfo) {
    let p = this.pickPrompter({ domWin });
    return p.promptAuth(channel, level, authInfo);
  },

  /**
   * Requests a username and a password. Shows a dialog with username and
   * password field, depending on flags also a domain field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {nsIChannel} channel - The channel that requires authentication.
   * @param {Number} level - Security level of the credential transmission.
   *        Any of nsIAuthPrompt2.<LEVEL_NONE|LEVEL_PW_ENCRYPTED|LEVEL_SECURE>
   * @param {nsIAuthInformation} authInfo - Authentication information object.
   * @returns {Boolean}
   *          true: Authentication can proceed using the values
   *          in the authInfo object.
   *          false: Authentication should be cancelled, usually because the
   *          user did not provide username/password.
   */
  promptAuthBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.promptAuth(...promptArgs);
  },

  /**
   * Requests a username and a password. Shows a dialog with username and
   * password field, depending on flags also a domain field.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {nsIChannel} channel - The channel that requires authentication.
   * @param {Number} level - Security level of the credential transmission.
   *        Any of nsIAuthPrompt2.<LEVEL_NONE|LEVEL_PW_ENCRYPTED|LEVEL_SECURE>
   * @param {nsIAuthInformation} authInfo - Authentication information object.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPromptAuth(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.promptAuth(...promptArgs);
  },
};

// Common utils not specific to a particular prompter style.
var PromptUtilsTemp = {
  __proto__: PromptUtils,

  getLocalizedString(key, formatArgs) {
    if (formatArgs) {
      return this.strBundle.formatStringFromName(key, formatArgs);
    }
    return this.strBundle.GetStringFromName(key);
  },

  confirmExHelper(flags, button0, button1, button2) {
    const BUTTON_DEFAULT_MASK = 0x03000000;
    let defaultButtonNum = (flags & BUTTON_DEFAULT_MASK) >> 24;
    let isDelayEnabled = flags & Ci.nsIPrompt.BUTTON_DELAY_ENABLE;

    // Flags can be used to select a specific pre-defined button label or
    // a caller-supplied string (button0/button1/button2). If no flags are
    // set for a button, then the button won't be shown.
    let argText = [button0, button1, button2];
    let buttonLabels = [null, null, null];
    for (let i = 0; i < 3; i++) {
      let buttonLabel;
      switch (flags & 0xff) {
        case Ci.nsIPrompt.BUTTON_TITLE_OK:
          buttonLabel = PromptUtils.getLocalizedString("OK");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_CANCEL:
          buttonLabel = PromptUtils.getLocalizedString("Cancel");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_YES:
          buttonLabel = PromptUtils.getLocalizedString("Yes");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_NO:
          buttonLabel = PromptUtils.getLocalizedString("No");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_SAVE:
          buttonLabel = PromptUtils.getLocalizedString("Save");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_DONT_SAVE:
          buttonLabel = PromptUtils.getLocalizedString("DontSave");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_REVERT:
          buttonLabel = PromptUtils.getLocalizedString("Revert");
          break;
        case Ci.nsIPrompt.BUTTON_TITLE_IS_STRING:
          buttonLabel = argText[i];
          break;
      }
      if (buttonLabel) {
        buttonLabels[i] = buttonLabel;
      }
      flags >>= 8;
    }

    return [
      buttonLabels[0],
      buttonLabels[1],
      buttonLabels[2],
      defaultButtonNum,
      isDelayEnabled,
    ];
  },

  getAuthInfo(authInfo) {
    let username, password;

    let flags = authInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN && authInfo.domain) {
      username = authInfo.domain + "\\" + authInfo.username;
    } else {
      username = authInfo.username;
    }

    password = authInfo.password;

    return [username, password];
  },

  setAuthInfo(authInfo, username, password) {
    let flags = authInfo.flags;
    if (flags & Ci.nsIAuthInformation.NEED_DOMAIN) {
      // Domain is separated from username by a backslash
      let idx = username.indexOf("\\");
      if (idx == -1) {
        authInfo.username = username;
      } else {
        authInfo.domain = username.substring(0, idx);
        authInfo.username = username.substring(idx + 1);
      }
    } else {
      authInfo.username = username;
    }
    authInfo.password = password;
  },

  /**
   * Strip out things like userPass and path for display.
   */
  getFormattedHostname(uri) {
    return uri.scheme + "://" + uri.hostPort;
  },

  // Note: there's a similar implementation in the login manager.
  getAuthTarget(aChannel, aAuthInfo) {
    let displayHost, realm;

    // If our proxy is demanding authentication, don't use the
    // channel's actual destination.
    if (aAuthInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY) {
      if (!(aChannel instanceof Ci.nsIProxiedChannel)) {
        throw new Error("proxy auth needs nsIProxiedChannel");
      }

      let info = aChannel.proxyInfo;
      if (!info) {
        throw new Error("proxy auth needs nsIProxyInfo");
      }

      // Proxies don't have a scheme, but we'll use "moz-proxy://"
      // so that it's more obvious what the login is for.
      let idnService = Cc["@mozilla.org/network/idn-service;1"].getService(
        Ci.nsIIDNService
      );
      displayHost =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      realm = aAuthInfo.realm;
      if (!realm) {
        realm = displayHost;
      }

      return { realm, displayHost };
    }

    displayHost = this.getFormattedHostname(aChannel.URI);
    let displayHostOnly = aChannel.URI.hostPort;

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = aAuthInfo.realm;
    if (!realm) {
      realm = displayHost;
    }

    return { realm, displayHostOnly, displayHost };
  },

  makeAuthMessage(prompt, channel, authInfo) {
    if (prompt.modalType != MODAL_TYPE_TAB) {
      return this._legacyMakeAuthMessage(channel, authInfo);
    }

    let isProxy = authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY;
    let isPassOnly = authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD;
    let isCrossOrig =
      authInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;
    let username = authInfo.username;

    // We use the realm and displayHost only for proxy auth,
    // and the displayHostOnly (hostPort) only for x-origin auth prompts.
    // Otherwise we rely on the title of the dialog displaying the correct
    // title.
    let { displayHost, realm, displayHostOnly } = this.getAuthTarget(
      channel,
      authInfo
    );

    if (isProxy) {
      // The realm is server-controlled. Trim it if it's very long, to
      // avoid the dialog becoming unusable.
      // For background, see https://bugzilla.mozilla.org/show_bug.cgi?id=244273
      if (realm.length > 150) {
        realm = realm.substring(0, 150);
        // Append "..." (or localized equivalent).
        realm += this.ellipsis;
      }

      return PromptUtils.getLocalizedString("EnterLoginForProxy3", [
        realm,
        displayHost,
      ]);
    }
    if (isPassOnly) {
      return PromptUtils.getLocalizedString("EnterPasswordOnlyFor", [username]);
    }
    if (isCrossOrig) {
      return PromptUtils.getLocalizedString("EnterCredentialsCrossOrigin", [
        displayHostOnly,
      ]);
    }
    return PromptUtils.getLocalizedString("EnterCredentials");
  },

  _legacyMakeAuthMessage(channel, authInfo) {
    let isProxy = authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY;
    let isPassOnly = authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD;
    let isCrossOrig =
      authInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;

    let username = authInfo.username;
    let { displayHost, realm } = this.getAuthTarget(channel, authInfo);

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!authInfo.realm && !isProxy) {
      realm = "";
    }

    // The realm is server-controlled. Trim it if it's very long, to
    // avoid the dialog becoming unusable.
    // For background, see https://bugzilla.mozilla.org/show_bug.cgi?id=244273
    if (realm.length > 150) {
      realm = realm.substring(0, 150);
      // Append "..." (or localized equivalent).
      realm += this.ellipsis;
    }

    let text;
    if (isProxy) {
      text = PromptUtils.getLocalizedString("EnterLoginForProxy3", [
        realm,
        displayHost,
      ]);
    } else if (isPassOnly) {
      text = PromptUtils.getLocalizedString("EnterPasswordFor", [
        username,
        displayHost,
      ]);
    } else if (isCrossOrig) {
      text = PromptUtils.getLocalizedString(
        "EnterUserPasswordForCrossOrigin2",
        [displayHost]
      );
    } else if (!realm) {
      text = PromptUtils.getLocalizedString("EnterUserPasswordFor2", [
        displayHost,
      ]);
    } else {
      text = PromptUtils.getLocalizedString("EnterLoginForRealm3", [
        realm,
        displayHost,
      ]);
    }

    return text;
  },

  getBrandFullName() {
    return this.brandBundle.GetStringFromName("brandFullName");
  },
};

PromptUtils = PromptUtilsTemp;

XPCOMUtils.defineLazyGetter(PromptUtils, "strBundle", function() {
  let bundle = Services.strings.createBundle(
    "chrome://global/locale/commonDialogs.properties"
  );
  if (!bundle) {
    throw new Error("String bundle for Prompter not present!");
  }
  return bundle;
});

XPCOMUtils.defineLazyGetter(PromptUtils, "brandBundle", function() {
  let bundle = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
  if (!bundle) {
    throw new Error("String bundle for branding not present!");
  }
  return bundle;
});

XPCOMUtils.defineLazyGetter(PromptUtils, "ellipsis", function() {
  let ellipsis = "\u2026";
  try {
    ellipsis = Services.prefs.getComplexValue(
      "intl.ellipsis",
      Ci.nsIPrefLocalizedString
    ).data;
  } catch (e) {}
  return ellipsis;
});

class ModalPrompter {
  constructor({
    browsingContext = null,
    domWin = null,
    modalType = null,
    async = false,
  }) {
    if (browsingContext && domWin) {
      throw new Error("Pass either browsingContext or domWin");
    }

    if (domWin) {
      // We have a domWin, get the associated browsing context
      this.browsingContext = BrowsingContext.getFromWindow(domWin);
    } else {
      this.browsingContext = browsingContext;
    }

    if (
      domWin &&
      (!modalType || modalType == MODAL_TYPE_WINDOW) &&
      !this.browsingContext?.isContent &&
      this.browsingContext?.associatedWindow?.gDialogBox
    ) {
      modalType = MODAL_TYPE_INTERNAL_WINDOW;
    }

    // Use given modal type or fallback to default
    this.modalType = modalType || ModalPrompter.defaultModalType;

    this.async = async;

    this.QueryInterface = ChromeUtils.generateQI([
      "nsIPrompt",
      "nsIAuthPrompt",
      "nsIAuthPrompt2",
      "nsIWritablePropertyBag2",
    ]);
  }

  set modalType(modalType) {
    // Setting modal type window is always allowed
    if (modalType == MODAL_TYPE_WINDOW) {
      this._modalType = modalType;
      return;
    }

    // For content prompts for non-content windows, use window prompts:
    if (modalType == MODAL_TYPE_CONTENT && !this.browsingContext?.isContent) {
      this._modalType = MODAL_TYPE_WINDOW;
      return;
    }

    // We can't use content / tab prompts if we don't have a suitable parent.
    if (
      !this.browsingContext?.isContent &&
      modalType != MODAL_TYPE_INTERNAL_WINDOW
    ) {
      // Only show this error if we're not about to fall back again and show a different one.
      if (this.browsingContext?.associatedWindow?.gDialogBox) {
        Cu.reportError(
          "Prompter: Browser not available. Falling back to internal window prompt."
        );
      }
      modalType = MODAL_TYPE_INTERNAL_WINDOW;
    }

    if (
      modalType == MODAL_TYPE_INTERNAL_WINDOW &&
      (this.browsingContext?.isContent ||
        !this.browsingContext?.associatedWindow?.gDialogBox)
    ) {
      Cu.reportError(
        "Prompter: internal dialogs not available in this context. Falling back to window prompt."
      );
      modalType = MODAL_TYPE_WINDOW;
    }

    this._modalType = modalType;
  }

  get modalType() {
    return this._modalType;
  }

  /* ---------- internal methods ---------- */

  /**
   * Synchronous wrapper around {@link ModalPrompter#openPrompt}
   * @param {Object} args Prompt arguments. When prompt has been closed, they are updated to reflect the result state.
   */
  openPromptSync(args) {
    let closed = false;
    this.openPrompt(args)
      .then(returnedArgs => {
        if (returnedArgs) {
          for (let key in returnedArgs) {
            args[key] = returnedArgs[key];
          }
        }
      })
      .finally(() => {
        closed = true;
      });
    Services.tm.spinEventLoopUntilOrQuit(
      "prompts/Prompter.jsm:openPromptSync",
      () => closed
    );
  }

  async openPrompt(args) {
    if (!this.browsingContext) {
      // We don't have a browsing context, fallback to a window prompt.
      args.modalType = MODAL_TYPE_WINDOW;
      this.openWindowPrompt(null, args);
      return args;
    }

    if (this._modalType == MODAL_TYPE_INTERNAL_WINDOW) {
      await this.openInternalWindowPrompt(
        this.browsingContext.associatedWindow,
        args
      );
      return args;
    }

    // Select prompts are not part of CommonDialog
    // and thus not supported as tab or content prompts yet. See Bug 1622817.
    // Once they are integrated this override should be removed.
    if (args.promptType == "select" && this.modalType !== MODAL_TYPE_WINDOW) {
      Cu.reportError(
        "Prompter: 'select' prompts do not support tab/content prompting. Falling back to window prompt."
      );
      args.modalType = MODAL_TYPE_WINDOW;
    } else {
      args.modalType = this.modalType;
    }

    const IS_CONTENT =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT;

    let actor;
    try {
      if (IS_CONTENT) {
        // When in the content, get the PromptChild actor.
        actor = this.browsingContext.window.windowGlobalChild.getActor(
          "Prompt"
        );
      } else {
        // When in the parent, get the PromptParent actor.
        actor = this.browsingContext.currentWindowGlobal.getActor("Prompt");
      }
    } catch (_) {
      // We can't get the prompt actor, fallback to window prompt.
      let parentWin;
      // If given a chrome BC we can try to get its window
      if (!this.browsingContext.isContent && this.browsingContext.window) {
        parentWin = this.browsingContext.window;
      } else {
        // Try to get the window which is the browsers parent
        parentWin = this.browsingContext.top?.embedderElement?.ownerGlobal;
      }
      this.openWindowPrompt(parentWin, args);
      return args;
    }

    /* For prompts with a channel, we want to show the origin requesting
     * authentication. This is different from the prompt principal,
     * which is based on the document loaded in the browsing context over
     * which the prompt appears. So if page foo.com loads bar.com, and the
     * latter asks for auth, we want that bar.com's origin, not foo.com.
     * To avoid confusion, we use different properties
     * (authOrigin / promptPrincipal) to track this information.
     */
    if (args.channel) {
      try {
        args.authOrigin = args.channel.URI.hostPort;
      } catch (ex) {
        args.authOrigin = args.channel.URI.prePath;
      }
      args.isInsecureAuth =
        args.channel.URI.schemeIs("http") &&
        !args.channel.loadInfo.isTopLevelLoad;
    } else {
      args.promptPrincipal = this.browsingContext.window?.document.nodePrincipal;
    }
    if (IS_CONTENT) {
      let docShell = this.browsingContext.docShell;
      let inPermitUnload = docShell?.contentViewer?.inPermitUnload;
      args.inPermitUnload = inPermitUnload;
      let eventDetail = Cu.cloneInto(
        {
          tabPrompt: this.modalType != MODAL_TYPE_WINDOW,
          inPermitUnload,
        },
        this.browsingContext.window
      );
      PromptUtils.fireDialogEvent(
        this.browsingContext.window,
        "DOMWillOpenModalDialog",
        null,
        eventDetail
      );

      // Put content window in the modal state while the prompt is open.
      let windowUtils = this.browsingContext.window?.windowUtils;
      if (windowUtils) {
        windowUtils.enterModalState();
      }
    } else if (args.inPermitUnload) {
      args.promptPrincipal = this.browsingContext.currentWindowGlobal.documentPrincipal;
    }

    // It is technically possible for multiple prompts to be sent from a single
    // BrowsingContext. See bug 1266353. We use a randomly generated UUID to
    // differentiate between the different prompts.
    let id = "id" + Services.uuid.generateUUID().toString();

    args._remoteId = id;

    let returnedArgs;
    try {
      if (IS_CONTENT) {
        // If we're in the content process, send a message to the PromptParent
        // window actor.
        returnedArgs = await actor.sendQuery("Prompt:Open", args);
      } else {
        // If we're in the parent process we already have the parent actor.
        // We can call its message handler directly.
        returnedArgs = await actor.receiveMessage({
          name: "Prompt:Open",
          data: args,
        });
      }

      if (returnedArgs?.promptAborted) {
        throw Components.Exception(
          "prompt aborted by user",
          Cr.NS_ERROR_NOT_AVAILABLE
        );
      }
    } finally {
      if (IS_CONTENT) {
        let windowUtils = this.browsingContext.window?.windowUtils;
        if (windowUtils) {
          windowUtils.leaveModalState();
        }
        PromptUtils.fireDialogEvent(
          this.browsingContext.window,
          "DOMModalDialogClosed"
        );
      }
    }
    return returnedArgs;
  }

  /**
   * Open a window modal prompt
   *
   * There's an implied contract that says modal prompts should still work when
   * no "parent" window is passed for the dialog (eg, the "Master Password"
   * dialog does this).  These prompts must be shown even if there are *no*
   * visible windows at all.
   * We try and find a window to use as the parent, but don't consider if that
   * is visible before showing the prompt. parentWindow may still be null if
   * there are _no_ windows open.
   * @param {Window} [parentWindow] - The parent window for the prompt, may be
   *        null.
   * @param {Object} args - Prompt options and return values.
   */
  openWindowPrompt(parentWindow = null, args) {
    let uri = args.promptType == "select" ? SELECT_DIALOG : COMMON_DIALOG;
    let propBag = PromptUtils.objectToPropBag(args);
    Services.ww.openWindow(
      parentWindow || Services.ww.activeWindow,
      uri,
      "_blank",
      "centerscreen,chrome,modal,titlebar",
      propBag
    );
    PromptUtils.propBagToObject(propBag, args);
  }

  async openInternalWindowPrompt(parentWindow, args) {
    if (!parentWindow?.gDialogBox || !ModalPrompter.windowPromptSubDialog) {
      this.openWindowPrompt(parentWindow, args);
      return;
    }
    let propBag = PromptUtils.objectToPropBag(args);
    propBag.setProperty("async", this.async);
    let uri = args.promptType == "select" ? SELECT_DIALOG : COMMON_DIALOG;
    await parentWindow.gDialogBox.open(uri, propBag);
    propBag.deleteProperty("async");
    PromptUtils.propBagToObject(propBag, args);
  }

  /**
   * Calls async prompt method and optionally runs promise chained task on
   * result data. Converts result data to nsIPropertyBag.
   * @param {Object} args - Prompt arguments.
   * @param {Function} [task] - Function which is called with the modified
   *  prompt args object once the prompt has been closed. Must return a
   *  result object for the prompt caller.
   * @returns {Promise<nsIPropertyBag>} - Resolves with a property bag holding the
   * prompt result properties. Resolves once prompt has been closed.
   */
  async openPromptAsync(args, task) {
    let result = await this.openPrompt(args);
    // If task is not defined, the prompt method does not return
    // anything. In this case we can resolve without value.
    if (!task) {
      return undefined;
    }
    // Convert task result to nsIPropertyBag and resolve
    let taskResult = task(result);
    if (!(taskResult instanceof Object)) {
      throw new Error("task must return object");
    }
    return PromptUtils.objectToPropBag(taskResult);
  }

  /*
   * ---------- interface disambiguation ----------
   *
   * nsIPrompt and nsIAuthPrompt share 3 method names with slightly
   * different arguments. All but prompt() have the same number of
   * arguments, so look at the arg types to figure out how we're being
   * called. :-(
   */
  prompt() {
    // also, the nsIPrompt flavor has 5 args instead of 6.
    if (typeof arguments[2] == "object") {
      return this.nsIPrompt_prompt.apply(this, arguments);
    }
    return this.nsIAuthPrompt_prompt.apply(this, arguments);
  }

  promptUsernameAndPassword() {
    // Both have 6 args, so use types.
    if (typeof arguments[2] == "object") {
      // Add the null channel:
      let args = Array.from(arguments);
      args.unshift(null);
      return this.nsIPrompt_promptUsernameAndPassword.apply(this, args);
    }
    return this.nsIAuthPrompt_promptUsernameAndPassword.apply(this, arguments);
  }

  promptPassword() {
    // Both have 5 args, so use types.
    if (typeof arguments[2] == "object") {
      // Add the null channel:
      let args = Array.from(arguments);
      args.unshift(null);
      return this.nsIPrompt_promptPassword.apply(this, args);
    }
    return this.nsIAuthPrompt_promptPassword.apply(this, arguments);
  }

  /* ----------  nsIPrompt  ---------- */

  alert(title, text) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Alert");
    }

    let args = {
      promptType: "alert",
      title,
      text,
    };

    if (this.async) {
      return this.openPromptAsync(args);
    }

    return this.openPromptSync(args);
  }

  alertCheck(title, text, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Alert");
    }

    // For sync calls checkValue is an XPCOM inout. XPCOM wraps primitves in
    // objects for call by reference.
    // The async version of this method uses call by value.
    let checked = this.async ? checkValue : checkValue.value;

    let args = {
      promptType: "alertCheck",
      title,
      text,
      checkLabel,
      checked,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        checked: result.checked,
      }));
    }

    this.openPromptSync(args);
    checkValue.value = args.checked;
    return undefined;
  }

  confirm(title, text) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Confirm");
    }

    let args = {
      promptType: "confirm",
      title,
      text,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({ ok: result.ok }));
    }

    this.openPromptSync(args);
    return args.ok;
  }

  confirmCheck(title, text, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("ConfirmCheck");
    }

    let checked = this.async ? checkValue : checkValue.value;

    let args = {
      promptType: "confirmCheck",
      title,
      text,
      checkLabel,
      checked,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        // Checkbox state always returned, even if cancel clicked.
        checked: result.checked,
        // Did user click Ok or Cancel?
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);
    checkValue.value = args.checked;
    return args.ok;
  }

  confirmEx(
    title,
    text,
    flags,
    button0,
    button1,
    button2,
    checkLabel,
    checkValue,
    extraArgs = {}
  ) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Confirm");
    }

    let args = {
      promptType: "confirmEx",
      title,
      text,
      checkLabel,
      checked: this.async ? checkValue : checkValue.value,
      ok: false,
      buttonNumClicked: 1,
      ...extraArgs,
    };

    let [
      label0,
      label1,
      label2,
      defaultButtonNum,
      isDelayEnabled,
    ] = PromptUtils.confirmExHelper(flags, button0, button1, button2);

    args.defaultButtonNum = defaultButtonNum;
    args.enableDelay = isDelayEnabled;

    if (label0) {
      args.button0Label = label0;
      if (label1) {
        args.button1Label = label1;
        if (label2) {
          args.button2Label = label2;
        }
      }
    }

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        checked: !!result.checked,
        buttonNumClicked: result.buttonNumClicked,
      }));
    }

    this.openPromptSync(args);
    checkValue.value = args.checked;
    return args.buttonNumClicked;
  }

  nsIPrompt_prompt(title, text, value, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Prompt");
    }

    let args = {
      promptType: "prompt",
      title,
      text,
      value: this.async ? value : value.value,
      checkLabel,
      checked: this.async ? checkValue : checkValue.value,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        checked: !!result.checked,
        value: result.value,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
      value.value = args.value;
    }

    return ok;
  }

  nsIPrompt_promptUsernameAndPassword(channel, title, text, user, pass) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptUsernameAndPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      channel,
      promptType: "promptUserAndPass",
      title,
      text,
      user: this.async ? user : user.value,
      pass: this.async ? pass : pass.value,
      button0Label: PromptUtils.getLocalizedString("SignIn"),
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        user: result.user,
        pass: result.pass,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      user.value = args.user;
      pass.value = args.pass;
    }

    return ok;
  }

  nsIPrompt_promptPassword(channel, title, text, pass) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      channel,
      promptType: "promptPassword",
      title,
      text,
      pass: this.async ? pass : pass.value,
      button0Label: PromptUtils.getLocalizedString("SignIn"),
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        pass: result.pass,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      pass.value = args.pass;
    }

    return ok;
  }

  select(title, text, list, selected) {
    if (!title) {
      title = PromptUtils.getLocalizedString("Select");
    }

    let args = {
      promptType: "select",
      title,
      text,
      list,
      selected: -1,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        selected: result.selected,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      selected.value = args.selected;
    }

    return ok;
  }

  /* ----------  nsIAuthPrompt  ---------- */

  nsIAuthPrompt_prompt(
    title,
    text,
    passwordRealm,
    savePassword,
    defaultText,
    result
  ) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    if (defaultText) {
      result.value = defaultText;
    }
    return this.nsIPrompt_prompt(title, text, result, null, {});
  }

  nsIAuthPrompt_promptUsernameAndPassword(
    title,
    text,
    passwordRealm,
    savePassword,
    user,
    pass
  ) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    return this.nsIPrompt_promptUsernameAndPassword(
      null,
      title,
      text,
      user,
      pass
    );
  }

  nsIAuthPrompt_promptPassword(title, text, passwordRealm, savePassword, pass) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp,
    // and we don't have a channel here.
    return this.nsIPrompt_promptPassword(null, title, text, pass);
  }

  /* ----------  nsIAuthPrompt2  ---------- */

  promptAuth(channel, level, authInfo) {
    let message = PromptUtils.makeAuthMessage(this, channel, authInfo);

    let [username, password] = PromptUtils.getAuthInfo(authInfo);

    let userParam = this.async ? username : { value: username };
    let passParam = this.async ? password : { value: password };

    let result;
    if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      result = this.nsIPrompt_promptPassword(channel, null, message, passParam);
    } else {
      result = this.nsIPrompt_promptUsernameAndPassword(
        channel,
        null,
        message,
        userParam,
        passParam
      );
    }

    // For the async case result is an nsIPropertyBag with prompt results.
    if (this.async) {
      return result.then(bag => {
        let ok = bag.getProperty("ok");
        if (ok) {
          let username = bag.getProperty("user");
          let password = bag.getProperty("pass");
          PromptUtils.setAuthInfo(authInfo, username, password);
        }
        return ok;
      });
    }

    // For the sync case result is the "ok" boolean which indicates whether
    // the user has confirmed the dialog.
    if (result) {
      PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
    }
    return result;
  }

  asyncPromptAuth(
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    // Nothing calls this directly; netwerk ends up going through
    // nsIPromptService::GetPrompt, which delegates to login manager.
    // Login manger handles the async bits itself, and only calls out
    // promptAuth, never asyncPromptAuth.
    //
    // Bug 565582 will change this.
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  /* ----------  nsIWritablePropertyBag2 ---------- */
  // Legacy way to set modal type when prompting via nsIPrompt.
  // Please prompt via nsIPromptService. This will be removed in the future.
  setPropertyAsUint32(name, value) {
    if (name == "modalType") {
      this.modalType = value;
    } else {
      throw Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
    }
  }
}

XPCOMUtils.defineLazyPreferenceGetter(
  ModalPrompter,
  "defaultModalType",
  "prompts.defaultModalType",
  MODAL_TYPE_WINDOW
);

XPCOMUtils.defineLazyPreferenceGetter(
  ModalPrompter,
  "windowPromptSubDialog",
  "prompts.windowPromptSubDialog",
  false
);

function AuthPromptAdapterFactory() {}
AuthPromptAdapterFactory.prototype = {
  classID: Components.ID("{6e134924-6c3a-4d86-81ac-69432dd971dc}"),
  QueryInterface: ChromeUtils.generateQI(["nsIAuthPromptAdapterFactory"]),

  /* ----------  nsIAuthPromptAdapterFactory ---------- */

  createAdapter(oldPrompter) {
    return new AuthPromptAdapter(oldPrompter);
  },
};

// Takes an nsIAuthPrompt implementation, wraps it with a nsIAuthPrompt2 shell.
function AuthPromptAdapter(oldPrompter) {
  this.oldPrompter = oldPrompter;
}
AuthPromptAdapter.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),
  oldPrompter: null,

  /* ----------  nsIAuthPrompt2 ---------- */

  promptAuth(channel, level, authInfo, checkLabel, checkValue) {
    let message = PromptUtils.makeAuthMessage(
      this.oldPrompter,
      channel,
      authInfo
    );

    let [username, password] = PromptUtils.getAuthInfo(authInfo);
    let userParam = { value: username };
    let passParam = { value: password };

    let { displayHost, realm } = PromptUtils.getAuthTarget(channel, authInfo);
    let authTarget = displayHost + " (" + realm + ")";

    let ok;
    if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      ok = this.oldPrompter.promptPassword(
        null,
        message,
        authTarget,
        Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY,
        passParam
      );
    } else {
      ok = this.oldPrompter.promptUsernameAndPassword(
        null,
        message,
        authTarget,
        Ci.nsIAuthPrompt.SAVE_PASSWORD_PERMANENTLY,
        userParam,
        passParam
      );
    }

    if (ok) {
      PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
    }
    return ok;
  },

  asyncPromptAuth(
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

// Wrapper using the old embedding contractID, since it's already common in
// the addon ecosystem.
function EmbedPrompter() {}
EmbedPrompter.prototype = new Prompter();
EmbedPrompter.prototype.classID = Components.ID(
  "{7ad1b327-6dfa-46ec-9234-f2a620ea7e00}"
);

var EXPORTED_SYMBOLS = [
  "Prompter",
  "EmbedPrompter",
  "AuthPromptAdapterFactory",
];
