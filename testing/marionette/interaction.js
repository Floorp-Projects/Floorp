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

Cu.importGlobalProperties(["File"]);

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
 * The element is scrolled into view before visibility- or interactability
 * checks are performed.
 *
 * Selenium-style visibility checks will be performed if |specCompat|
 * is false (default).  Otherwise pointer-interactability checks will be
 * performed.  If either of these fail an {@code ElementNotVisibleError}
 * is returned.
 *
 * If |strict| is enabled (defaults to disabled), further accessibility
 * checks will be performed, and these may result in an {@code
 * ElementNotAccessibleError} being returned.
 *
 * When |el| is not enabled, an {@code InvalidElementStateError}
 * is returned.
 *
 * @param {DOMElement|XULElement} el
 *     Element to click.
 * @param {boolean=} strict
 *     Enforce strict accessibility tests.
 * @param {boolean=} specCompat
 *     Use WebDriver specification compatible interactability definition.
 *
 * @throws {ElementNotVisibleError}
 *     If either Selenium-style visibility check or
 *     pointer-interactability check fails.
 * @throws {ElementNotAccessibleError}
 *     If |strict| is true and element is not accessible.
 * @throws {InvalidElementStateError}
 *     If |el| is not enabled.
 */
interaction.clickElement = function*(el, strict = false, specCompat = false) {
  let win = getWindow(el);
  let a11y = accessibility.get(strict);

  let visibilityCheckEl  = el;
  if (el.localName == "option") {
    visibilityCheckEl = interaction.getSelectForOptionElement(el);
  }

  let interactable = false;
  if (specCompat) {
    if (!element.isPointerInteractable(visibilityCheckEl)) {
      element.scrollIntoView(el);
    }
    interactable = element.isPointerInteractable(visibilityCheckEl);
  } else {
    interactable = element.isVisible(visibilityCheckEl);
  }
  if (!interactable) {
    throw new ElementNotVisibleError();
  }

  if (!atom.isElementEnabled(el)) {
    throw new InvalidElementStateError("Element is not enabled");
  }

  yield a11y.getAccessible(el, true).then(acc => {
    a11y.assertVisible(acc, el, interactable);
    a11y.assertEnabled(acc, el, true);
    a11y.assertActionable(acc, el);
  });

  // chrome elements
  if (element.isXULElement(el)) {
    if (el.localName == "option") {
      interaction.selectOption(el);
    } else {
      el.click();
    }

  // content elements
  } else {
    if (el.localName == "option") {
      interaction.selectOption(el);
    } else {
      let centre = interaction.calculateCentreCoords(el);
      let opts = {};
      event.synthesizeMouseAtPoint(centre.x, centre.y, opts, win);
    }
  }
};

/**
 * Calculate the in-view centre point, that is the centre point of the
 * area of the first DOM client rectangle that is inside the viewport.
 *
 * @param {DOMElement} el
 *     Element to calculate the visible centre point of.
 *
 * @return {Object.<string, number>}
 *     X- and Y-position.
 */
interaction.calculateCentreCoords = function (el) {
  let rects = el.getClientRects();
  return {
    x: rects[0].left + rects[0].width / 2.0,
    y: rects[0].top + rects[0].height / 2.0,
  };
};

/**
 * Select <option> element in a <select> list.
 *
 * Because the dropdown list of select elements are implemented using
 * native widget technology, our trusted synthesised events are not able
 * to reach them.  Dropdowns are instead handled mimicking DOM events,
 * which for obvious reasons is not ideal, but at the current point in
 * time considered to be good enough.
 *
 * @param {HTMLOptionElement} option
 *     Option element to select.
 *
 * @throws TypeError
 *     If |el| is a XUL element or not an <option> element.
 * @throws Error
 *     If unable to find |el|'s parent <select> element.
 */
interaction.selectOption = function (el) {
  if (element.isXULElement(el)) {
    throw new Error("XUL dropdowns not supported");
  }
  if (el.localName != "option") {
    throw new TypeError("Invalid elements");
  }

  let win = getWindow(el);
  let parent = interaction.getSelectForOptionElement(el);

  event.mouseover(parent);
  event.mousemove(parent);
  event.mousedown(parent);
  event.focus(parent);
  event.input(parent);

  // toggle selectedness the way holding down control works
  el.selected = !el.selected;

  event.change(parent);
  event.mouseup(parent);
  event.click(parent);
};

/**
 * Appends |path| to an <input type=file>'s file list.
 *
 * @param {HTMLInputElement} el
 *     An <input type=file> element.
 * @param {string} path
 *     Full path to file.
 */
interaction.uploadFile = function* (el, path) {
  let file = yield File.createFromFileName(path).then(file => {
    return file;
  }, () => {
    return null;
  });

  if (!file) {
    throw new InvalidArgumentError("File not found: " + path);
  }

  let fs = Array.prototype.slice.call(el.files);
  fs.push(file);

  // <input type=file> opens OS widget dialogue
  // which means the mousedown/focus/mouseup/click events
  // occur before the change event
  event.mouseover(el);
  event.mousemove(el);
  event.mousedown(el);
  event.focus(el);
  event.mouseup(el);
  event.click(el);

  el.mozSetFileArray(fs);

  event.change(el);
};

/**
 * Locate the <select> element that encapsulate an <option> element.
 *
 * @param {HTMLOptionElement} optionEl
 *     Option element.
 *
 * @return {HTMLSelectElement}
 *     Select element wrapping |optionEl|.
 *
 * @throws {Error}
 *     If unable to find the <select> element.
 */
interaction.getSelectForOptionElement = function (optionEl) {
  let parent = optionEl;
  while (parent.parentNode && parent.localName != "select") {
    parent = parent.parentNode;
  }

  if (parent.localName != "select") {
    throw new Error("Unable to find parent of <option> element");
  }

  return parent;
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
interaction.sendKeysToElement = function (el, value, ignoreVisibility, strict = false) {
  let win = getWindow(el);
  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el, true).then(acc => {
    a11y.assertActionable(acc, el);
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
interaction.isElementDisplayed = function (el, strict = false) {
  let win = getWindow(el);
  let displayed = atom.isElementDisplayed(el, win);

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el).then(acc => {
    a11y.assertVisible(acc, el, displayed);
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
interaction.isElementEnabled = function (el, strict = false) {
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
    a11y.assertEnabled(acc, el, enabled);
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
interaction.isElementSelected = function (el, strict = false) {
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
    a11y.assertSelected(acc, el, selected);
    return selected;
  });
};

function getWindow(el) {
  return el.ownerGlobal;
}
