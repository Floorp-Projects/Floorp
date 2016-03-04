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

this.EXPORTED_SYMBOLS = ["interaction"];

/**
 * XUL elements that support disabled attribute.
 */
const DISABLED_ATTRIBUTE_SUPPORTED_XUL = new Set([
  "ARROWSCROLLBOX",
  "BUTTON",
  "CHECKBOX",
  "COLORPICKER",
  "COMMAND",
  "DATEPICKER",
  "DESCRIPTION",
  "KEY",
  "KEYSET",
  "LABEL",
  "LISTBOX",
  "LISTCELL",
  "LISTHEAD",
  "LISTHEADER",
  "LISTITEM",
  "MENU",
  "MENUITEM",
  "MENULIST",
  "MENUSEPARATOR",
  "PREFERENCE",
  "RADIO",
  "RADIOGROUP",
  "RICHLISTBOX",
  "RICHLISTITEM",
  "SCALE",
  "TAB",
  "TABS",
  "TEXTBOX",
  "TIMEPICKER",
  "TOOLBARBUTTON",
  "TREE",
]);

/**
 * XUL elements that support checked property.
 */
const CHECKED_PROPERTY_SUPPORTED_XUL = new Set([
  "BUTTON",
  "CHECKBOX",
  "LISTITEM",
  "TOOLBARBUTTON",
]);

/**
 * XUL elements that support selected property.
 */
const SELECTED_PROPERTY_SUPPORTED_XUL = new Set([
  "LISTITEM",
  "MENU",
  "MENUITEM",
  "MENUSEPARATOR",
  "RADIO",
  "RICHLISTITEM",
  "TAB",
]);

this.interaction = {};

/**
 * Interact with an element by clicking it.
 *
 * @param {DOMElement|XULElement} el
 *     Element to click.
 * @param {boolean=} strict
 *     Enforce strict accessibility tests.
 * @param {boolean=} specCompat
 *     Use WebDriver specification compatible interactability definition.
 */
interaction.clickElement = function(el, strict = false, specCompat = false) {
  let win = getWindow(el);

  let visible = false;
  if (specCompat) {
    visible = element.isInteractable(el);
    if (visible) {
      el.scrollIntoView(false);
    }
  } else {
    visible = element.isVisible(el, win);
  }

  if (!visible) {
    throw new ElementNotVisibleError("Element is not visible");
  }

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el, true).then(acc => {
    a11y.checkVisible(acc, el, visible);

    if (atom.isElementEnabled(el)) {
      a11y.checkEnabled(acc, el, true);
      a11y.checkActionable(acc, el);

      if (element.isXULElement(el)) {
        el.click();
      } else {
        let rects = el.getClientRects();
        event.synthesizeMouseAtPoint(
            rects[0].left + rects[0].width / 2,
            rects[0].top + rects[0].height / 2,
            {} /* opts */,
            win);
      }

    } else {
      throw new InvalidElementStateError("Element is not enabled");
    }
  });
};

/**
 * Send keys to element.
 *
 * @param {DOMElement|XULElement} el
 *     Element to send key events to.
 * @param {Array.<string>} value
 *     Sequence of keystrokes to send to the element.
 * @param {boolean} ignoreVisibility
 *     Flag to enable or disable element visibility tests.
 * @param {boolean=} strict
 *     Enforce strict accessibility tests.
 */
interaction.sendKeysToElement = function(el, value, ignoreVisibility, strict = false) {
  let win = getWindow(el);
  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el, true).then(acc => {
    a11y.checkActionable(acc, el);
    event.sendKeysToElement(value, el, {ignoreVisibility: false}, win);
  });
};

/**
 * Determine the element displayedness of an element.
 *
 * @param {DOMElement|XULElement} el
 *     Element to determine displayedness of.
 * @param {boolean=} strict
 *     Enforce strict accessibility tests.
 *
 * @return {boolean}
 *     True if element is displayed, false otherwise.
 */
interaction.isElementDisplayed = function(el, strict = false) {
  let win = getWindow(el);
  let displayed = atom.isElementDisplayed(el, win);

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el).then(acc => {
    a11y.checkVisible(acc, el, displayed);
    return displayed;
  });
};

/**
 * Check if element is enabled.
 *
 * @param {DOMElement|XULElement} el
 *     Element to test if is enabled.
 *
 * @return {boolean}
 *     True if enabled, false otherwise.
 */
interaction.isElementEnabled = function(el, strict = false) {
  let enabled = true;
  let win = getWindow(el);

  if (element.isXULElement(el)) {
    // check if XUL element supports disabled attribute
    if (DISABLED_ATTRIBUTE_SUPPORTED_XUL.has(el.tagName.toUpperCase())) {
      let disabled = atom.getElementAttribute(el, "disabled", win);
      if (disabled && disabled === "true") {
        enabled = false;
      }
    }
  } else {
    enabled = atom.isElementEnabled(el, {frame: win});
  }

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el).then(acc => {
    a11y.checkEnabled(acc, el, enabled);
    return enabled;
  });
};

/**
 * Determines if the referenced element is selected or not.
 *
 * This operation only makes sense on input elements of the Checkbox-
 * and Radio Button states, or option elements.
 *
 * @param {DOMElement|XULElement} el
 *     Element to test if is selected.
 * @param {boolean=} strict
 *     Enforce strict accessibility tests.
 *
 * @return {boolean}
 *     True if element is selected, false otherwise.
 */
interaction.isElementSelected = function(el, strict = false) {
  let selected = true;
  let win = getWindow(el);

  if (element.isXULElement(el)) {
    let tagName = el.tagName.toUpperCase();
    if (CHECKED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
      selected = el.checked;
    }
    if (SELECTED_PROPERTY_SUPPORTED_XUL.has(tagName)) {
      selected = el.selected;
    }
  } else {
    selected = atom.isElementSelected(el, win);
  }

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el).then(acc => {
    a11y.checkSelected(acc, el, selected);
    return selected;
  });
};

function getWindow(el) {
  return el.ownerDocument.defaultView;
}
