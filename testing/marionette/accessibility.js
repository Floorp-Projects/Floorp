/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Accessibility, Components, Log, ElementNotAccessibleError,
          XPCOMUtils */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Log.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'setInterval',
  'resource://gre/modules/Timer.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'clearInterval',
  'resource://gre/modules/Timer.jsm');
XPCOMUtils.defineLazyModuleGetter(this, 'ElementNotAccessibleError',
  'chrome://marionette/content/error.js');

this.EXPORTED_SYMBOLS = ['Accessibility'];

/**
 * Accessible states used to check element's state from the accessiblity API
 * perspective.
 */
const states = {
  unavailable: Ci.nsIAccessibleStates.STATE_UNAVAILABLE,
  focusable: Ci.nsIAccessibleStates.STATE_FOCUSABLE,
  selectable: Ci.nsIAccessibleStates.STATE_SELECTABLE,
  selected: Ci.nsIAccessibleStates.STATE_SELECTED
};

var logger = Log.repository.getLogger('Marionette');

/**
 * Component responsible for interacting with platform accessibility API. Its
 * methods serve as wrappers for testing content and chrome accessibility as
 * well as accessibility of user interactions.
 *
 * @param {Function} getCapabilies
 *        Session capabilities getter.
 */
this.Accessibility = function Accessibility(getCapabilies = () => {}) {
  // A flag indicating whether the accessibility issue should be logged or cause
  // an exception. Default: log to stdout.
  Object.defineProperty(this, 'strict', {
    configurable: true,
    get: function() {
      let capabilies = getCapabilies();
      return !!capabilies.raisesAccessibilityExceptions;
    }
  });
  // An interface for in-process accessibility clients
  // Note: we access it lazily to not enable accessibility when it is not needed
  Object.defineProperty(this, 'retrieval', {
    configurable: true,
    get: function() {
      delete this.retrieval;
      this.retrieval = Cc[
        '@mozilla.org/accessibleRetrieval;1'].getService(
          Ci.nsIAccessibleRetrieval);
      return this.retrieval;
    }
  });
};

