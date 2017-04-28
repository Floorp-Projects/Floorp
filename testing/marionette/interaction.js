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
 * performed.  If either of these fail an
 * {@code ElementNotInteractableError} is thrown.
 *
 * If |strict| is enabled (defaults to disabled), further accessibility
 * checks will be performed, and these may result in an
 * {@code ElementNotAccessibleError} being returned.
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
 * @throws {ElementNotInteractableError}
 *     If either Selenium-style visibility check or
 *     pointer-interactability check fails.
 * @throws {ElementClickInterceptedError}
 *     If |el| is obscured by another element and a click would not hit,
 *     in |specCompat| mode.
 * @throws {ElementNotAccessibleError}
 *     If |strict| is true and element is not accessible.
 * @throws {InvalidElementStateError}
 *     If |el| is not enabled.
 */
interaction.clickElement = function* (el, strict = false, specCompat = false) {
  const a11y = accessibility.get(strict);
  if (element.isXULElement(el)) {
    yield chromeClick(el, a11y);
  } else if (specCompat) {
    yield webdriverClickElement(el, a11y);
  } else {
    yield seleniumClickElement(el, a11y);
  }
};

function* webdriverClickElement (el, a11y) {
  const win = getWindow(el);
  const doc = win.document;

  // step 3
  if (el.localName == "input" && el.type == "file") {
    throw new InvalidArgumentError(
        "Cannot click <input type=file> elements");
  }

  let containerEl = element.getContainer(el);

  // step 4
  if (!element.isInView(containerEl)) {
    element.scrollIntoView(containerEl);
  }

  // step 5
  // TODO(ato): wait for containerEl to be in view

  // step 6
  // if we cannot bring the container element into the viewport
  // there is no point in checking if it is pointer-interactable
  if (!element.isInView(containerEl)) {
    throw new ElementNotInteractableError(
        error.pprint`Element ${el} could not be scrolled into view`);
  }

  // step 7
  let rects = containerEl.getClientRects();
  let clickPoint = element.getInViewCentrePoint(rects[0], win);

  if (element.isObscured(containerEl)) {
    throw new ElementClickInterceptedError(containerEl, clickPoint);
  }

  yield a11y.getAccessible(el, true).then(acc => {
    a11y.assertVisible(acc, el, true);
    a11y.assertEnabled(acc, el, true);
    a11y.assertActionable(acc, el);
  });

  // step 8
  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    event.synthesizeMouseAtPoint(clickPoint.x, clickPoint.y, {}, win);
  }

  // step 9
  yield interaction.flushEventLoop(win);

  // step 10
  // if the click causes navigation, the post-navigation checks are
  // handled by the load listener in listener.js
}

function* chromeClick (el, a11y) {
  if (!atom.isElementEnabled(el)) {
    throw new InvalidElementStateError("Element is not enabled");
  }

  yield a11y.getAccessible(el, true).then(acc => {
    a11y.assertVisible(acc, el, true);
    a11y.assertEnabled(acc, el, true);
    a11y.assertActionable(acc, el);
  });

  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    el.click();
  }
}

function* seleniumClickElement (el, a11y) {
  let win = getWindow(el);

  let visibilityCheckEl  = el;
  if (el.localName == "option") {
    visibilityCheckEl = element.getContainer(el);
  }

  if (!element.isVisible(visibilityCheckEl)) {
    throw new ElementNotInteractableError();
  }

  if (!atom.isElementEnabled(el)) {
    throw new InvalidElementStateError("Element is not enabled");
  }

  yield a11y.getAccessible(el, true).then(acc => {
    a11y.assertVisible(acc, el, true);
    a11y.assertEnabled(acc, el, true);
    a11y.assertActionable(acc, el);
  });

  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    let rects = el.getClientRects();
    let centre = element.getInViewCentrePoint(rects[0], win);
    let opts = {};
    event.synthesizeMouseAtPoint(centre.x, centre.y, opts, win);
  }
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
  let containerEl = element.getContainer(el);

  event.mouseover(containerEl);
  event.mousemove(containerEl);
  event.mousedown(containerEl);
  event.focus(containerEl);
  event.input(containerEl);

  // toggle selectedness the way holding down control works
  el.selected = !el.selected;

  event.change(containerEl);
  event.mouseup(containerEl);
  event.click(containerEl);
};

/**
 * Flushes the event loop by requesting an animation frame.
 *
 * This will wait for the browser to repaint before returning, typically
 * flushing any queued events.
 *
 * If the document is unloaded during this request, the promise is
 * rejected.
 *
 * @param {Window} win
 *     Associated window.
 *
 * @return {Promise}
 *     Promise is accepted once event queue is flushed, or rejected if
 *     |win| has closed or been unloaded before the queue can be flushed.
 */
interaction.flushEventLoop = function* (win) {
  return new Promise(resolve => {
    let handleEvent = event => {
      win.removeEventListener("beforeunload", this);
      resolve();
    };

    if (win.closed) {
      resolve();
      return;
    }

    win.addEventListener("beforeunload", handleEvent, false);
    win.requestAnimationFrame(handleEvent);
  });
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
