/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from file_ime_state_test_helper.js */

async function runIMEStateOnFocusMoveTests(
  aGlobalDescription,
  aContainer,
  aTIPWrapper,
  aWindow = window
) {
  const doc = aContainer.ownerDocument;
  const containerIsEditable = nodeIsEditable(aContainer);
  const winUtils = SpecialPowers.wrap(aWindow).windowUtils;

  async function runTest(aTest) {
    const description = `runIMEStateOnFocusMoveTests(${aGlobalDescription}): ${aTest.description}`;
    info(`Start testing ${description}`);
    const element = aTest.createElement(doc);
    const waitForLoadIfIFrame = new Promise(resolve => {
      if (element.tagName == "IFRAME") {
        element.addEventListener("load", resolve, {
          capture: true,
          once: true,
        });
      } else {
        resolve();
      }
    });
    try {
      aContainer.appendChild(element);
      await waitForLoadIfIFrame;

      function moveFocus(
        aFocusEventListener,
        aIMEFocusBlurNotificationListener
      ) {
        const elementToSetFocus = element.contentDocument
          ? element.contentDocument.documentElement
          : element;
        let createdElementForPreviousFocusedElement;
        if (doc.designMode == "on") {
          doc.activeElement?.blur();
        } else if (containerIsEditable) {
          aContainer.focus(); // FIXME: use editing host instead
        } else {
          createdElementForPreviousFocusedElement = doc.createElement("input");
          createdElementForPreviousFocusedElement.setAttribute(
            "type",
            aTest.expectedEnabledValue ==
              Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
              ? "password"
              : "text"
          );
          aContainer.appendChild(createdElementForPreviousFocusedElement);
          createdElementForPreviousFocusedElement.focus();
        }
        const focusEventTarget = element.contentDocument
          ? element.contentDocument
          : element;
        focusEventTarget.addEventListener("focus", aFocusEventListener, {
          capture: true,
        });
        aTIPWrapper.onIMEFocusBlur = aIMEFocusBlurNotificationListener;
        aTIPWrapper.clearFocusBlurNotifications();

        const previousFocusedElement = getFocusedElementOrUAWidgetHost();
        if (aTest.setFocusIntoUAWidget) {
          aTest.setFocusIntoUAWidget(elementToSetFocus);
        } else {
          elementToSetFocus.focus();
        }

        focusEventTarget.removeEventListener("focus", aFocusEventListener, {
          capture: true,
        });
        aTIPWrapper.onIMEFocusBlur = null;

        const currentFocusedElement = getFocusedElementOrUAWidgetHost();
        createdElementForPreviousFocusedElement?.remove();
        if (aTest.isFocusable) {
          is(
            currentFocusedElement,
            elementToSetFocus,
            `${description}, the element should've been focused`
          );
          return elementToSetFocus == currentFocusedElement;
        }
        is(
          currentFocusedElement,
          previousFocusedElement,
          `${description}, the element should not get focus because of non-focusable`
        );
        return previousFocusedElement == currentFocusedElement;
      }

      function testOpened(aOpened) {
        doc.getElementById("text").focus();
        winUtils.IMEIsOpen = aOpened;
        if (!moveFocus()) {
          return;
        }
        const expectedOpenState =
          aTest.expectedOpenState !== undefined
            ? aTest.expectedOpenState
            : aOpened;
        is(
          winUtils.IMEIsOpen,
          expectedOpenState,
          `${description}, IME should ${
            aTest.expectedOpenState ? "become" : "keep"
          } ${expectedOpenState ? "open" : "closed"}`
        );
      }

      // IME Enabled state testing
      let focusEventCount = 0;
      function focusEventListener(aEvent) {
        focusEventCount++;
        is(
          winUtils.IMEStatus,
          aTest.expectedEnabledValue,
          `${description}, wrong enabled state at focus event`
        );
      }
      function focusBlurNotificationListener(aNotificationType) {
        switch (aNotificationType) {
          case "notify-focus":
            is(
              winUtils.IMEStatus,
              aTest.expectedEnabledValue,
              `${description}, IME should receive a focus notification after IME state is updated`
            );
            break;
          case "notify-blur":
            const changingStatus = !(
              containerIsEditable &&
              aTest.expectedEnabledValue ==
                Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
            );
            if (element.contentDocument?.designMode == "on") {
              is(
                Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED, // FIXME: This is odd, but winUtils.IMEStatus sometimes IME_STATUS_PASSWORD
                aTest.expectedEnabledValue,
                `${description}, IME should receive a blur notification after IME state is updated`
              );
            } else if (changingStatus) {
              isnot(
                winUtils.IMEStatus,
                aTest.expectedEnabledValue,
                `${description}, IME should receive a blur notification BEFORE IME state is updated`
              );
            } else {
              is(
                winUtils.IMEStatus,
                aTest.expectedEnabledValue,
                `${description}, IME should receive a blur notification and its context has expected IME state if the state isn't being changed`
              );
            }
            break;
        }
      }

      if (!moveFocus(focusEventListener, focusBlurNotificationListener)) {
        return;
      }

      if (aTest.isFocusable) {
        if (aTest.focusEventIsExpected) {
          ok(
            focusEventCount > 0,
            `${description}, focus event should be fired`
          );
          if (
            aTest.expectedEnabledValue ==
              Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED ||
            aTest.expectedEnabledValue ==
              Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
          ) {
            ok(
              aTIPWrapper.numberOfFocusNotifications > 0,
              `${description}, IME should receive a focus notification`
            );
            if (
              doc.designMode == "on" &&
              element.contentDocument?.designMode != "on"
            ) {
              is(
                aTIPWrapper.numberOfBlurNotifications,
                0,
                `${description}, IME shouldn't receive a blur notification in designMode since focus isn't moved from another editor`
              );
            } else {
              ok(
                aTIPWrapper.numberOfBlurNotifications > 0,
                `${description}, IME should receive a blur notification for the previous focused editor`
              );
            }
            ok(
              aTIPWrapper.IMEHasFocus,
              `${description}, IME should have focus right now`
            );
          } else {
            is(
              aTIPWrapper.numberOfFocusNotifications,
              0,
              `${description}, IME shouldn't receive a focus notification`
            );
            ok(
              aTIPWrapper.numberOfBlurNotifications > 0,
              `${description}, IME should receive a blur notification`
            );
            ok(
              !aTIPWrapper.IMEHasFocus,
              `${description}, IME shouldn't have focus right now`
            );
          }
        } else {
          ok(
            focusEventCount > 0,
            `${description}, focus event should be fired`
          );
        }
      } else {
        is(
          aTIPWrapper.numberOfFocusNotifications,
          0,
          `${description}, IME shouldn't receive a focus notification at testing non-focusable element`
        );
        is(
          aTIPWrapper.numberOfBlurNotifications,
          0,
          `${description}, IME shouldn't receive a blur notification at testing non-focusable element`
        );
      }

      is(
        winUtils.IMEStatus,
        aTest.expectedEnabledValue,
        `${description}, wrong enabled state`
      );
      if (aTest.expectedInputElementType && doc.designMode != "on") {
        is(
          winUtils.focusedInputType,
          aTest.expectedInputElementType,
          `${description}, wrong input type`
        );
      } else if (doc.designMode == "on") {
        is(winUtils.focusedInputType, "", `${description}, wrong input type`);
      }

      if (
        !IsIMEOpenStateSupported() ||
        winUtils.IMEStatus != Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED ||
        aTest.expectedEnabledValue != Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
      ) {
        return;
      }

      // IME Open state testing
      testOpened(false);
      testOpened(true);
    } finally {
      element.remove();
      aTIPWrapper.clearFocusBlurNotifications();
    }
  }

  aWindow.focus();
  doc.activeElement?.blur();
  if (doc.designMode == "on") {
    is(
      winUtils.IMEStatus,
      Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
      `runIMEStateOnFocusMoveTests(${aGlobalDescription}): When no element has focus, IME should stay enabled in design mode`
    );
  } else {
    is(
      winUtils.IMEStatus,
      Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
      `runIMEStateOnFocusMoveTests(${aGlobalDescription}): When no element has focus, IME should be disabled`
    );
  }

  /**
   * Utility methods for defining kIMEStateTestList.
   */

  /**
   * @param {Element} aElement
   */
  function _elementIsConnectedAndNotInDesignMode(aElement) {
    return aElement.isConnected && !nodeIsInDesignMode(aElement);
  }

  /**
   * @param {Element} aElementContainer
   * @param {bool} aElementIsEditingHost
   */
  function _elementIsFocusableIfEditingHost(
    aElementContainer,
    aElementIsEditingHost
  ) {
    return !nodeIsEditable(aElementContainer) && aElementIsEditingHost;
  }

  function _IMEStateEnabledAlways() {
    return Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   * @param {bool} aElementIsEditingHost
   */
  function _IMEStateEnabledIfEditable(aElement, aElementIsEditingHost) {
    return nodeIsEditable(aElement) || aElementIsEditingHost
      ? Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED
      : Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED;
  }

  /**
   * @param {Element} aElement
   */
  function _IMEStateEnabledIfInDesignMode(aElement) {
    return _elementIsConnectedAndNotInDesignMode(aElement)
      ? Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED
      : Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   */
  function _IMEStatePasswordIfNotInDesignMode(aElement) {
    return _elementIsConnectedAndNotInDesignMode(aElement)
      ? Ci.nsIDOMWindowUtils.IME_STATUS_PASSWORD
      : Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED;
  }

  /**
   * @param {Element} aElement
   */
  function _elementIsConnectedAndNotEditable(aElement) {
    return aElement.isConnected && !nodeIsEditable(aElement);
  }

  /**
   * @param {Element} aElementContainer
   */
  function _focusEventIsExpectedUnlessEditableChild(aElementContainer) {
    return !nodeIsEditable(aElementContainer);
  }

  // Form controls except text editable elements are "disable" in normal
  // condition, however, if they are editable, they are "enabled".
  // XXX Probably there are some bugs: If the form controls editable, they
  //     shouldn't be focusable.
  function resolveTest(aTest) {
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
  const kIMEStateTestList = [
    {
      description: "input[type=text]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
    },
    {
      description: "input[type=password]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
    },
    {
      description: "input[type=checkbox]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "checkbox");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=radio]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "radio");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=submit]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "submit");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=reset]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "reset");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=file]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "file");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=button]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "button");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=image]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "image");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "input[type=url]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedInputElementType: "url",
    },
    {
      description: "input[type=email]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedInputElementType: "email",
    },
    {
      description: "input[type=search]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedInputElementType: "search",
    },
    {
      description: "input[type=tel]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedInputElementType: "tel",
    },
    {
      description: "input[type=number]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedInputElementType: "number",
    },
    {
      description: "input[type=date]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "date");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "date",
    },
    {
      description: "input[type=datetime-local]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "datetime-local");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "datetime-local",
    },
    {
      description: "input[type=time]",
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "time");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
      expectedInputElementType: "time",
    },
    // TODO(bug 1283382, bug 1283382): month and week

    // form controls
    {
      description: "button",
      createElement: aDocument => aDocument.createElement("button"),
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },
    {
      description: "textarea",
      createElement: aDocument => aDocument.createElement("textarea"),
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: "textarea[readonly]",
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("readonly", "");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
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
      isFocusable: _elementIsConnectedAndNotEditable,
      focusEventIsExpected: _focusEventIsExpectedUnlessEditableChild,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
    },

    // audio element
    {
      description: "audio[controls]",
      createElement: aDocument => {
        const element = aDocument.createElement("audio");
        element.setAttribute("controls", "");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
    },

    // video element
    {
      description: "video",
      createElement: aDocument => {
        const element = aDocument.createElement("video");
        element.setAttribute("controls", "");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfEditable,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledIfInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=text][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=text][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "text");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=url][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=url][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=url][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "url");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=email][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=email][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=email][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "email");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=search][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=search][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=search][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "search");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      expectedEnabledValue: _IMEStateEnabledAlways,
      isFocusable: _elementIsConnectedAndNotInDesignMode,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=tel][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=tel][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=tel][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "tel");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=number][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=number][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=number][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "number");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },

    {
      description: 'input[type=password][style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },
    {
      description: 'input[type=password][style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'input[type=password][style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("input");
        element.setAttribute("type", "password");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
    },
    {
      description: 'textarea[style="ime-mode: auto;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: auto;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'textarea[style="ime-mode: normal;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: normal;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
    {
      description: 'textarea[style="ime-mode: active;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: active;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedOpenState: true,
    },
    {
      description: 'textarea[style="ime-mode: inactive;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: inactive;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
      expectedOpenState: false,
    },
    {
      description: 'textarea[style="ime-mode: disabled;"]',
      createElement: aDocument => {
        const element = aDocument.createElement("textarea");
        element.setAttribute("style", "ime-mode: disabled;");
        return element;
      },
      isFocusable: _elementIsConnectedAndNotInDesignMode,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStatePasswordIfNotInDesignMode,
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
      isFocusable: _elementIsFocusableIfEditingHost,
      focusEventIsExpected: () => true,
      expectedEnabledValue: _IMEStateEnabledAlways,
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
      expectedEnabledValue: _IMEStateEnabledAlways,
    },
  ];

  for (const test of kIMEStateTestList) {
    await runTest(resolveTest(test));
  }
}
