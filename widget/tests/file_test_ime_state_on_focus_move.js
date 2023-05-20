/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

class IMEStateWhenNoActiveElementTester {
  #mDescription;

  constructor(aDescription) {
    this.#mDescription = aDescription;
  }

  async run(aDocument, aWindow = window) {
    aWindow.focus();
    aDocument.activeElement?.blur();

    await new Promise(resolve =>
      requestAnimationFrame(() => requestAnimationFrame(resolve))
    ); // wait for sending IME notifications

    return { designModeValue: aDocument.designMode };
  }

  check(aExpectedData, aWindow = window) {
    const winUtils = SpecialPowers.wrap(aWindow).windowUtils;
    if (aExpectedData.designModeValue == "on") {
      is(
        winUtils.IMEStatus,
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        `IMEStateWhenNoActiveElementTester(${
          this.#mDescription
        }): When no element has focus, IME should stay enabled in design mode`
      );
    } else {
      is(
        winUtils.IMEStatus,
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        `IMEStateWhenNoActiveElementTester(${
          this.#mDescription
        }): When no element has focus, IME should be disabled`
      );
    }
  }
}

class IMEStateOnFocusMoveTester {
  // Common fields
  #mDescription;
  #mTest;
  #mWindow;
  #mWindowUtils;

  // Only runner fields
  #mCreatedElement;
  #mCreatedElementForPreviousFocusedElement;
  #mElementToSetFocus;
  #mContainerIsEditable;

  // Only checker fields
  #mTIPWrapper;

  constructor(aDescription, aIndex, aWindow = window) {
    this.#mTest = IMEStateOnFocusMoveTester.#sTestList[aIndex];
    this.#mDescription = `IMEStateOnFocusMoveTester(${aDescription}): ${
      this.#mTest.description
    }`;
    this.#mWindow = aWindow;
    this.#mWindowUtils = SpecialPowers.wrap(this.#mWindow).windowUtils;
  }

  /**
   * prepareToRun should be called before run only in the process which will run the test.
   */
  async prepareToRun(aContainer) {
    const doc = aContainer.ownerDocument;
    this.#mTest = this.#resolveTest(this.#mTest, aContainer);
    this.#mContainerIsEditable = nodeIsEditable(aContainer);
    this.#mCreatedElement = this.#mTest.createElement(doc);
    const waitForLoadIfIFrame = new Promise(resolve => {
      if (this.#mCreatedElement.tagName == "IFRAME") {
        this.#mCreatedElement.addEventListener("load", resolve, {
          capture: true,
          once: true,
        });
      } else {
        resolve();
      }
    });
    aContainer.appendChild(this.#mCreatedElement);
    await waitForLoadIfIFrame;
    this.#mElementToSetFocus = this.#mCreatedElement.contentDocument
      ? this.#mCreatedElement.contentDocument.documentElement
      : this.#mCreatedElement;
    if (doc.designMode == "on") {
      doc.activeElement?.blur();
    } else if (this.#mContainerIsEditable) {
      getEditingHost(aContainer).focus(); // FIXME: use editing host instead
    } else {
      this.#mCreatedElementForPreviousFocusedElement =
        doc.createElement("input");
      this.#mCreatedElementForPreviousFocusedElement.setAttribute(
        "type",
        this.#mTest.expectedEnabledValue ==
          SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
          ? "password"
          : "text"
      );
      aContainer.appendChild(this.#mCreatedElementForPreviousFocusedElement);
      this.#mCreatedElementForPreviousFocusedElement.focus();
    }

    await new Promise(resolve =>
      requestAnimationFrame(() => requestAnimationFrame(resolve))
    ); // wait for sending IME notifications

    return {
      designModeValue: doc.designMode,
      containerIsEditable: this.#mContainerIsEditable,
      isFocusable: this.#mTest.isFocusable,
      focusEventFired: this.#mTest.focusEventIsExpected,
      enabledValue: this.#mTest.expectedEnabledValue,
      testedSubDocumentInDesignMode:
        this.#mCreatedElement.contentDocument?.designMode == "on",
    };
  }

  /**
   * prepareToCheck should be called before calling run only in the process which will check the result.
   */
  prepareToCheck(aExpectedData, aTIPWrapper) {
    info(`Starting ${this.#mDescription} (enable state check)...`);
    this.#mTIPWrapper = aTIPWrapper;
    this.#mTIPWrapper.onIMEFocusBlur = aNotificationType => {
      switch (aNotificationType) {
        case "notify-focus":
          info(aNotificationType);
          is(
            this.#mWindowUtils.IMEStatus,
            aExpectedData.enabledValue,
            `${
              this.#mDescription
            }, IME should receive a focus notification after IME state is updated`
          );
          break;
        case "notify-blur":
          info(aNotificationType);
          const changingStatus = !(
            aExpectedData.containerIsEditable &&
            aExpectedData.enabledValue ==
              SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
          );
          if (aExpectedData.designModeValue == "on") {
            is(
              // FIXME: This is odd, but #mWindowUtils.IMEStatus sometimes IME_STATUS_PASSWORD
              SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
              aExpectedData.enabledValue,
              `${
                this.#mDescription
              }, IME should receive a blur notification after IME state is updated`
            );
          } else if (changingStatus) {
            isnot(
              this.#mWindowUtils.IMEStatus,
              aExpectedData.enabledValue,
              `${
                this.#mDescription
              }, IME should receive a blur notification BEFORE IME state is updated`
            );
          } else {
            is(
              this.#mWindowUtils.IMEStatus,
              aExpectedData.enabledValue,
              `${
                this.#mDescription
              }, IME should receive a blur notification and its context has expected IME state if the state isn't being changed`
            );
          }
          break;
      }
    };

