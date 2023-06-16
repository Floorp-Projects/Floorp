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

class IMEStateOfTextControlInContentEditableOnReadonlyChangeTester {
  static #sTextControls = [
    {
      tag: "input",
      type: "text",
      readonly: false,
    },
    {
      tag: "input",
      type: "text",
      readonly: true,
    },
    {
      tag: "textarea",
      readonly: false,
    },
    {
      tag: "textarea",
      readonly: true,
    },
  ];

  static get numberOfTextControlTypes() {
    return IMEStateOfTextControlInContentEditableOnReadonlyChangeTester
      .#sTextControls.length;
  }

  static #createElement(aDocument, aTextControl) {
    const textControl = aDocument.createElement(aTextControl.tag);
    if (aTextControl.type !== undefined) {
      textControl.setAttribute("type", aTextControl.type);
    }
    if (aTextControl.readonly) {
      textControl.setAttribute("readonly", "");
    }
    return textControl;
  }

  #getDescription() {
    return `<${this.#mTextControl.tag}${
      this.#mTextControl.type !== undefined
        ? ` type=${this.#mTextControl.type}`
        : ""
    }${this.#mTextControl.readonly ? " readonly" : ""}>`;
  }

  #getExpectedIMEState() {
    return this.#mTextControl.readonly
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED
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
  #mEditingHost;
  #mTextControl;
  #mTextControlElement;
  #mWindow;

  // Checker only fields.
  #mWindowUtils;
  #mTIPWrapper;

  clear() {
    this.#mTIPWrapper?.clearFocusBlurNotifications();
    this.#mTIPWrapper = null;
  }

  /**
   * @param {number} aIndex Index of the test.
   * @param {Element} aEditingHost The editing host which will have a text control.
   * @param {Window} aWindow [optional] The DOM window containing aEditingHost.
   * @returns {object} Expected result of initial state.
   */
  async prepareToRun(aIndex, aEditingHost, aWindow = window) {
    this.#mWindow = aWindow;
    this.#mEditingHost = aEditingHost;
    this.#mEditingHost.ownerDocument.activeElement?.blur();
    this.#mEditingHost.removeAttribute("contenteditable");
    this.#mTextControlElement?.remove();
    await this.#flushPendingIMENotifications();
    this.#mTextControl =
      IMEStateOfTextControlInContentEditableOnReadonlyChangeTester.#sTextControls[
        aIndex
      ];
    this.#mTextControlElement =
      IMEStateOfTextControlInContentEditableOnReadonlyChangeTester.#createElement(
        this.#mEditingHost.ownerDocument,
        this.#mTextControl
      );
    this.#mEditingHost.appendChild(this.#mTextControlElement);
    this.#mTextControlElement.focus();
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when ${this.#getDescription()} simply has focus`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  #checkResult(aExpectedResult) {
    const description =
      "IMEStateOfTextControlInContentEditableOnReadonlyChangeTester";
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
  }

  /**
   * @param {object} aExpectedResult The expected result returned by prepareToRun().
   * @param {Window} aWindow The window whose IME state should be checked.
   * @param {TIPWrapper} aTIPWrapper The TIP wrapper of aWindow.
   */
  checkResultOfPreparation(aExpectedResult, aWindow, aTIPWrapper) {
    this.#mWindowUtils = SpecialPowers.wrap(aWindow).windowUtils;
    this.#mTIPWrapper = aTIPWrapper;
    this.#checkResult(aExpectedResult);
  }

  async runToMakeParentEditingHost() {
    this.#mEditingHost.setAttribute("contenteditable", "");
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when parent of ${this.#getDescription()} becomes contenteditable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingParentEditingHost(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeHTMLEditorReadonly() {
    const editor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    editor.flags |= SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when HTMLEditor for parent of ${this.#getDescription()} becomes readonly`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingHTMLEditorReadonly(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeHTMLEditorEditable() {
    const editor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    editor.flags &= ~SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when HTMLEditor for parent of ${this.#getDescription()} becomes editable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingHTMLEditorEditable(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeParentNonEditingHost() {
    this.#mEditingHost.removeAttribute("contenteditable");
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when parent of ${this.#getDescription()} becomes non-editable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingParentNonEditable(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }
}

class IMEStateOutsideContentEditableOnReadonlyChangeTester {
  static #sFocusTargets = [
    {
      tag: "input",
      type: "text",
      readonly: false,
    },
    {
      tag: "input",
      type: "text",
      readonly: true,
    },
    {
      tag: "textarea",
      readonly: false,
    },
    {
      tag: "textarea",
      readonly: true,
    },
    {
      tag: "button",
    },
    {
      tag: "body",
    },
  ];

  static get numberOfFocusTargets() {
    return IMEStateOutsideContentEditableOnReadonlyChangeTester.#sFocusTargets
      .length;
  }

  static #maybeCreateElement(aDocument, aFocusTarget) {
    if (aFocusTarget.tag == "body") {
      return null;
    }
    const element = aDocument.createElement(aFocusTarget.tag);
    if (aFocusTarget.type !== undefined) {
      element.setAttribute("type", aFocusTarget.type);
    }
    if (aFocusTarget.readonly) {
      element.setAttribute("readonly", "");
    }
    return element;
  }

  #getDescription() {
    return `<${this.#mFocusTarget.tag}${
      this.#mFocusTarget.type !== undefined
        ? ` type=${this.#mFocusTarget.type}`
        : ""
    }${this.#mFocusTarget.readonly ? " readonly" : ""}>`;
  }

  #getExpectedIMEState() {
    return this.#mFocusTarget.readonly ||
      this.#mFocusTarget.tag == "button" ||
      this.#mFocusTarget.tag == "body"
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED
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
  #mBody;
  #mEditingHost;
  #mFocusTarget;
  #mFocusTargetElement;
  #mWindow;

  // Checker only fields.
  #mWindowUtils;
  #mTIPWrapper;

  clear() {
    this.#mTIPWrapper?.clearFocusBlurNotifications();
    this.#mTIPWrapper = null;
  }

  /**
   * @param {number} aIndex Index of the test.
   * @param {Element} aEditingHost The editing host.
   * @param {Window} aWindow [optional] The DOM window containing aEditingHost.
   * @returns {object} Expected result of initial state.
   */
  async prepareToRun(aIndex, aEditingHost, aWindow = window) {
    this.#mWindow = aWindow;
    this.#mEditingHost = aEditingHost;
    this.#mEditingHost.removeAttribute("contenteditable");
    this.#mBody = this.#mEditingHost.ownerDocument.body;
    this.#mBody.ownerDocument.activeElement?.blur();
    if (this.#mFocusTargetElement != this.#mBody) {
      this.#mFocusTargetElement?.remove();
    }
    await this.#flushPendingIMENotifications();
    this.#mFocusTarget =
      IMEStateOutsideContentEditableOnReadonlyChangeTester.#sFocusTargets[
        aIndex
      ];
    this.#mFocusTargetElement =
      IMEStateOutsideContentEditableOnReadonlyChangeTester.#maybeCreateElement(
        this.#mBody.ownerDocument,
        this.#mFocusTarget
      );
    if (this.#mFocusTargetElement) {
      this.#mBody.appendChild(this.#mFocusTargetElement);
      this.#mFocusTargetElement.focus();
    }
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when ${this.#getDescription()} simply has focus`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  #checkResult(aExpectedResult) {
    const description = "IMEStateOutsideContentEditableOnReadonlyChangeTester";
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedResult.expectedIMEState,
      `${description}: IME state should be proper one for the focused element ${aExpectedResult.description}`
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
   * @param {object} aExpectedResult The expected result returned by prepareToRun().
   * @param {Window} aWindow The window whose IME state should be checked.
   * @param {TIPWrapper} aTIPWrapper The TIP wrapper of aWindow.
   */
  checkResultOfPreparation(aExpectedResult, aWindow, aTIPWrapper) {
    this.#mWindowUtils = SpecialPowers.wrap(aWindow).windowUtils;
    this.#mTIPWrapper = aTIPWrapper;
    this.#checkResult(aExpectedResult);
  }

  async runToMakeParentEditingHost() {
    this.#mEditingHost.setAttribute("contenteditable", "");
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when parent of ${this.#getDescription()} becomes contenteditable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingParentEditingHost(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeHTMLEditorReadonly() {
    const editor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    editor.flags |= SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when HTMLEditor for parent of ${this.#getDescription()} becomes readonly`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingHTMLEditorReadonly(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeHTMLEditorEditable() {
    const editor = SpecialPowers.wrap(this.#mWindow).docShell.editor;
    editor.flags &= ~SpecialPowers.Ci.nsIEditor.eEditorReadonlyMask;
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when HTMLEditor for parent of ${this.#getDescription()} becomes editable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingHTMLEditorEditable(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }

  async runToMakeParentNonEditingHost() {
    this.#mEditingHost.removeAttribute("contenteditable");
    await this.#flushPendingIMENotifications();
    const expectedIMEState = this.#getExpectedIMEState();
    return {
      description: `when parent of ${this.#getDescription()} becomes non-editable`,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResultOfMakingParentNonEditable(aExpectedResult) {
    this.#checkResult(aExpectedResult);
  }
}