Accessibility.prototype = {

  /**
   * Number of attempts to get an accessible object for an element. We attempt
   * more than once because accessible tree can be out of sync with the DOM tree
   * for a short period of time.
   * @type {Number}
   */
  GET_ACCESSIBLE_ATTEMPTS: 100,

  /**
   * An interval between attempts to retrieve an accessible object for an
   * element.
   * @type {Number} ms
   */
  GET_ACCESSIBLE_ATTEMPT_INTERVAL: 10,

  /**
   * Accessible object roles that support some action
   * @type Object
   */
  ACTIONABLE_ROLES: new Set([
    'pushbutton',
    'checkbutton',
    'combobox',
    'key',
    'link',
    'menuitem',
    'check menu item',
    'radio menu item',
    'option',
    'listbox option',
    'listbox rich option',
    'check rich option',
    'combobox option',
    'radiobutton',
    'rowheader',
    'switch',
    'slider',
    'spinbutton',
    'pagetab',
    'entry',
    'outlineitem'
  ]),

  /**
   * Get an accessible object for a DOM element
   * @param nsIDOMElement element
   * @param Boolean mustHaveAccessible a flag indicating that the element must
   * have an accessible object
   * @return nsIAccessible object for the element
   */
  getAccessibleObject(element, mustHaveAccessible = false) {
    return new Promise((resolve, reject) => {
      let acc = this.retrieval.getAccessibleFor(element);

      if (acc || !mustHaveAccessible) {
        // If accessible object is found, return it. If it is not required,
        // also resolve.
        resolve(acc);
      } else if (mustHaveAccessible && !this.strict) {
        // If we must have an accessible but are not raising accessibility
        // exceptions, reject now and avoid polling for an accessible object.
        reject();
      } else {
        // If we require an accessible object, we need to poll for it because
        // accessible tree might be out of sync with DOM tree for a short time.
        let attempts = this.GET_ACCESSIBLE_ATTEMPTS;
        let intervalId = setInterval(() => {
          let acc = this.retrieval.getAccessibleFor(element);
          if (acc || --attempts <= 0) {
            clearInterval(intervalId);
            if (acc) { resolve(acc); }
            else { reject(); }
          }
        }, this.GET_ACCESSIBLE_ATTEMPT_INTERVAL);
      }
    }).catch(() => this.error(
      'Element does not have an accessible object', element));
  },

  /**
   * Check if the accessible has a role that supports some action
   * @param nsIAccessible object
   * @return Boolean an indicator of role being actionable
   */
  isActionableRole(accessible) {
    return this.ACTIONABLE_ROLES.has(
      this.retrieval.getStringRole(accessible.role));
  },

  /**
   * Determine if an accessible has at least one action that it supports
   * @param nsIAccessible object
   * @return Boolean an indicator of supporting at least one accessible action
   */
  hasActionCount(accessible) {
    return accessible.actionCount > 0;
  },

  /**
   * Determine if an accessible has a valid name
   * @param nsIAccessible object
   * @return Boolean an indicator that the element has a non empty valid name
   */
  hasValidName(accessible) {
    return accessible.name && accessible.name.trim();
  },

  /**
   * Check if an accessible has a set hidden attribute
   * @param nsIAccessible object
   * @return Boolean an indicator that the element has a hidden accessible
   * attribute set to true
   */
  hasHiddenAttribute(accessible) {
    let hidden = false;
    try {
      hidden = accessible.attributes.getStringProperty('hidden');
    } finally {
      // If the property is missing, exception will be thrown.
      return hidden && hidden === 'true';
    }
  },

  /**
   * Verify if an accessible has a given state
   * @param nsIAccessible object
   * @param Number stateToMatch the state to match
   * @return Boolean accessible has a state
   */
  matchState(accessible, stateToMatch) {
    let state = {};
    accessible.getState(state, {});
    return !!(state.value & stateToMatch);
  },

  /**
   * Check if an accessible is hidden from the user of the accessibility API
   * @param nsIAccessible object
   * @return Boolean an indicator that the element is hidden from the user
   */
  isHidden(accessible) {
    while (accessible) {
      if (this.hasHiddenAttribute(accessible)) {
        return true;
      }
      accessible = accessible.parent;
    }
    return false;
  },

  /**
   * Send an error message or log the error message in the log
   * @param String message
   * @param DOMElement element that caused an error
   */
  error(message, element) {
    if (!message) {
      return;
    }
    if (element) {
      let {id, tagName, className} = element;
      message += `: id: ${id}, tagName: ${tagName}, className: ${className}`;
    }
    if (this.strict) {
      throw new ElementNotAccessibleError(message);
    }
    logger.debug(message);
  },

  /**
   * Check if the element's visible state corresponds to its accessibility API
   * visibility
   * @param nsIAccessible object
   * @param WebElement corresponding to nsIAccessible object
   * @param Boolean visible element's visibility state
   */
  checkVisible(accessible, element, visible) {
    if (!accessible) {
      return;
    }
    let hiddenAccessibility = this.isHidden(accessible);
    let message;
    if (visible && hiddenAccessibility) {
      message = 'Element is not currently visible via the accessibility API ' +
        'and may not be manipulated by it';
    } else if (!visible && !hiddenAccessibility) {
      message = 'Element is currently only visible via the accessibility API ' +
        'and can be manipulated by it';
    }
    this.error(message, element);
  },

  /**
   * Check if the element's unavailable accessibility state matches the enabled
   * state
   * @param nsIAccessible object
   * @param WebElement corresponding to nsIAccessible object
   * @param Boolean enabled element's enabled state
   * @param Object container frame and optional ShadowDOM
   */
  checkEnabled(accessible, element, enabled, container) {
    if (!accessible) {
      return;
    }
    let disabledAccessibility = this.matchState(accessible, states.unavailable);
    let explorable = container.frame.document.defaultView.getComputedStyle(
      element).getPropertyValue('pointer-events') !== 'none';
    let message;

    if (!explorable && !disabledAccessibility) {
      message = 'Element is enabled but is not explorable via the ' +
        'accessibility API';
    } else if (enabled && disabledAccessibility) {
      message = 'Element is enabled but disabled via the accessibility API';
    } else if (!enabled && !disabledAccessibility) {
      message = 'Element is disabled but enabled via the accessibility API';
    }
    this.error(message, element);
  },

  /**
   * Check if it is possible to activate an element with the accessibility API
   * @param nsIAccessible object
   * @param WebElement corresponding to nsIAccessible object
   */
  checkActionable(accessible, element) {
    if (!accessible) {
      return;
    }
    let message;
    if (!this.hasActionCount(accessible)) {
      message = 'Element does not support any accessible actions';
    } else if (!this.isActionableRole(accessible)) {
      message = 'Element does not have a correct accessibility role ' +
        'and may not be manipulated via the accessibility API';
    } else if (!this.hasValidName(accessible)) {
      message = 'Element is missing an accessible name';
    } else if (!this.matchState(accessible, states.focusable)) {
      message = 'Element is not focusable via the accessibility API';
    }
    this.error(message, element);
  },

  /**
   * Check if element's selected state corresponds to its accessibility API
   * selected state.
   * @param nsIAccessible object
   * @param WebElement corresponding to nsIAccessible object
   * @param Boolean selected element's selected state
   */
  checkSelected(accessible, element, selected) {
    if (!accessible) {
      return;
    }
    if (!this.matchState(accessible, states.selectable)) {
      // Element is not selectable via the accessibility API
      return;
    }

    let selectedAccessibility = this.matchState(accessible, states.selected);
    let message;
    if (selected && !selectedAccessibility) {
      message =
        'Element is selected but not selected via the accessibility API';
    } else if (!selected && selectedAccessibility) {
      message =
        'Element is not selected but selected via the accessibility API';
    }
    this.error(message, element);
  }
};
