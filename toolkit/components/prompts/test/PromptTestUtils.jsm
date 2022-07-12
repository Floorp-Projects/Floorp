/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Utility module for tests to interact with prompts spawned by nsIPrompt or
 * nsIPromptService.
 */

"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

const EXPORTED_SYMBOLS = ["PromptTestUtils"];

const kPrefs = {};

// Whether prompts with modal type TAB are shown as SubDialog (true) or
// TabModalPrompt (false).
XPCOMUtils.defineLazyPreferenceGetter(
  kPrefs,
  "tabPromptSubDialogEnabled",
  "prompts.tabChromePromptSubDialog",
  false
);

// Whether web content prompts (alert etc.) are shown as SubDialog (true)
// or TabModalPrompt (false)
XPCOMUtils.defineLazyPreferenceGetter(
  kPrefs,
  "contentPromptSubDialogEnabled",
  "prompts.contentPromptSubDialog",
  false
);

function isCommonDialog(modalType) {
  return (
    modalType === Services.prompt.MODAL_TYPE_WINDOW ||
    (kPrefs.tabPromptSubDialogEnabled &&
      modalType === Services.prompt.MODAL_TYPE_TAB) ||
    (kPrefs.contentPromptSubDialogEnabled &&
      modalType === Services.prompt.MODAL_TYPE_CONTENT)
  );
}