    this.#mTIPWrapper.clearFocusBlurNotifications();
  }

  /**
   * @returns {bool} whether expected element has focus or not after moving focus.
   */
  async run() {
    const previousFocusedElement = getFocusedElementOrUAWidgetHost();
    if (this.#mTest.setFocusIntoUAWidget) {
      this.#mTest.setFocusIntoUAWidget(this.#mElementToSetFocus);
    } else {
      this.#mElementToSetFocus.focus();
    }

    await new Promise(resolve =>
      requestAnimationFrame(() => requestAnimationFrame(resolve))
    ); // wait for sending IME notifications

    const currentFocusedElement = getFocusedElementOrUAWidgetHost();
    this.#mCreatedElementForPreviousFocusedElement?.remove();
    if (this.#mTest.isFocusable) {
      return this.#mElementToSetFocus == currentFocusedElement;
    }
    return previousFocusedElement == currentFocusedElement;
  }

  check(aExpectedData) {
    this.#mTIPWrapper.onIMEFocusBlur = null;

    if (aExpectedData.isFocusable) {
      if (aExpectedData.focusEventFired) {
        if (
          aExpectedData.enabledValue ==
            SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED ||
          aExpectedData.enabledValue ==
            SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
        ) {
          ok(
            this.#mTIPWrapper.numberOfFocusNotifications > 0,
            `${this.#mDescription}, IME should receive a focus notification`
          );
          if (
            aExpectedData.designModeValue == "on" &&
            !aExpectedData.testedSubDocumentInDesignMode
          ) {
            is(
              this.#mTIPWrapper.numberOfBlurNotifications,
              0,
              `${
                this.#mDescription
              }, IME shouldn't receive a blur notification in designMode since focus isn't moved from another editor`
            );
          } else {
            ok(
              this.#mTIPWrapper.numberOfBlurNotifications > 0,
              `${
                this.#mDescription
              }, IME should receive a blur notification for the previous focused editor`
            );
          }
          ok(
            this.#mTIPWrapper.IMEHasFocus,
            `${this.#mDescription}, IME should have focus right now`
          );
        } else {
          is(
            this.#mTIPWrapper.numberOfFocusNotifications,
            0,
            `${this.#mDescription}, IME shouldn't receive a focus notification`
          );
          ok(
            this.#mTIPWrapper.numberOfBlurNotifications > 0,
            `${this.#mDescription}, IME should receive a blur notification`
          );
          ok(
            !this.#mTIPWrapper.IMEHasFocus,
            `${this.#mDescription}, IME shouldn't have focus right now`
          );
        }
      } else {
        ok(true, `${this.#mDescription}, focus event should be fired`);
      }
    } else {
      is(
        this.#mTIPWrapper.numberOfFocusNotifications,
        0,
        `${
          this.#mDescription
        }, IME shouldn't receive a focus notification at testing non-focusable element`
      );
      is(
        this.#mTIPWrapper.numberOfBlurNotifications,
        0,
        `${
          this.#mDescription
        }, IME shouldn't receive a blur notification at testing non-focusable element`
      );
    }

    is(
      this.#mWindowUtils.IMEStatus,
      aExpectedData.enabledValue,
      `${this.#mDescription}, wrong enabled state`
    );
    if (
      this.#mTest.expectedInputElementType &&
      aExpectedData.designModeValue != "on"
    ) {
      is(
        this.#mWindowUtils.focusedInputType,
        this.#mTest.expectedInputElementType,
        `${this.#mDescription}, wrong input type`
      );
    } else if (aExpectedData.designModeValue == "on") {
      is(
        this.#mWindowUtils.focusedInputType,
        "",
        `${this.#mDescription}, wrong input type`
      );
    }
  }

  destroy() {
    this.#mCreatedElement?.remove();
    this.#mCreatedElementForPreviousFocusedElement?.remove();
    this.#mTIPWrapper?.clearFocusBlurNotifications();
    this.#mTIPWrapper = null;
  }

  /**
   * Open/Close state test check
   * Note that these tests are not run now.
   * If these tests should run between `run` and `cleanUp` call of the above
   * tests.
   */
  canTestOpenCloseState(aExpectedData) {
    return (
      IsIMEOpenStateSupported() &&
      this.#mWindowUtils.IMEStatus ==
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED &&
      aExpectedData.enabledValue ==
        SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
    );
  }
  async prepareToRunOpenCloseTest(aContainer) {
    const doc = aContainer.ownerDocument;
    this.#mCreatedElementForPreviousFocusedElement?.remove();
    this.#mCreatedElementForPreviousFocusedElement = doc.createElement("input");
    this.#mCreatedElementForPreviousFocusedElement.setAttribute("type", "text");
    aContainer.appendChild(this.#mCreatedElementForPreviousFocusedElement);

    this.#mContainerIsEditable = nodeIsEditable(aContainer);
    this.#mCreatedElement = this.#mTest.createElement(doc);
    const waitForLoadIfIFrame = new Promise(resolve => {
      if (this.#mCreatedElement.tagName == "IFRAME") {
        this.#mCreatedElement.addEventListener("load", resolve, {
          capture: true,
          once: true,
        });
      } else {
        resolve();
      }
    });
    aContainer.appendChild(this.#mCreatedElement);
    await waitForLoadIfIFrame;
    this.#mElementToSetFocus = this.#mCreatedElement.contentDocument
      ? this.#mCreatedElement.contentDocument.documentElement
      : this.#mCreatedElement;

    this.#mCreatedElementForPreviousFocusedElement.focus();

    return {};
  }
  prepareToCheckOpenCloseTest(aPreviousOpenState, aExpectedData) {
    info(`Starting ${this.#mDescription} (open/close state check)...`);
    this.#mWindowUtils.IMEIsOpen = aPreviousOpenState;
    aExpectedData.defaultOpenState = this.#mWindowUtils.IMEIsOpen;
  }
  async runOpenCloseTest() {
    return this.run();
  }
  checkOpenCloseTest(aExpectedData) {
    const expectedOpenState =
      this.#mTest.expectedOpenState != undefined
        ? this.#mTest.expectedOpenState
        : aExpectedData.defaultOpenState;
    is(
      this.#mWindowUtils.IMEIsOpen,
      expectedOpenState,
      `${this.#mDescription}, IME should ${
        expectedOpenState != aExpectedData.defaultOpenState ? "become" : "keep"
      } ${expectedOpenState ? "open" : "closed"}`
    );
  }

  /**
   * Utility methods for defining kIMEStateTestList.
   */

  /**
   * @param {Element} aElement
   */
  static #elementIsConnectedAndNotInDesignMode(aElement) {
    return aElement.isConnected && !nodeIsInDesignMode(aElement);
  }

  /**
   * @param {Element} aElementContainer
   * @param {bool} aElementIsEditingHost
   */
  static #elementIsFocusableIfEditingHost(
    aElementContainer,
    aElementIsEditingHost
  ) {
    return !nodeIsEditable(aElementContainer) && aElementIsEditingHost;
  }

  static #IMEStateEnabledAlways() {
    return SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   * @param {bool} aElementIsEditingHost
   */
  static #IMEStateEnabledIfEditable(aElement, aElementIsEditingHost) {
    return nodeIsEditable(aElement) || aElementIsEditingHost
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
      : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED;
  }

  /**
   * @param {Element} aElement
   */
  static #IMEStateEnabledIfInDesignMode(aElement) {
    return IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode(
      aElement
    )
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED
      : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   */
  static #IMEStatePasswordIfNotInDesignMode(aElement) {
    return IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode(
      aElement
    )
      ? SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
      : SpecialPowers.Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   */
  static #elementIsConnectedAndNotEditable(aElement) {
    return aElement.isConnected && !nodeIsEditable(aElement);
  }

  /**
   * @param {Element} aElementContainer
   */
  static #focusEventIsExpectedUnlessEditableChild(aElementContainer) {
    return !nodeIsEditable(aElementContainer);
  }

  // Form controls except text editable elements are "disable" in normal
  // condition, however, if they are editable, they are "enabled".
  // XXX Probably there are some bugs: If the form controls editable, they
  //     shouldn't be focusable.
  #resolveTest(aTest, aContainer) {
    const isFocusable = aTest.isFocusable(
      aContainer,
      aTest.isNewElementEditingHost
    );
    return {
      // Description of the new element
      description: aTest.description,
      // Create element to check IME state
      createElement: aTest.createElement,
      // Whether the new element is an editing host if container is not editable
      isNewElementEditingHost: aTest.isNewElementEditingHost,
      // If the test wants to move focus into an element in UA widget, define
      // this and set focus in it.
      setFocusIntoUAWidget: aTest.setFocusIntoUAWidget,
      // Whether the element is focusable or not
      isFocusable,
      // Whether focus events are fired on the element if it's focusable
      focusEventIsExpected:
        isFocusable &&
        aTest.focusEventIsExpected(aContainer, aTest.isNewElementEditingHost),
      // Expected IME enabled state when the element has focus
      expectedEnabledValue: aTest.expectedEnabledValue(
        aContainer,
        aTest.isNewElementEditingHost
      ),
      // Expected IME open state when the element gets focus
      // "undefined" means that IME open state should not be changed
      expectedOpenState: aTest.expectedOpenState,
      // Expected type of input element if it's an <input>
      expectedInputElementType: aTest.expectedInputElementType,
    };
  }
  static #sTestList = [
    {
      description: "input[type=text]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "text",
    },
    {
      description: "input[type=text][readonly]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("readonly", "");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "input[type=password]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
      expectedInputElementType: "password",
    },
    {
      description: "input[type=password][readonly]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("readonly", "");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "input[type=checkbox]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "checkbox");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=radio]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "radio");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=submit]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "submit");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=reset]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "reset");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=file]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "file");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=button]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "button");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=image]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "image");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=url]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "url",
    },
    {
      description: "input[type=email]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "email",
    },
    {
      description: "input[type=search]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "search",
    },
    {
      description: "input[type=tel]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "tel",
    },
    {
      description: "input[type=number]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedInputElementType: "number",
    },
    {
      description: "input[type=date]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "date");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "date",
    },
    {
      description: "input[type=datetime-local]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "datetime-local");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "datetime-local",
    },
    {
      description: "input[type=time]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "time");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "time",
    },
    // TODO(bug 1283382, bug 1283382): month and week

    // form controls
    {
      description: "button",
      createElement: aDocument => aDocument.createElement("button"),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "textarea",
      createElement: aDocument => aDocument.createElement("textarea"),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: "textarea[readonly]",
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("readonly", "");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "select (dropdown list)",
      createElement: aDocument => {
        const select = aDocument.createElement("select");
        const option1 = aDocument.createElement("option");
        option1.textContent = "abc";
        const option2 = aDocument.createElement("option");
        option2.textContent = "def";
        const option3 = aDocument.createElement("option");
        option3.textContent = "ghi";
        select.appendChild(option1);
        select.appendChild(option2);
        select.appendChild(option3);
        return select;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "select (list box)",
      createElement: aDocument => {
        const select = aDocument.createElement("select");
        select.setAttribute("multiple", "multiple");
        const option1 = aDocument.createElement("option");
        option1.textContent = "abc";
        const option2 = aDocument.createElement("option");
        option2.textContent = "def";
        const option3 = aDocument.createElement("option");
        option3.textContent = "ghi";
        select.appendChild(option1);
        select.appendChild(option2);
        select.appendChild(option3);
        return select;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },

    // a element
    {
      id: "a_href",
      description: "a[href]",
      createElement: aDocument => {
        const element = aDocument.createElement("a");
        element.setAttribute("href", "about:blank");
        return element;
      },
      isFocusable: IMEStateOnFocusMoveTester.#elementIsConnectedAndNotEditable,
      focusEventIsExpected:
        IMEStateOnFocusMoveTester.#focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },

    // audio element
    {
      description: "audio[controls]",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "playButton in audio",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("playButton")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "scrubber in audio",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("scrubber")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "muteButton in audio",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("muteButton")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "volumeControl in audio",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("volumeControl")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },

    // video element
    {
      description: "video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfEditable,
    },
    {
      description: "playButton in video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("playButton")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "scrubber in video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("scrubber")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "muteButton in video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("muteButton")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },
    {
      description: "volumeControl in video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      setFocusIntoUAWidget: aElement =>
        SpecialPowers.wrap(aElement)
          .openOrClosedShadowRoot.getElementById("volumeControl")
          .focus(),
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStateEnabledIfInDesignMode,
    },

    // ime-mode
    {
      description: 'input[type=text][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=text][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=text][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'input[type=text][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'input[type=text][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=url][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=url][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=url][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: true,
    },
    {
      description: 'input[type=url][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: false,
    },
    {
      description: 'input[type=url][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=email][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=email][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=email][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: true,
    },
    {
      description: 'input[type=email][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: false,
    },
    {
      description: 'input[type=email][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=search][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=search][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=search][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: true,
    },
    {
      description: 'input[type=search][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedOpenState: false,
    },
    {
      description: 'input[type=search][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=tel][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=tel][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=tel][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'input[type=tel][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'input[type=tel][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=number][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=number][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=number][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'input[type=number][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'input[type=number][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=password][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },
    {
      description: 'input[type=password][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'input[type=password][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'input[type=password][style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'input[type=password][style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },
    {
      description: 'textarea[style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'textarea[style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: 'textarea[style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'textarea[style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'textarea[style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable:
        IMEStateOnFocusMoveTester.#elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue:
        IMEStateOnFocusMoveTester.#IMEStatePasswordIfNotInDesignMode,
    },

    // HTML editors
    {
      description: 'div[contenteditable="true"]',
      createElement: aDocument => {
        const div = aDocument.createElement("div");
        div.setAttribute("contenteditable", "");
        return div;
      },
      isNewElementEditingHost: true,
      isFocusable: IMEStateOnFocusMoveTester.#elementIsFocusableIfEditingHost,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
    {
      description: "designMode editor",
      createElement: aDocument => {
        const iframe = aDocument.createElement("iframe");
        iframe.srcdoc = "<!doctype html><html><body></body></html>";
        iframe.addEventListener(
          "load",
          () => (iframe.contentDocument.designMode = "on"),
          { capture: true, once: true }
        );
        return iframe;
      },
      isFocusable: () => true,
      focusEventIsExpected: () => true,
      expectedEnabledValue: IMEStateOnFocusMoveTester.#IMEStateEnabledAlways,
    },
  ];

  static get numberOfTests() {
    return IMEStateOnFocusMoveTester.#sTestList.length;
  }
}
