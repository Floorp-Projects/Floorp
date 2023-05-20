/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

class IMEStateOnInputTypeChangeTester {
  static #sInputElementList = [
    {
      type: "",
      textControl: true,
    },
    {
      type: "text",
      textControl: true,
    },
    {
      type: "text",
      readonly: true,
      textControl: true,
    },
    {
      type: "password",
      textControl: true,
    },
    {
      type: "password",
      readonly: true,
      textControl: true,
    },
    {
      type: "checkbox",
      buttonControl: true,
    },
    {
      type: "radio",
      buttonControl: true,
    },
    {
      type: "submit",
      buttonControl: true,
    },
    {
      type: "reset",
      buttonControl: true,
    },
    {
      type: "file",
      buttonControl: true,
    },
    {
      type: "button",
      buttonControl: true,
    },
    {
      type: "image",
      alt: "image",
      buttonControl: true,
    },
    {
      type: "url",
      textControl: true,
    },
    {
      type: "email",
      textControl: true,
    },
    {
      type: "search",
      textControl: true,
    },
    {
      type: "tel",
      textControl: true,
    },
    {
      type: "number",
      textControl: true,
    },
    {
      type: "date",
      dateTimeControl: true,
    },
    {
      type: "datetime-local",
      dateTimeControl: true,
    },
    {
      type: "time",
      dateTimeControl: true,
    },
    {
      type: "range",
      buttonControl: true,
    },
    {
      type: "color",
      buttonControl: true,
    },
    // TODO(bug 1283382, bug 1283382): month and week
  ];

  static get numberOfTests() {
    return IMEStateOnInputTypeChangeTester.#sInputElementList.length;
  }

  /**
   * @param {HTMLInputElement} aInputElement The input element.
   * @returns {number} Expected IME state when aInputElement has focus.
   */
  static #getExpectedIMEEnabledState(aInputElement) {
    if (aInputElement.readonly) {
      return SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED;
    }
    switch (aInputElement.type) {
      case "text":
      case "url":
      case "email":
      case "search":
      case "tel":
      case "number":
        return aInputElement.style.imeMode == "disabled"
          ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
          : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;

      case "password":
        return aInputElement.style.imeMode == "" ||
          aInputElement.style.imeMode == "auto" ||
          aInputElement.style.imeMode == "disabled"
          ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
          : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;

      default:
        return SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED;
    }
  }

  static #getDescription(aInputElement) {
    let desc = "<input";
    if (aInputElement.getAttribute("type")) {
      desc += ` type="${aInputElement.getAttribute("type")}"`;
    }
    if (aInputElement.readonly) {
      desc += " readonly";
    }
    if (aInputElement.getAttribute("style")) {
      desc += ` style="ime-mode: ${aInputElement.style.imeMode}"`;
    }
    return desc + ">";
  }

  // Only runner fields
  #mSrcTypeIndex;
  #mType;
  #mInputElement;
  #mNewType;
  #mWindow;

  // Only checker fields
  #mWindowUtils;
  #mTIPWrapper;

  /**
   * @param {number} aSrcTypeIndex `type` attribute value index in #sInputElementList
   */
  constructor(aSrcTypeIndex) {
    this.#mSrcTypeIndex = aSrcTypeIndex;
    this.#mType =
      IMEStateOnInputTypeChangeTester.#sInputElementList[this.#mSrcTypeIndex];
  }

  clear() {
    this.#mInputElement?.remove();
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
   * @param {number} aDestTypeIndex New type index.
   * @param {Window} aWindow The window running tests.
   * @param {Element} aContainer The element which should have new <input>.
   * @param {undefined|string} aIMEModeValue [optional] ime-mode value if you want to specify.
   * @returns {object|bool} Expected data before running tests.  If the test should not run, returns false.
   */
  async prepareToRun(aDestTypeIndex, aWindow, aContainer, aIMEModeValue) {
    this.#mWindow = aWindow;

    if (aContainer.ownerDocument.activeElement) {
      aContainer.ownerDocument.activeElement.blur();
      await this.#flushPendingIMENotifications();
    }
    if (this.#mInputElement?.isConnected) {
      this.#mInputElement.remove();
    }
    this.#mInputElement = null;

    this.#mNewType =
      IMEStateOnInputTypeChangeTester.#sInputElementList[aDestTypeIndex];
    if (
      aDestTypeIndex == this.#mSrcTypeIndex ||
      this.#mNewType.readonly ||
      (!this.#mType.textControl && !this.#mNewType.textControl)
    ) {
      return false;
    }
    this.#mInputElement = aContainer.ownerDocument.createElement("input");
    if (this.#mType.type != "") {
      this.#mInputElement.setAttribute("type", this.#mType.type);
    }
    if (this.#mType.readonly) {
      this.#mInputElement.setAttribute("readonly", "");
    }
    if (aIMEModeValue && aIMEModeValue !== "") {
      this.#mInputElement.setAttribute("style", `ime-mode: ${aIMEModeValue}`);
    }
    if (this.#mType.alt) {
      this.#mInputElement.setAttribute("alt", this.#mType.alt);
    }
    const waitForFocus = new Promise(resolve =>
      this.#mInputElement.addEventListener("focus", resolve, { once: true })
    );
    aContainer.appendChild(this.#mInputElement);
    this.#mInputElement.focus();
    await waitForFocus;

    await this.#flushPendingIMENotifications();

    const expectedIMEState =
      IMEStateOnInputTypeChangeTester.#getExpectedIMEEnabledState(
        this.#mInputElement
      );
    return {
      description: IMEStateOnInputTypeChangeTester.#getDescription(
        this.#mInputElement
      ),
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
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
    const description = `IMEStateOnInputTypeChangeTester(${aExpectedData.description})`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.expectedIMEState,
      `${description}: IME state should be set to expected value before running test`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedData.expectedIMEFocus,
      `${description}: IME state should ${
        aExpectedData.expectedIMEFocus ? "" : "not"
      }have focus before running test`
    );
  }

  /**
   * @returns {object} The expected results.
   */
  async run() {
    if (this.#mNewType.type == "") {
      this.#mInputElement.removeAttribute("type");
    } else {
      this.#mInputElement.setAttribute("type", this.#mNewType.type);
    }

    await this.#flushPendingIMENotifications();

    const expectedIMEState =
      IMEStateOnInputTypeChangeTester.#getExpectedIMEEnabledState(
        this.#mInputElement
      );
    return {
      newType: this.#mNewType.type,
      expectedIMEState,
      expectedIMEFocus:
        expectedIMEState !=
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
    };
  }

  checkResult(aExpectedDataBeforeRun, aExpectedData) {
    const description = `IMEStateOnInputTypeChangeTester(${
      aExpectedDataBeforeRun.description
    } -> type="${aExpectedData.newType || ""}")`;
    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.expectedIMEState,
      `${description}: IME state should be set to expected value`
    );
    is(
      this.#mTIPWrapper.IMEHasFocus,
      aExpectedData.expectedIMEFocus,
      `${description}: IME state should have focus`
    );
  }
}
