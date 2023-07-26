/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  waitForObserverTopic: "chrome://remote/content/marionette/sync.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

ChromeUtils.defineLazyGetter(lazy, "service", () => {
  try {
    return Cc["@mozilla.org/accessibilityService;1"].getService(
      Ci.nsIAccessibilityService
    );
  } catch (e) {
    lazy.logger.warn("Accessibility module is not present");
    return undefined;
  }
});

/** @namespace */
export const accessibility = {
  get service() {
    return lazy.service;
  },
};

/**
 * Accessible states used to check element"s state from the accessiblity API
 * perspective.
 *
 * Note: if gecko is built with --disable-accessibility, the interfaces
 * are not defined. This is why we use getters instead to be able to use
 * these statically.
 */
accessibility.State = {
  get Unavailable() {
    return Ci.nsIAccessibleStates.STATE_UNAVAILABLE;
  },
  get Focusable() {
    return Ci.nsIAccessibleStates.STATE_FOCUSABLE;
  },
  get Selectable() {
    return Ci.nsIAccessibleStates.STATE_SELECTABLE;
  },
  get Selected() {
    return Ci.nsIAccessibleStates.STATE_SELECTED;
  },
};

/**
 * Accessible object roles that support some action.
 */
accessibility.ActionableRoles = new Set([
  "checkbutton",
  "check menu item",
  "check rich option",
  "combobox",
  "combobox option",
  "entry",
  "key",
  "link",
  "listbox option",
  "listbox rich option",
  "menuitem",
  "option",
  "outlineitem",
  "pagetab",
  "pushbutton",
  "radiobutton",
  "radio menu item",
  "rowheader",
  "slider",
  "spinbutton",
  "switch",
]);

/**
 * Factory function that constructs a new {@code accessibility.Checks}
 * object with enforced strictness or not.
 */
accessibility.get = function (strict = false) {
  return new accessibility.Checks(!!strict);
};

/**
 * Wait for the document accessibility state to be different from STATE_BUSY.
 *
 * @param {Document} doc
 *     The document to wait for.
 * @returns {Promise}
 *     A promise which resolves when the document's accessibility state is no
 *     longer busy.
 */
function waitForDocumentAccessibility(doc) {
  const documentAccessible = accessibility.service.getAccessibleFor(doc);
  const state = {};
  documentAccessible.getState(state, {});
  if ((state.value & Ci.nsIAccessibleStates.STATE_BUSY) == 0) {
    return Promise.resolve();
  }

  // Accessibility for the doc is busy, so wait for the state to change.
  return lazy.waitForObserverTopic("accessible-event", {
    checkFn: subject => {
      // If event type does not match expected type, skip the event.
      // If event's accessible does not match expected accessible,
      // skip the event.
      const event = subject.QueryInterface(Ci.nsIAccessibleEvent);
      return (
        event.eventType === Ci.nsIAccessibleEvent.EVENT_STATE_CHANGE &&
        event.accessible === documentAccessible
      );
    },
  });
}

/**
 * Retrieve the Accessible for the provided element.
 *
 * @param {Element} element
 *     The element for which we need to retrieve the accessible.
 *
 * @returns {nsIAccessible|null}
 *     The Accessible object corresponding to the provided element or null if
 *     the accessibility service is not available.
 */
accessibility.getAccessible = async function (element) {
  if (!accessibility.service) {
    return null;
  }

  // First, wait for accessibility to be ready for the element's document.
  await waitForDocumentAccessibility(element.ownerDocument);

  const acc = accessibility.service.getAccessibleFor(element);
  if (acc) {
    return acc;
  }

  // The Accessible doesn't exist yet. This can happen because a11y tree
  // mutations happen during refresh driver ticks. Stop the refresh driver from
  // doing its regular ticks and force two refresh driver ticks: the first to
  // let layout update and notify a11y, and the second to let a11y process
  // updates.
  const windowUtils = element.ownerGlobal.windowUtils;
  windowUtils.advanceTimeAndRefresh(0);
  windowUtils.advanceTimeAndRefresh(0);
  // Go back to normal refresh driver ticks.
  windowUtils.restoreNormalRefresh();
  return accessibility.service.getAccessibleFor(element);
};

