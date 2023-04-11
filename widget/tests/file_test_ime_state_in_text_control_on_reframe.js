/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

// Bug 580388 and bug 808287
class IMEStateInTextControlOnReframeTester {
  static #sTextControls = [
    {
      tag: "input",
      type: "text",
    },
    {
      tag: "input",
      type: "password",
    },
    {
      tag: "textarea",
    },
  ];

  static get numberOfTextControlTypes() {
    return IMEStateInTextControlOnReframeTester.#sTextControls.length;
  }

  #createElement() {
    const textControl = this.#mDocument.createElement(this.#mTextControl.tag);
    if (this.#mTextControl.type !== undefined) {
      textControl.setAttribute("type", this.#mTextControl.type);
    }
    return textControl;
  }

  #getDescription() {
    return `<${this.#mTextControl.tag}${
      this.#mTextControl.type !== undefined
        ? ` type=${this.#mTextControl.type}`
        : ""
    }>`;
  }

  #getExpectedIMEState() {
    return this.#mTextControl.type == "password"
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
      : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  #flushPendingIMENotifications() {
    return new Promise(resolve =>
      this.#mWindow.requestAnimationFrame(() =>
        this.#mWindow.requestAnimationFrame(resolve)
      )
    );
  }

  // Runner only fields.
  #mTextControl;
  #mTextControlElement;
  #mWindow;
  #mDocument;

  // Checker only fields.
  #mWindowUtils;
  #mTIPWrapper;

  clear() {
    this.#mTIPWrapper?.clearFocusBlurNotifications();
    this.#mTIPWrapper = null;
  }

  /**
   * @param {number} aIndex Index of the test.
   * @param {Element} aDocument The document to run the test.
   * @param {Window} aWindow [optional] The DOM window for aDocument.
   * @returns {object} Expected result of initial state.
   */
  async prepareToRun(aIndex, aDocument, aWindow = window) {
    this.#mWindow = aWindow;
    this.#mDocument = aDocument;
    this.#mDocument.activeElement?.blur();
    this.#mTextControlElement?.remove();
    await this.#flushPendingIMENotifications();
    this.#mTextControl =
      IMEStateInTextControlOnReframeTester.#sTextControls[aIndex];
    this.#mTextControlElement = this.#createElement();
    this.#mDocument.body.appendChild(this.#mTextControlElement);
    this.#mTextControlElement.focus();
    this.#mTextControlElement.style.overflow = "visible";
    this.#mTextControlElement.addEventListener(
      "input",
      aEvent => {
        aEvent.target.style.overflow = "hidden";
      },
      {
        capture: true,
      }
    );
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when ${this.#getDescription()} has focus`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      expectedNumberOfFocusNotifications: 1,
    };
  }

  #checkResult(aExpectedResult) {
    const description = "IMEStateInTextControlOnReframeTester";
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedResult.expectedIMEState,
      `${description}: IME state should be proper one for the text control ${aExpectedResult.description}`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedResult.expectedIMEFocus,
      `${description}: IME should ${
        aExpectedResult.expectedIMEFocus ? "" : "not "
      }have focus ${aExpectedResult.description}`
    );
    if (aExpectedResult.numberOfFocusNotifications !== undefined) {
      is(
        this.#mTIPWrapper.numberOfFocusNotifications,
        aExpectedResult.numberOfFocusNotifications,
        `${description}: focus notifications should've been received ${
          this.#mTIPWrapper.numberOfFocusNotifications
        } times ${aExpectedResult.description}`
      );
    }
    if (aExpectedResult.numberOfBlurNotifications !== undefined) {
      is(
        this.#mTIPWrapper.numberOfBlurNotifications,
        aExpectedResult.numberOfBlurNotifications,
        `${description}: blur notifications should've been received ${
          this.#mTIPWrapper.numberOfBlurNotifications
        } times ${aExpectedResult.description}`
      );
    }
  }

  /**
   * @param {object} aExpectedResult The expected result returned by prepareToRun().
   * @param {Window} aWindow The window whose IME state should be checked.
   * @param {TIPWrapper} aTIPWrapper The TIP wrapper of aWindow.
   */
  checkResultAfterTypingA(aExpectedResult, aWindow, aTIPWrapper) {
    this.#mWindowUtils = SpecialPowers.wrap(aWindow).windowUtils;
    this.#mTIPWrapper = aTIPWrapper;
    this.#checkResult(aExpectedResult);

    this.#mTIPWrapper.clearFocusBlurNotifications();
  }

  async prepareToRun2() {
    this.#mTextControlElement.addEventListener("focus", aEvent => {
      // Perform a style change and flush it to trigger reframing.
      aEvent.target.style.overflow = "visible";
      aEvent.target.getBoundingClientRect();
    });
    this.#mTextControlElement.blur();
    this.#mTextControlElement.focus();

    await this.#flushPendingIMENotifications();

    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when ${this.#getDescription()} is reframed by focus event listener`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      expectedNumberOfFocusNotifications: 1,
      expectedNumberOfBlurNotifications: 1,
    };
  }

  /**
   * @param {object} aExpectedResult The expected result returned by prepareToRun().
   */
  checkResultAfterTypingA2(aExpectedResult) {
    this.#checkResult(aExpectedResult);

    this.#mTIPWrapper.clearFocusBlurNotifications();
  }
}
