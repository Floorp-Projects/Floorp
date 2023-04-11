/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

class IMEStateInContentEditableOnReadonlyChangeTester {
  // Runner only fields.
  #mEditingHost;
  #mFocusElement;
  #mWindow;

  // Tester only fields.
  #mTIPWrapper;
  #mWindowUtils;

  clear() {
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

  #getExpectedIMEState() {
    // Although if this.#mFocusElement is a <button>, its `.focus()` call
    // focus it, but caret is not set into it and following typing is handled
    // outside the <button>.  Therefore, anyway the enabled state should be
    // "enabled".
    return SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aEditingHost  The editing host.
   * @param {Element} aFocusElement Element which should have focus.  This must
   * be an inclusive descendant of the editing host and editable element.
   * @param {Window} aWindow [optional] The window.
   * @returns {object} Expected result of initial state.
   */
  async prepareToRun(aEditingHost, aFocusElement, aWindow = window) {
    this.#mWindow = aWindow;
    this.#mEditingHost = aEditingHost;
    this.#mFocusElement = aFocusElement;

    if (this.#mEditingHost.ownerDocument.activeElement) {
      this.#mEditingHost.ownerDocument.activeElement.blur();
      await this.#flushPendingIMENotifications();
    }

    this.#mWindow.focus();
    this.#mEditingHost.setAttribute("contenteditable", "");
    this.#mFocusElement.focus();

    await this.#flushPendingIMENotifications();

    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when initialized with setting focus to ${
        this.#mFocusElement == this.#mEditingHost
          ? "the editing host"
          : `<${this.#mFocusElement.tagName.toLowerCase()}>`
      }`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  /**
   * @param {object} aExpectedResult The expected result of the test.
   */
  #checkResult(aExpectedResult) {
    const description = `IMEStateInContentEditableOnReadonlyChangeTester`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedResult.expectedIMEState,
      `${description}: IME enabled state should be expected one ${aExpectedResult.description}`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedResult.expectedIMEFocus,
      `${description}: IME should ${
        aExpectedResult.expectedIMEFocus ? "" : "not "
      }have focus ${aExpectedResult.description}`
    );
  }

  /**
   * @param {object} aExpectedResult The expected result of prepareToRun().
   * @param {Window} aWindow The window to check IME state.
   * @param {TIPWrapper} aTIPWrapper The TIPWrapper for aWindow.
   */
  checkResultOfPreparation(aExpectedResult, aWindow, aTIPWrapper) {
    this.#mWindowUtils = SpecialPowers.wrap(aWindow).windowUtils;
    this.#mTIPWrapper = aTIPWrapper;
    this.#checkResult(aExpectedResult);
  }

  /**
   * @returns {object} The expected result.
   */
  async runToMakeHTMLEditorReadonly() {
    const htmlEditor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    htmlEditor.flags |= SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;

    await this.#flushPendingIMENotifications();

    return {
      description:
        this.#mFocusElement == this.#mEditingHost
          ? "when the editing host has focus"
          : `when <${this.#mFocusElement.tagName.toLowerCase()}> has focus`,
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      expectedIMEFocus: false,
    };
  }

  /**
   * @param {object} aExpectedResult The expected result of runToMakeHTMLEditorReadonly().
   */
  checkResultOfMakingHTMLEditorReadonly(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  /**
   * @returns {object} The expected result.
   */
  async runToMakeHTMLEditorEditable() {
    const htmlEditor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    htmlEditor.flags &= ~SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;

    await this.#flushPendingIMENotifications();

    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description:
        this.#mFocusElement == this.#mEditingHost
          ? "when the editing host has focus"
          : `when <${this.#mFocusElement.tagName.toLowerCase()}> has focus`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  /**
   * @param {object} aExpectedResult The expected result of runToMakeHTMLEditorEditable().
   */
  checkResultOfMakingHTMLEditorEditable(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToRemoveContentEditableAttribute() {
    this.#mEditingHost.removeAttribute("contenteditable");

    await this.#flushPendingIMENotifications();

    return {
      description:
        this.#mFocusElement == this.#mEditingHost
          ? "after removing contenteditable attribute when the editing host has focus"
          : `after removing contenteditable attribute when <${this.#mFocusElement.tagName.toLowerCase()}> has focus`,
      expectedIMEState: SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      expectedIMEFocus: false,
    };
  }

  /**
   * @param {object} aExpectedResult The expected result of runToRemoveContentEditableAttribute().
   */
  checkResultOfRemovingContentEditableAttribute(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }
}
