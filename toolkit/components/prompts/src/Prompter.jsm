/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { objectToPropBag } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
).BrowserUtils;
// This is redefined below, for strange and unfortunate reasons.
var { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

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
    Ci.nsIPromptFactory,
    Ci.nsIPromptService,
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
   * Puts up a dialog with an edit field, a password field, and an optional,
   * labeled checkbox.
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
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptUsernameAndPassword(
    domWin,
    title,
    text,
    user,
    pass,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptUsernameAndPassword(
      title,
      text,
      user,
      pass,
      checkLabel,
      checkValue
    );
  },

  /**
   * Puts up a dialog with an edit field, a password field, and an optional,
   * labeled checkbox.
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
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptUsernameAndPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptUsernameAndPassword(...promptArgs);
  },

  /**
   * Puts up a dialog with an edit field, a password field, and an optional,
   * labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} user - Default value for the username field.
   * @param {String} pass - Contains the default value for the password field.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, checked: Boolean, user: String, pass: String }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPromptUsernameAndPassword(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.nsIPrompt_promptUsernameAndPassword(...promptArgs);
  },

  /**
   * Puts up a dialog with a password field and an optional, labeled checkbox.
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {Object} pass - Contains the default value for the password field
   *        when this method is called (null value is ok). Upon return, if the
   *        user pressed OK, this parameter contains a newly allocated string
   *        value. Otherwise, the parameter's value is unmodified.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptPassword(domWin, title, text, pass, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.nsIPrompt_promptPassword(
      title,
      text,
      pass,
      checkLabel,
      checkValue
    );
  },

  /**
   * Puts up a dialog with a password field and an optional, labeled checkbox.
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
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean} true for OK, false for Cancel.
   */
  promptPasswordBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.nsIPrompt_promptPassword(...promptArgs);
  },

  /**
   * Puts up a dialog with a password field and an optional, labeled checkbox.
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} title - Text to appear in the title of the dialog.
   * @param {String} text - Text to appear in the body of the dialog.
   * @param {String} pass - Contains the default value for the password field.
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Boolean} checkValue - The initial checked state of the checkbox.
   * @returns {Promise<nsIPropertyBag<{ ok: Boolean, checked: Boolean, pass: String }>>}
   *          A promise which resolves when the prompt is dismissed.
   */
  asyncPromptPassword(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType, async: true });
    return p.nsIPrompt_promptPassword(...promptArgs);
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
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
   * @returns {Boolean}
   *          true: Authentication can proceed using the values
   *          in the authInfo object.
   *          false: Authentication should be cancelled, usually because the
   *          user did not provide username/password.
   */
  promptAuth(domWin, channel, level, authInfo, checkLabel, checkValue) {
    let p = this.pickPrompter({ domWin });
    return p.promptAuth(channel, level, authInfo, checkLabel, checkValue);
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
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after this method returns.
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
   * Asynchronously prompt the user for a username and password.
   * This has largely the same semantics as promptUsernameAndPassword(),
   * but returns immediately after calling and returns the entered
   * data in a callback.
   *
   * @param {mozIDOMWindowProxy} domWin - The parent window or null.
   * @param {nsIChannel} channel - The channel that requires authentication.
   * @param {nsIAuthPromptCallback} callback - Called once the prompt has been
   *        closed.
   * @param {nsISupports} context
   * @param {Number} level - Security level of the credential transmission.
   *        Any of nsIAuthPrompt2.<LEVEL_NONE|LEVEL_PW_ENCRYPTED|LEVEL_SECURE>
   * @param {nsIAuthInformation} authInfo
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after the callback.
   * @returns {nsICancelable} Interface to cancel prompt.
   */
  asyncPromptAuth(
    domWin,
    channel,
    callback,
    context,
    level,
    authInfo,
    checkLabel,
    checkValue
  ) {
    let p = this.pickPrompter({ domWin });
    return p.asyncPromptAuth(
      channel,
      callback,
      context,
      level,
      authInfo,
      checkLabel,
      checkValue
    );
  },

  /**
   * Asynchronously prompt the user for a username and password.
   * This has largely the same semantics as promptUsernameAndPassword(),
   * but returns immediately after calling and returns the entered
   * data in a callback.
   *
   * @param {BrowsingContext} browsingContext - The browsing context the
   *        prompt should be opened for.
   * @param {Number} modalType - The modal type of the prompt.
   *        nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {nsIChannel} channel - The channel that requires authentication.
   * @param {nsIAuthPromptCallback} callback - Called once the prompt has been
   *        closed.
   * @param {nsISupports} context
   * @param {Number} level - Security level of the credential transmission.
   *        Any of nsIAuthPrompt2.<LEVEL_NONE|LEVEL_PW_ENCRYPTED|LEVEL_SECURE>
   * @param {nsIAuthInformation} authInfo
   * @param {String} checkLabel - Text to appear with the checkbox.
   *        If null, check box will not be shown.
   * @param {Object} checkValue - Contains the initial checked state of the
   *        checkbox when this method is called and the final checked state
   *        after the callback.
   * @returns {nsICancelable} Interface to cancel prompt.
   */
  asyncPromptAuthBC(browsingContext, modalType, ...promptArgs) {
    let p = this.pickPrompter({ browsingContext, modalType });
    return p.asyncPromptAuth(...promptArgs);
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

  // Copied from login manager
  getAuthTarget(aChannel, aAuthInfo) {
    let hostname, realm;

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
      hostname =
        "moz-proxy://" +
        idnService.convertUTF8toACE(info.host) +
        ":" +
        info.port;
      realm = aAuthInfo.realm;
      if (!realm) {
        realm = hostname;
      }

      return [hostname, realm];
    }

    hostname = this.getFormattedHostname(aChannel.URI);

    // If a HTTP WWW-Authenticate header specified a realm, that value
    // will be available here. If it wasn't set or wasn't HTTP, we'll use
    // the formatted hostname instead.
    realm = aAuthInfo.realm;
    if (!realm) {
      realm = hostname;
    }

    return [hostname, realm];
  },

  makeAuthMessage(channel, authInfo) {
    let isProxy = authInfo.flags & Ci.nsIAuthInformation.AUTH_PROXY;
    let isPassOnly = authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD;
    let isCrossOrig =
      authInfo.flags & Ci.nsIAuthInformation.CROSS_ORIGIN_SUB_RESOURCE;

    let username = authInfo.username;
    let [displayHost, realm] = this.getAuthTarget(channel, authInfo);

    // Suppress "the site says: $realm" when we synthesized a missing realm.
    if (!authInfo.realm && !isProxy) {
      realm = "";
    }

    // Trim obnoxiously long realms.
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
    this.browsingContext = browsingContext;
    this._domWin = domWin;

    if (this._domWin) {
      // We have a domWin, get the associated browsing context
      this.browsingContext = BrowsingContext.getFromWindow(this._domWin);
    } else if (this.browsingContext) {
      // We have a browsingContext, get the associated dom window
      if (this.browsingContext.window) {
        this._domWin = this.browsingContext.window;
      } else {
        this._domWin =
          this.browsingContext.embedderElement &&
          this.browsingContext.embedderElement.ownerGlobal;
      }
    }

    // Use given modal type or fallback to default
    this.modalType = modalType || ModalPrompter.defaultModalType;

    this.async = async;

    this.QueryInterface = ChromeUtils.generateQI([
      Ci.nsIPrompt,
      Ci.nsIAuthPrompt,
      Ci.nsIAuthPrompt2,
      Ci.nsIWritablePropertyBag2,
    ]);
  }

  set modalType(modalType) {
    // Setting modal type window is always allowed
    if (modalType == Ci.nsIPrompt.MODAL_TYPE_WINDOW) {
      this._modalType = modalType;
      return;
    }

    // If we have a chrome window and the browsing context isn't embedded
    // in a browser, we can't use tab/content prompts.
    // Or if we don't allow tab or content prompts, override modalType
    // argument to use window prompts
    if (
      !this.browsingContext ||
      !this._domWin ||
      (this._domWin.isChromeWindow &&
        !this.browsingContext.top.embedderElement) ||
      !ModalPrompter.tabModalEnabled
    ) {
      modalType = Ci.nsIPrompt.MODAL_TYPE_WINDOW;

      Cu.reportError(
        "Prompter: Browser not available or tab modal prompts disabled. Falling back to window prompt."
      );
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
    Services.tm.spinEventLoopUntilOrShutdown(() => closed);
  }

  async openPrompt(args) {
    if (!this.browsingContext) {
      // We don't have a browsing context, fallback to a window prompt.
      this.openWindowPrompt(null, args);
      return args;
    }

    // Select prompts are not part of CommonDialog
    // and thus not supported as tab or content prompts yet. See Bug 1622817.
    // Once they are integrated this override should be removed.
    if (
      args.promptType == "select" &&
      this.modalType !== Ci.nsIPrompt.MODAL_TYPE_WINDOW
    ) {
      Cu.reportError(
        "Prompter: 'select' prompts do not support tab/content prompting. Falling back to window prompt."
      );
      args.modalType = Ci.nsIPrompt.MODAL_TYPE_WINDOW;
    } else {
      args.modalType = this.modalType;
    }

    args.browsingContext = this.browsingContext;

    let actor;
    try {
      actor = this._domWin.windowGlobalChild.getActor("Prompt");
    } catch (error) {
      Cu.reportError(error);
      // We can't get the prompt actor, fallback to window prompt.
      this.openWindowPrompt(this._domWin, args);
      return args;
    }

    let docShell =
      (this.browsingContext && this.browsingContext.docShell) ||
      this._domWin.docShell;
    let inPermitUnload =
      docShell.contentViewer && docShell.contentViewer.inPermitUnload;
    let eventDetail = Cu.cloneInto(
      {
        tabPrompt: this.modalType != Ci.nsIPrompt.MODAL_TYPE_WINDOW,
        inPermitUnload,
      },
      this._domWin
    );
    PromptUtils.fireDialogEvent(
      this._domWin,
      "DOMWillOpenModalDialog",
      null,
      eventDetail
    );

    let windowUtils =
      Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT &&
      this._domWin.windowUtils;

    // Put content windows in the modal state while the prompt is open.
    if (windowUtils) {
      windowUtils.enterModalState();
    }

    // It is technically possible for multiple prompts to be sent from a single
    // BrowsingContext. See bug 1266353. We use a randomly generated UUID to
    // differentiate between the different prompts.
    let id =
      "id" +
      Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator)
        .generateUUID()
        .toString();

    args.promptPrincipal = this._domWin.document.nodePrincipal;
    args.inPermitUnload = inPermitUnload;
    args._remoteId = id;

    let returnedArgs;

    try {
      returnedArgs = await actor.sendQuery("Prompt:Open", args);
      if (returnedArgs && returnedArgs.promptAborted) {
        throw Components.Exception(
          "prompt aborted by user",
          Cr.NS_ERROR_NOT_AVAILABLE
        );
      }
    } finally {
      if (windowUtils) {
        windowUtils.leaveModalState();
      }
      PromptUtils.fireDialogEvent(this._domWin, "DOMModalDialogClosed");
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
    const COMMON_DIALOG = "chrome://global/content/commonDialog.xhtml";
    const SELECT_DIALOG = "chrome://global/content/selectDialog.xhtml";

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
    return objectToPropBag(taskResult);
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
      return this.nsIPrompt_promptUsernameAndPassword.apply(this, arguments);
    }
    return this.nsIAuthPrompt_promptUsernameAndPassword.apply(this, arguments);
  }

  promptPassword() {
    // Both have 5 args, so use types.
    if (typeof arguments[2] == "object") {
      return this.nsIPrompt_promptPassword.apply(this, arguments);
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
    checkValue
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

  nsIPrompt_promptUsernameAndPassword(
    title,
    text,
    user,
    pass,
    checkLabel,
    checkValue
  ) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptUsernameAndPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      promptType: "promptUserAndPass",
      title,
      text,
      user: this.async ? user : user.value,
      pass: this.async ? pass : pass.value,
      checkLabel,
      checked: this.async ? checkValue : checkValue.value,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        checked: result.checked,
        user: result.user,
        pass: result.pass,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
      user.value = args.user;
      pass.value = args.pass;
    }

    return ok;
  }

  nsIPrompt_promptPassword(title, text, pass, checkLabel, checkValue) {
    if (!title) {
      title = PromptUtils.getLocalizedString("PromptPassword3", [
        PromptUtils.getBrandFullName(),
      ]);
    }

    let args = {
      promptType: "promptPassword",
      title,
      text,
      pass: this.async ? pass : pass.value,
      checkLabel,
      checked: this.async ? checkValue : checkValue.value,
      ok: false,
    };

    if (this.async) {
      return this.openPromptAsync(args, result => ({
        checked: result.checked,
        pass: result.pass,
        ok: result.ok,
      }));
    }

    this.openPromptSync(args);

    // Did user click Ok or Cancel?
    let ok = args.ok;
    if (ok) {
      checkValue.value = args.checked;
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
      title,
      text,
      user,
      pass,
      null,
      {}
    );
  }

  nsIAuthPrompt_promptPassword(title, text, passwordRealm, savePassword, pass) {
    // The passwordRealm and savePassword args were ignored by nsPrompt.cpp
    return this.nsIPrompt_promptPassword(title, text, pass, null, {});
  }

  /* ----------  nsIAuthPrompt2  ---------- */

  promptAuth(channel, level, authInfo, checkLabel, checkValue) {
    let message = PromptUtils.makeAuthMessage(channel, authInfo);

    let [username, password] = PromptUtils.getAuthInfo(authInfo);

    let userParam = { value: username };
    let passParam = { value: password };

    let ok;
    if (authInfo.flags & Ci.nsIAuthInformation.ONLY_PASSWORD) {
      ok = this.nsIPrompt_promptPassword(
        null,
        message,
        passParam,
        checkLabel,
        checkValue
      );
    } else {
      ok = this.nsIPrompt_promptUsernameAndPassword(
        null,
        message,
        userParam,
        passParam,
        checkLabel,
        checkValue
      );
    }

    if (ok) {
      PromptUtils.setAuthInfo(authInfo, userParam.value, passParam.value);
    }
    return ok;
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
  Ci.nsIPrompt.MODAL_TYPE_WINDOW
);

XPCOMUtils.defineLazyPreferenceGetter(
  ModalPrompter,
  "tabModalEnabled",
  "prompts.tab_modal.enabled",
  true
);

function AuthPromptAdapterFactory() {}
AuthPromptAdapterFactory.prototype = {
  classID: Components.ID("{6e134924-6c3a-4d86-81ac-69432dd971dc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAuthPromptAdapterFactory]),

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
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAuthPrompt2]),
  oldPrompter: null,

  /* ----------  nsIAuthPrompt2 ---------- */

  promptAuth(channel, level, authInfo, checkLabel, checkValue) {
    let message = PromptUtils.makeAuthMessage(channel, authInfo);

    let [username, password] = PromptUtils.getAuthInfo(authInfo);
    let userParam = { value: username };
    let passParam = { value: password };

    let [host, realm] = PromptUtils.getAuthTarget(channel, authInfo);
    let authTarget = host + " (" + realm + ")";

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
