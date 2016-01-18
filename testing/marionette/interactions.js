/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global Components, Accessibility, ElementNotVisibleError,
   InvalidElementStateError, Interactions */

var {utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ['Interactions'];

Cu.import('chrome://marionette/content/accessibility.js');
Cu.import('chrome://marionette/content/error.js');

/**
 * XUL elements that support disabled attribtue.
 */
const DISABLED_ATTRIBUTE_SUPPORTED_XUL = new Set([
  'ARROWSCROLLBOX',
  'BUTTON',
  'CHECKBOX',
  'COLORPICKER',
  'COMMAND',
  'DATEPICKER',
  'DESCRIPTION',
  'KEY',
  'KEYSET',
  'LABEL',
  'LISTBOX',
  'LISTCELL',
  'LISTHEAD',
  'LISTHEADER',
  'LISTITEM',
  'MENU',
  'MENUITEM',
  'MENULIST',
  'MENUSEPARATOR',
  'PREFERENCE',
  'RADIO',
  'RADIOGROUP',
  'RICHLISTBOX',
  'RICHLISTITEM',
  'SCALE',
  'TAB',
  'TABS',
  'TEXTBOX',
  'TIMEPICKER',
  'TOOLBARBUTTON',
  'TREE'
]);

/**
 * XUL elements that support checked property.
 */
const CHECKED_PROPERTY_SUPPORTED_XUL = new Set([
  'BUTTON',
  'CHECKBOX',
  'LISTITEM',
  'TOOLBARBUTTON'
]);

/**
 * XUL elements that support selected property.
 */
const SELECTED_PROPERTY_SUPPORTED_XUL = new Set([
  'LISTITEM',
  'MENU',
  'MENUITEM',
  'MENUSEPARATOR',
  'RADIO',
  'RICHLISTITEM',
  'TAB'
]);

/**
 * This function generates a pair of coordinates relative to the viewport given
 * a target element and coordinates relative to that element's top-left corner.
 * @param 'x', and 'y' are the relative to the target.
 *        If they are not specified, then the center of the target is used.
 */
function coordinates(target, x, y) {
  let box = target.getBoundingClientRect();
  if (typeof x === 'undefined') {
    x = box.width / 2;
  }
  if (typeof y === 'undefined') {
    y = box.height / 2;
  }
  return {
    x: box.left + x,
    y: box.top + y
  };
}

/**
 * A collection of interactions available in marionette.
 * @type {Object}
 */
this.Interactions = function(utils, getCapabilies) {
  this.utils = utils;
  this.accessibility = new Accessibility(getCapabilies);
};

Interactions.prototype = {
  /**
   * Send click event to element.
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @param ElementManager elementManager
   *
   * @param String id
   *        The DOM reference ID
   */
  clickElement(container, elementManager, id) {
    let el = elementManager.getKnownElement(id, container);
    let visible = this.checkVisible(container, el);
    if (!visible) {
      throw new ElementNotVisibleError('Element is not visible');
    }
    return this.accessibility.getAccessibleObject(el, true).then(acc => {
      this.accessibility.checkVisible(acc, el, visible);
      if (this.utils.isElementEnabled(el)) {
        this.accessibility.checkEnabled(acc, el, true, container);
        this.accessibility.checkActionable(acc, el);
        if (this.isXULElement(el)) {
          el.click();
        } else {
          let rects = el.getClientRects();
          this.utils.synthesizeMouseAtPoint(rects[0].left + rects[0].width/2,
                                            rects[0].top + rects[0].height/2,
                                            {}, el.ownerDocument.defaultView);
        }
      } else {
        throw new InvalidElementStateError('Element is not enabled');
      }
    });
  },

  /**
   * Send keys to element
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @param ElementManager elementManager
   *
   * @param String id
   *        The DOM reference ID
   *
   * @param String?Array value
   *        Value to send to an element
   *
   * @param Boolean ignoreVisibility
   *        A flag to check element visibility
   */
  sendKeysToElement(container, elementManager, id, value, ignoreVisibility) {
    let el = elementManager.getKnownElement(id, container);
    return this.accessibility.getAccessibleObject(el, true).then(acc => {
      this.accessibility.checkActionable(acc, el);
      this.utils.sendKeysToElement(
        container.frame, el, value, ignoreVisibility);
    });
  },

  /**
   * Determine the element displayedness of the given web element.
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @param ElementManager elementManager
   *
   * @param {WebElement} id
   *     Reference to web element.
   *
   * Also performs additional accessibility checks if enabled by session
   * capability.
   */
  isElementDisplayed(container, elementManager, id) {
    let el = elementManager.getKnownElement(id, container);
    let displayed = this.utils.isElementDisplayed(el);
    return this.accessibility.getAccessibleObject(el).then(acc => {
      this.accessibility.checkVisible(acc, el, displayed);
      return displayed;
    });
  },

  /**
   * Check if element is enabled.
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @param ElementManager elementManager
   *
   * @param {WebElement} id
   *     Reference to web element.
   *
   * @return {boolean}
   *     True if enabled, false otherwise.
   */
  isElementEnabled(container, elementManager, id) {
    let el = elementManager.getKnownElement(id, container);
    let enabled = true;
    if (this.isXULElement(el)) {
      // Check if XUL element supports disabled attribute
      if (DISABLED_ATTRIBUTE_SUPPORTED_XUL.has(el.tagName.toUpperCase())) {
        let disabled = this.utils.getElementAttribute(el, 'disabled');
        if (disabled && disabled === 'true') {
          enabled = false;
        }
      }
    } else {
      enabled = this.utils.isElementEnabled(el);
    }
    return this.accessibility.getAccessibleObject(el).then(acc => {
      this.accessibility.checkEnabled(acc, el, enabled, container);
      return enabled;
    });
  },

  /**
   * Determines if the referenced element is selected or not.
   *
   * This operation only makes sense on input elements of the Checkbox-
   * and Radio Button states, or option elements.
   *
   * @param nsIDOMWindow, ShadowRoot container
   *        The window and an optional shadow root that contains the element
   *
   * @param ElementManager elementManager
   *
   * @param {WebElement} id
   *     Reference to web element.
   */
  isElementSelected(container, elementManager, id) {
    let el = elementManager.getKnownElement(id, container);
    let selected = true;
    if (this.isXULElement(el)) {
      let tagName = el.tagName.toUpperCase();
      if (CHECKED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
        selected = el.checked;
      }
      if (SELECTED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
        selected = el.selected;
      }
    } else {
      selected = this.utils.isElementSelected(el);
    }
    return this.accessibility.getAccessibleObject(el).then(acc => {
      this.accessibility.checkSelected(acc, el, selected);
      return selected;
    });
  },

  /**
   * This function throws the visibility of the element error if the element is
   * not displayed or the given coordinates are not within the viewport.
   *
   * @param 'x', and 'y' are the coordinates relative to the target.
   *        If they are not specified, then the center of the target is used.
   */
  checkVisible(container, el, x, y) {
    // Bug 1094246 - Webdriver's isShown doesn't work with content xul
    if (!this.isXULElement(el)) {
      //check if the element is visible
      let visible = this.utils.isElementDisplayed(el);
      if (!visible) {
        return false;
      }
    }

    if (el.tagName.toLowerCase() === 'body') {
      return true;
    }
    if (!this.elementInViewport(container, el, x, y)) {
      //check if scroll function exist. If so, call it.
      if (el.scrollIntoView) {
        el.scrollIntoView(false);
        if (!this.elementInViewport(container, el)) {
          return false;
        }
      }
      else {
        return false;
      }
    }
    return true;
  },

  isXULElement(el) {
    return this.utils.getElementAttribute(el, 'namespaceURI').indexOf(
      'there.is.only.xul') >= 0;
  },

  /**
   * This function returns true if the given coordinates are in the viewport.
   * @param 'x', and 'y' are the coordinates relative to the target.
   *        If they are not specified, then the center of the target is used.
   */
  elementInViewport(container, el, x, y) {
    let c = coordinates(el, x, y);
    let win = container.frame;
    let viewPort = {
      top: win.pageYOffset,
      left: win.pageXOffset,
      bottom: win.pageYOffset + win.innerHeight,
      right: win.pageXOffset + win.innerWidth
    };
    return (viewPort.left <= c.x + win.pageXOffset &&
            c.x + win.pageXOffset <= viewPort.right &&
            viewPort.top <= c.y + win.pageYOffset &&
            c.y + win.pageYOffset <= viewPort.bottom);
  }
};
