"use strict";

function IsIMEOpenStateSupported() {
  // We support to control IME open state on Windows and Mac actually.  However,
  // we cannot test it on Mac if the current keyboard layout is not CJK. And also
  // we cannot test it on Win32 if the system didn't be installed IME. So,
  // currently we should not run the open state testing.
  return false;
}

/**
 * @param {Node} aNode
 */
function nodeIsInShadowDOM(aNode) {
  for (let node = aNode; node; node = node.parentNode) {
    if (node instanceof ShadowRoot) {
      return true;
    }
    if (node == node.parentNode) {
      break;
    }
  }
  return false;
}

/**
 * @param {Node} aNode
 */
function nodeIsInDesignMode(aNode) {
  return (
    aNode.isConnected &&
    !nodeIsInShadowDOM(aNode) &&
    aNode.ownerDocument.designMode == "on"
  );
}

/**
 * param {Node} aNode
 */
function getEditingHost(aNode) {
  if (nodeIsInDesignMode(aNode)) {
    return aNode.ownerDocument.documentElement;
  }
  for (
    let element =
      aNode.nodeType == Node.ELEMENT_NODE ? aNode : aNode.parentElement;
    element;
    element = element.parentElement
  ) {
    const contenteditable = element.getAttribute("contenteditable");
    if (contenteditable === "true" || contenteditable === "") {
      return element;
    }
    if (contenteditable === "false") {
      return null;
    }
  }
  return null;
}

/**
 * @param {Node} aNode
 */
function nodeIsEditable(aNode) {
  if (nodeIsInDesignMode(aNode)) {
    return true;
  }
  if (!aNode.isConnected) {
    return false;
  }
  return getEditingHost(aNode) != null;
}

/**
 * @param {Element} aElement
 */
function elementIsEditingHost(aElement) {
  return (
    nodeIsEditable(aElement) &&
    (!aElement.parentElement || !getEditingHost(aElement) == aElement)
  );
}

/**
 * @returns {Element} Retrieve focused element.  If focused element is a element
 *                    in UA widget, this returns its host element.  E.g., when
 *                    a button in the controls of <audio> or <video> has focus,
 *                    this returns the <video> or <audio>.
 */
function getFocusedElementOrUAWidgetHost() {
  const focusedElement = SpecialPowers.focusManager.focusedElement;
  if (SpecialPowers.wrap(focusedElement)?.containingShadowRoot?.isUAWidget()) {
    return focusedElement.containingShadowRoot.host;
  }
  return focusedElement;
}

class TIPWrapper {
  #mTIP = null;
  #mFocusBlurNotifications = [];
  #mFocusBlurListener;
  #mWindow;

  constructor(aWindow) {
    this.#mWindow = aWindow;
    this.#mTIP = Cc["@mozilla.org/text-input-processor;1"].createInstance(
      Ci.nsITextInputProcessor
    );
    if (!this.beginInputTransactionForTests()) {
      this.#mTIP = null;
    }
  }

  beginInputTransactionForTests() {
    return this.#mTIP.beginInputTransactionForTests(
      this.#mWindow,
      this.#observer.bind(this)
    );
  }

  typeA() {
    const AKey = new this.#mWindow.KeyboardEvent("", {
      key: "a",
      code: "KeyA",
      keyCode: this.#mWindow.KeyboardEvent.DOM_VK_A,
    });
    this.#mTIP.keydown(AKey);
    this.#mTIP.keyup(AKey);
  }

  isAvailable() {
    return this.#mTIP != null;
  }

  #observer(aTIP, aNotification) {
    if (aTIP != this.#mTIP) {
      return false;
    }
    switch (aNotification.type) {
      case "request-to-commit":
        this.#mTIP.commitComposition();
        break;
      case "request-to-cancel":
        this.#mTIP.cancelComposition();
        break;
      case "notify-focus":
      case "notify-blur":
        this.#mFocusBlurNotifications.push(aNotification.type);
        if (this.#mFocusBlurListener) {
          this.#mFocusBlurListener(aNotification.type);
        }
        break;
    }
    return true;
  }

  get TIP() {
    return this.#mTIP;
  }

  /**
   * @param {Function} aListener
   */
  set onIMEFocusBlur(aListener) {
    this.#mFocusBlurListener = aListener;
  }

  get focusBlurNotifications() {
    return this.#mFocusBlurNotifications.concat();
  }

  get numberOfFocusNotifications() {
    return this.#mFocusBlurNotifications.filter(t => t == "notify-focus")
      .length;
  }
  get numberOfBlurNotifications() {
    return this.#mFocusBlurNotifications.filter(t => t == "notify-blur").length;
  }

  get IMEHasFocus() {
    return (
      !!this.#mFocusBlurNotifications.length &&
      this.#mFocusBlurNotifications[this.#mFocusBlurNotifications.length - 1] ==
        "notify-focus"
    );
  }

  clearFocusBlurNotifications() {
    this.#mFocusBlurNotifications = [];
  }

  destroy() {
    this.#mTIP = null;
    this.#mFocusBlurListener = null;
    this.#mFocusBlurNotifications = [];
  }
}
