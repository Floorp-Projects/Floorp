/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/error.js");
Cu.import("chrome://marionette/content/element.js");
Cu.import("chrome://marionette/content/event.js");

this.EXPORTED_SYMBOLS = ["Interactions"];

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
 * A collection of interactions available in marionette.
 * @type {Object}
 */
this.Interactions = function(getCapabilies) {
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
    let visible = element.isVisible(el);
    if (!visible) {
      throw new ElementNotVisibleError('Element is not visible');
    }
    return this.accessibility.getAccessibleObject(el, true).then(acc => {
      this.accessibility.checkVisible(acc, el, visible);
      if (atom.isElementEnabled(el)) {
        this.accessibility.checkEnabled(acc, el, true, container);
        this.accessibility.checkActionable(acc, el);
        if (element.isXULElement(el)) {
          el.click();
        } else {
          let rects = el.getClientRects();
          let win = el.ownerDocument.defaultView;
          event.synthesizeMouseAtPoint(
              rects[0].left + rects[0].width / 2,
              rects[0].top + rects[0].height / 2,
              {} /* opts */,
              win);
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
      event.sendKeysToElement(
          value, el, {ignoreVisibility: false}, container.frame);
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
    let displayed = atom.isElementDisplayed(el, container.frame);
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
    if (element.isXULElement(el)) {
      // Check if XUL element supports disabled attribute
      if (DISABLED_ATTRIBUTE_SUPPORTED_XUL.has(el.tagName.toUpperCase())) {
        let disabled = atom.getElementAttribute(el, 'disabled', container.frame);
        if (disabled && disabled === 'true') {
          enabled = false;
        }
      }
    } else {
      enabled = atom.isElementEnabled(el, container.frame);
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
    if (element.isXULElement(el)) {
      let tagName = el.tagName.toUpperCase();
      if (CHECKED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
        selected = el.checked;
      }
      if (SELECTED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
        selected = el.selected;
      }
    } else {
      selected = atom.isElementSelected(el, container.frame);
    }
    return this.accessibility.getAccessibleObject(el).then(acc => {
      this.accessibility.checkSelected(acc, el, selected);
      return selected;
    });
  },
};