/**
 * Component responsible for interacting with platform accessibility
 * API.
 *
 * Its methods serve as wrappers for testing content and chrome
 * accessibility as well as accessibility of user interactions.
 */
accessibility.Checks = class {
  /**
   * @param {boolean} strict
   *     Flag indicating whether the accessibility issue should be logged
   *     or cause an error to be thrown.  Default is to log to stdout.
   */
  constructor(strict) {
    this.strict = strict;
  }

  /**
   * Assert that the element has a corresponding accessible object, and retrieve
   * this accessible. Note that if the accessibility.Checks component was
   * created in non-strict mode, this helper will not attempt to resolve the
   * accessible at all and will simply return null.
   *
   * @param {DOMElement|XULElement} element
   *     Element to get the accessible object for.
   * @param {boolean=} mustHaveAccessible
   *     Flag indicating that the element must have an accessible object.
   *     Defaults to not require this.
   *
   * @returns {Promise.<nsIAccessible>}
   *     Promise with an accessibility object for the given element.
   */
  async assertAccessible(element, mustHaveAccessible = false) {
    if (!this.strict) {
      return null;
    }

    const accessible = await accessibility.getAccessible(element);
    if (!accessible && mustHaveAccessible) {
      this.error("Element does not have an accessible object", element);
    }

    return accessible;
  }

  /**
   * Test if the accessible has a role that supports some arbitrary
   * action.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   *
   * @returns {boolean}
   *     True if an actionable role is found on the accessible, false
   *     otherwise.
   */
  isActionableRole(accessible) {
    return accessibility.ActionableRoles.has(
      accessibility.service.getStringRole(accessible.role)
    );
  }

  /**
   * Test if an accessible has at least one action that it supports.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   *
   * @returns {boolean}
   *     True if the accessible has at least one supported action,
   *     false otherwise.
   */
  hasActionCount(accessible) {
    return accessible.actionCount > 0;
  }

  /**
   * Test if an accessible has a valid name.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   *
   * @returns {boolean}
   *     True if the accessible has a non-empty valid name, or false if
   *     this is not the case.
   */
  hasValidName(accessible) {
    return accessible.name && accessible.name.trim();
  }

  /**
   * Test if an accessible has a {@code hidden} attribute.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   *
   * @returns {boolean}
   *     True if the accessible object has a {@code hidden} attribute,
   *     false otherwise.
   */
  hasHiddenAttribute(accessible) {
    let hidden = false;
    try {
      hidden = accessible.attributes.getStringProperty("hidden");
    } catch (e) {}
    // if the property is missing, error will be thrown
    return hidden && hidden === "true";
  }

  /**
   * Verify if an accessible has a given state.
   * Test if an accessible has a given state.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object to test.
   * @param {number} stateToMatch
   *     State to match.
   *
   * @returns {boolean}
   *     True if |accessible| has |stateToMatch|, false otherwise.
   */
  matchState(accessible, stateToMatch) {
    let state = {};
    accessible.getState(state, {});
    return !!(state.value & stateToMatch);
  }

  /**
   * Test if an accessible is hidden from the user.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   *
   * @returns {boolean}
   *     True if element is hidden from user, false otherwise.
   */
  isHidden(accessible) {
    if (!accessible) {
      return true;
    }

    while (accessible) {
      if (this.hasHiddenAttribute(accessible)) {
        return true;
      }
      accessible = accessible.parent;
    }
    return false;
  }

  /**
   * Test if the element's visible state corresponds to its accessibility
   * API visibility.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   * @param {DOMElement|XULElement} element
   *     Element associated with |accessible|.
   * @param {boolean} visible
   *     Visibility state of |element|.
   *
   * @throws ElementNotAccessibleError
   *     If |element|'s visibility state does not correspond to
   *     |accessible|'s.
   */
  assertVisible(accessible, element, visible) {
    let hiddenAccessibility = this.isHidden(accessible);

    let message;
    if (visible && hiddenAccessibility) {
      message =
        "Element is not currently visible via the accessibility API " +
        "and may not be manipulated by it";
    } else if (!visible && !hiddenAccessibility) {
      message =
        "Element is currently only visible via the accessibility API " +
        "and can be manipulated by it";
    }
    this.error(message, element);
  }

  /**
   * Test if the element's unavailable accessibility state matches the
   * enabled state.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   * @param {DOMElement|XULElement} element
   *     Element associated with |accessible|.
   * @param {boolean} enabled
   *     Enabled state of |element|.
   *
   * @throws ElementNotAccessibleError
   *     If |element|'s enabled state does not match |accessible|'s.
   */
  assertEnabled(accessible, element, enabled) {
    if (!accessible) {
      return;
    }

    let win = element.ownerGlobal;
    let disabledAccessibility = this.matchState(
      accessible,
      accessibility.State.Unavailable
    );
    let explorable =
      win.getComputedStyle(element).getPropertyValue("pointer-events") !==
      "none";

    let message;
    if (!explorable && !disabledAccessibility) {
      message =
        "Element is enabled but is not explorable via the " +
        "accessibility API";
    } else if (enabled && disabledAccessibility) {
      message = "Element is enabled but disabled via the accessibility API";
    } else if (!enabled && !disabledAccessibility) {
      message = "Element is disabled but enabled via the accessibility API";
    }
    this.error(message, element);
  }

  /**
   * Test if it is possible to activate an element with the accessibility
   * API.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   * @param {DOMElement|XULElement} element
   *     Element associated with |accessible|.
   *
   * @throws ElementNotAccessibleError
   *     If it is impossible to activate |element| with |accessible|.
   */
  assertActionable(accessible, element) {
    if (!accessible) {
      return;
    }

    let message;
    if (!this.hasActionCount(accessible)) {
      message = "Element does not support any accessible actions";
    } else if (!this.isActionableRole(accessible)) {
      message =
        "Element does not have a correct accessibility role " +
        "and may not be manipulated via the accessibility API";
    } else if (!this.hasValidName(accessible)) {
      message = "Element is missing an accessible name";
    } else if (!this.matchState(accessible, accessibility.State.Focusable)) {
      message = "Element is not focusable via the accessibility API";
    }

    this.error(message, element);
  }

  /**
   * Test that an element's selected state corresponds to its
   * accessibility API selected state.
   *
   * @param {nsIAccessible} accessible
   *     Accessible object.
   * @param {DOMElement|XULElement} element
   *     Element associated with |accessible|.
   * @param {boolean} selected
   *     The |element|s selected state.
   *
   * @throws ElementNotAccessibleError
   *     If |element|'s selected state does not correspond to
   *     |accessible|'s.
   */
  assertSelected(accessible, element, selected) {
    if (!accessible) {
      return;
    }

    // element is not selectable via the accessibility API
    if (!this.matchState(accessible, accessibility.State.Selectable)) {
      return;
    }

    let selectedAccessibility = this.matchState(
      accessible,
      accessibility.State.Selected
    );

    let message;
    if (selected && !selectedAccessibility) {
      message =
        "Element is selected but not selected via the accessibility API";
    } else if (!selected && selectedAccessibility) {
      message =
        "Element is not selected but selected via the accessibility API";
    }
    this.error(message, element);
  }

  /**
   * Throw an error if strict accessibility checks are enforced and log
   * the error to the log.
   *
   * @param {string} message
   * @param {DOMElement|XULElement} element
   *     Element that caused an error.
   *
   * @throws ElementNotAccessibleError
   *     If |strict| is true.
   */
  error(message, element) {
    if (!message || !this.strict) {
      return;
    }
    if (element) {
      let { id, tagName, className } = element;
      message += `: id: ${id}, tagName: ${tagName}, className: ${className}`;
    }

    throw new lazy.error.ElementNotAccessibleError(message);
  }
};
