/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

class IMEStateOnReadonlyChangeTester {
  static #sTextControlTypes = [
    {
      description: "<input>",
      createElement: aDoc => aDoc.createElement("input"),
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="text">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "text");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="password">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "password");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD,
    },
    {
      description: `<input type="url">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "url");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="email">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "email");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="search">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "search");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="tel">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "tel");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<input type="number">`,
      createElement: aDoc => {
        const input = aDoc.createElement("input");
        input.setAttribute("type", "number");
        return input;
      },
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
    {
      description: `<textarea></textarea>`,
      createElement: aDoc => aDoc.createElement("textarea"),
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
    },
  ];

  static get numberOfTextControlTypes() {
    return IMEStateOnReadonlyChangeTester.#sTextControlTypes.length;
  }

  // Only runner fields
  #mTextControl;
  #mTextControlElement;
  #mWindow;

  // Only checker fields
  #mWindowUtils;
  #mTIPWrapper;

  clear() {
    this.#mTextControlElement?.remove();
    this.#mTIPWrapper?.clearFocusBlurNotifications();
    this.#mTIPWrapper = null;
  }

  #flushPendingIMENotifications() {
    return new Promise(resolve =>
      this.#mWindow.requestAnimationFrame(() =>
        this.#mWindow.requestAnimationFrame(resolve)
      )
    );
  }

  /**
   * @param {number} aIndex The index of text control types.
   * @param {Window} aWindow The window running tests.
   * @param {Element} aContainer The element which should have new <input> or <textarea>.
   * @returns {object} Expected data before running tests.
   */
  async prepareToRun(aIndex, aWindow, aContainer) {
    this.#mWindow = aWindow;
    this.#mTextControl =
      IMEStateOnReadonlyChangeTester.#sTextControlTypes[aIndex];

    if (aContainer.ownerDocument.activeElement) {
      aContainer.ownerDocument.activeElement.blur();
      await this.#flushPendingIMENotifications();
    }
    if (this.#mTextControlElement?.isConnected) {
      this.#mTextControlElement.remove();
    }
    this.#mTextControlElement = this.#mTextControl.createElement(
      aContainer.ownerDocument
    );

    const waitForFocus = new Promise(resolve =>
      this.#mTextControlElement.addEventListener("focus", resolve, {
        once: true,
      })
    );
    aContainer.appendChild(this.#mTextControlElement);
    this.#mTextControlElement.focus();
    await waitForFocus;

    await this.#flushPendingIMENotifications();

    return {
      description: this.#mTextControl.description,
      expectedIMEState: this.#mTextControl.expectedIMEState,
      expectedIMEFocus: true,
    };
  }

  /**
   * @param {object} aExpectedData The expected data which was returned by prepareToRun().
   * @param {TIPWrapper} aTIPWrapper TIP wrapper in aWindow.
   * @param {Window} aWindow [optional] The window object of which you want to check IME state.
   */
  checkBeforeRun(aExpectedData, aTIPWrapper, aWindow = window) {
    this.#mWindowUtils = SpecialPowers.wrap(aWindow).windowUtils;
    this.#mTIPWrapper = aTIPWrapper;
    const description = `IMEStateOnReadonlyChangeTester(${aExpectedData.description})`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.expectedIMEState,
      `${description}: IME state should be set to expected value before setting readonly attribute`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedData.expectedIMEFocus,
      `${description}: IME state should ${
        aExpectedData.expectedIMEFocus ? "" : "not"
      } have focus before setting readonly attribute`
    );
  }

  /**
   * @returns {object} Expected result.
   */
  async runToMakeTextControlReadonly() {
    this.#mTextControlElement.setAttribute("readonly", "");

    await this.#flushPendingIMENotifications();

    return {
      description: this.#mTextControl.description,
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      expectedIMEFocus: false,
    };
  }

  /**
   * @param {object} aExpectedData Expected result which is returned by runToMakeTextControlReadonly().
   */
  checkResultOfMakingTextControlReadonly(aExpectedData) {
    const description = `IMEStateOnReadonlyChangeTester(${aExpectedData.description})`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.expectedIMEState,
      `${description}: IME state should be set to expected value after setting readonly attribute`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedData.expectedIMEFocus,
      `${description}: IME state should ${
        aExpectedData.expectedIMEFocus ? "" : "not"
      } have focus after setting readonly attribute`
    );
  }

  /**
   * @returns {object} Expected result.
   */
  async runToMakeTextControlEditable() {
    this.#mTextControlElement.removeAttribute("readonly");

    await this.#flushPendingIMENotifications();

    return {
      description: this.#mTextControl.description,
      expectedIMEState: this.#mTextControl.expectedIMEState,
      expectedIMEFocus: true,
    };
  }

  /**
   * @param {object} aExpectedData Expected result which is returned by runToMakeTextControlEditable().
   */
  checkResultOfMakingTextControlEditable(aExpectedData) {
    const description = `IMEStateOnReadonlyChangeTester(${aExpectedData.description})`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.expectedIMEState,
      `${description}: IME state should be set to expected value after removing readonly attribute`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedData.expectedIMEFocus,
      `${description}: IME state should ${
        aExpectedData.expectedIMEFocus ? "" : "not"
      } have focus after removing readonly attribute`
    );
  }
}