let PromptTestUtils = {
  /**
   * Wait for a prompt from nsIPrompt or nsIPromptsService, interact with it and
   * click the specified button to close it.
   * @param {Browser|Window} [parent] - Parent of the prompt. This can be
   * either the parent window or the browser. For tab prompts, if given a
   * window, the currently selected browser in that window will be used.
   * @param {Object} promptOptions - @see waitForPrompt
   * @param {Object} promptActions - @see handlePrompt
   * @returns {Promise} - A promise which resolves once the prompt has been
   * closed.
   */
  async handleNextPrompt(parent, promptOptions, promptActions) {
    let dialog = await this.waitForPrompt(parent, promptOptions);
    return this.handlePrompt(dialog, promptActions);
  },

  /**
   * Interact with an existing prompt and close it.
   * @param {Dialog} dialog - The dialog instance associated with the prompt.
   * @param {Object} [actions] - Options on how to interact with the
   * prompt and how to close it.
   * @param {Boolean} [actions.checkboxState] - Set the checkbox state.
   * true = checked, false = unchecked.
   * @param {Number} [actions.buttonNumClick] - Which button to click to close
   * the prompt.
   * @param {String} [actions.loginInput] - Input text for the login text field.
   * This field is also used for text input for the "prompt" type.
   * @param {String} [actions.passwordInput] - Input text for the password text
   * field.
   * @returns {Promise} - A promise which resolves once the prompt has been
   * closed.
   */
  handlePrompt(
    dialog,
    {
      checkboxState = null,
      buttonNumClick = 0,
      loginInput = null,
      passwordInput = null,
    } = {}
  ) {
    let promptClosePromise;

    // Get parent window to listen for prompt close event
    let win;
    if (isCommonDialog(dialog.args.modalType)) {
      win = dialog.ui.prompt?.opener;
    } else {
      // Tab prompts should always have a parent window
      win = dialog.ui.prompt.win;
    }

    if (win) {
      promptClosePromise = BrowserTestUtils.waitForEvent(
        win,
        "DOMModalDialogClosed"
      );
    } else {
      // We don't have a parent, wait for window close instead
      promptClosePromise = BrowserTestUtils.windowClosed(dialog.ui.prompt);
    }

    if (typeof checkboxState == "boolean") {
      dialog.ui.checkbox.checked = checkboxState;
    }

    if (loginInput != null) {
      dialog.ui.loginTextbox.value = loginInput;
    }

    if (passwordInput != null) {
      dialog.ui.password1Textbox.value = passwordInput;
    }

    let button = dialog.ui["button" + buttonNumClick];
    if (!button) {
      throw new Error("Could not find button with index " + buttonNumClick);
    }
    button.click();

    return promptClosePromise;
  },

  /**
   * Wait for a prompt from nsIPrompt or nsIPromptsService to open.
   * @param {Browser|Window} [parent] - Parent of the prompt. This can be either
   * the parent window or the browser. For tab prompts, if given a window, the
   * currently selected browser in that window will be used.
   * If not given a parent, the method will return on prompts of any window.
   * @param {Object} attrs - The prompt attributes to filter for.
   * @param {Number} attrs.modalType - Whether the expected prompt is a content, tab or window prompt.
   * nsIPromptService.<MODAL_TYPE_WINDOW|MODAL_TYPE_TAB|MODAL_TYPE_CONTENT>
   * @param {String} [attrs.promptType] - Common dialog type of the prompt to filter for.
   *  @see {@link CommonDialog} for possible prompt types.
   * @returns {Promise<CommonDialog>} - A Promise which resolves with a dialog
   * object once the prompt has loaded.
   */
  async waitForPrompt(parent, { modalType, promptType = null } = {}) {
    if (!modalType) {
      throw new Error("modalType is mandatory");
    }

    // Get window by browser or browser by window, depending on what is passed
    // via the parent arg. If the caller passes parent=null, both will be null.
    let parentWindow;
    let parentBrowser;
    if (parent) {
      if (Element.isInstance(parent)) {
        // Parent is browser
        parentBrowser = parent;
        parentWindow = parentBrowser.ownerGlobal;
      } else if (parent instanceof Ci.nsIDOMChromeWindow) {
        // Parent is window
        parentWindow = parent;
        parentBrowser = parentWindow.gBrowser?.selectedBrowser;
      } else {
        throw new Error("Invalid parent. Expected browser or dom window");
      }
    }

    let topic = isCommonDialog(modalType)
      ? "common-dialog-loaded"
      : "tabmodal-dialog-loaded";

    let dialog;
    await TestUtils.topicObserved(topic, subject => {
      // If we are not given a browser, use the currently selected browser of the window
      let browser =
        parentBrowser || subject.ownerGlobal.gBrowser?.selectedBrowser;
      if (isCommonDialog(modalType)) {
        // Is not associated with given parent window, skip
        if (parentWindow && subject.opener !== parentWindow) {
          return false;
        }

        // For tab prompts, ensure that the associated browser matches.
        if (browser && modalType == Services.prompt.MODAL_TYPE_TAB) {
          let dialogBox = parentWindow.gBrowser.getTabDialogBox(browser);
          let hasMatchingDialog = dialogBox
            .getTabDialogManager()
            ._dialogs.some(
              d => d._frame?.browsingContext == subject.browsingContext
            );
          if (!hasMatchingDialog) {
            return false;
          }
        }

        if (browser && modalType == Services.prompt.MODAL_TYPE_CONTENT) {
          let dialogBox = parentWindow.gBrowser.getTabDialogBox(browser);
          let hasMatchingDialog = dialogBox
            .getContentDialogManager()
            ._dialogs.some(
              d => d._frame?.browsingContext == subject.browsingContext
            );
          if (!hasMatchingDialog) {
            return false;
          }
        }

        // subject is the window object of the prompt which has a Dialog object
        // attached.
        dialog = subject.Dialog;
      } else {
        // subject is the tabprompt dom node
        // Get the full prompt object which has the dialog object
        let prompt = browser.tabModalPromptBox.getPrompt(subject);

        // Is not associated with given parent browser, skip.
        if (!prompt) {
          return false;
        }

        dialog = prompt.Dialog;
      }

      // Not the modalType we're looking for.
      // For window prompts dialog.args.modalType is undefined.
      if (isCommonDialog(modalType) && dialog.args.modalType !== modalType) {
        return false;
      }

      // Not the promptType we're looking for.
      if (promptType && dialog.args.promptType !== promptType) {
        return false;
      }

      // Prompt found
      return true;
    });

    return dialog;
  },
};
