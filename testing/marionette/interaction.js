/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");

Cu.import("chrome://marionette/content/accessibility.js");
Cu.import("chrome://marionette/content/atom.js");
Cu.import("chrome://marionette/content/element.js");
const {
  ElementClickInterceptedError,
  ElementNotInteractableError,
  InvalidArgumentError,
  InvalidElementStateError,
} = Cu.import("chrome://marionette/content/error.js", {});
Cu.import("chrome://marionette/content/event.js");
const {pprint} = Cu.import("chrome://marionette/content/format.js", {});
const {TimedPromise} = Cu.import("chrome://marionette/content/sync.js", {});

Cu.importGlobalProperties(["File"]);

this.EXPORTED_SYMBOLS = ["interaction"];

/** XUL elements that support disabled attribute. */
const DISABLED_ATTRIBUTE_SUPPORTED_XUL = new Set([
  "ARROWSCROLLBOX",
  "BUTTON",
  "CHECKBOX",
  "COLORPICKER",
  "COMMAND",
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
  "TOOLBARBUTTON",
  "TREE",
]);

/**
 * Common form controls that user can change the value property
 * interactively.
 */
const COMMON_FORM_CONTROLS = new Set([
  "input",
  "textarea",
  "select",
]);

/**
 * Input elements that do not fire <tt>input</tt> and <tt>change</tt>
 * events when value property changes.
 */
const INPUT_TYPES_NO_EVENT = new Set([
  "checkbox",
  "radio",
  "file",
  "hidden",
  "image",
  "reset",
  "button",
  "submit",
]);

/** @namespace */
this.interaction = {};

/**
 * Interact with an element by clicking it.
 *
 * The element is scrolled into view before visibility- or interactability
 * checks are performed.
 *
 * Selenium-style visibility checks will be performed
 * if <var>specCompat</var> is false (default).  Otherwise
 * pointer-interactability checks will be performed.  If either of these
 * fail an {@link ElementNotInteractableError} is thrown.
 *
 * If <var>strict</var> is enabled (defaults to disabled), further
 * accessibility checks will be performed, and these may result in an
 * {@link ElementNotAccessibleError} being returned.
 *
 * When <var>el</var> is not enabled, an {@link InvalidElementStateError}
 * is returned.
 *
 * @param {(DOMElement|XULElement)} el
 *     Element to click.
 * @param {boolean=} [strict=false] strict
 *     Enforce strict accessibility tests.
 * @param {boolean=} [specCompat=false] specCompat
 *     Use WebDriver specification compatible interactability definition.
 *
 * @throws {ElementNotInteractableError}
 *     If either Selenium-style visibility check or
 *     pointer-interactability check fails.
 * @throws {ElementClickInterceptedError}
 *     If <var>el</var> is obscured by another element and a click would
 *     not hit, in <var>specCompat</var> mode.
 * @throws {ElementNotAccessibleError}
 *     If <var>strict</var> is true and element is not accessible.
 * @throws {InvalidElementStateError}
 *     If <var>el</var> is not enabled.
 */
interaction.clickElement = async function(
    el, strict = false, specCompat = false) {
  const a11y = accessibility.get(strict);
  if (element.isXULElement(el)) {
    await chromeClick(el, a11y);
  } else if (specCompat) {
    await webdriverClickElement(el, a11y);
  } else {
    await seleniumClickElement(el, a11y);
  }
};

async function webdriverClickElement(el, a11y) {
  const win = getWindow(el);

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
        pprint`Element ${el} could not be scrolled into view`);
  }

  // step 7
  let rects = containerEl.getClientRects();
  let clickPoint = element.getInViewCentrePoint(rects[0], win);

  if (element.isObscured(containerEl)) {
    throw new ElementClickInterceptedError(containerEl, clickPoint);
  }

  let acc = await a11y.getAccessible(el, true);
  a11y.assertVisible(acc, el, true);
  a11y.assertEnabled(acc, el, true);
  a11y.assertActionable(acc, el);

  // step 8
  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    // step 9
    let clicked = interaction.flushEventLoop(containerEl);
    event.synthesizeMouseAtPoint(clickPoint.x, clickPoint.y, {}, win);
    await clicked;
  }

  // step 10
  // if the click causes navigation, the post-navigation checks are
  // handled by the load listener in listener.js
}

async function chromeClick(el, a11y) {
  if (!atom.isElementEnabled(el)) {
    throw new InvalidElementStateError("Element is not enabled");
  }

  let acc = await a11y.getAccessible(el, true);
  a11y.assertVisible(acc, el, true);
  a11y.assertEnabled(acc, el, true);
  a11y.assertActionable(acc, el);

  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    el.click();
  }
}

async function seleniumClickElement(el, a11y) {
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

  let acc = await a11y.getAccessible(el, true);
  a11y.assertVisible(acc, el, true);
  a11y.assertEnabled(acc, el, true);
  a11y.assertActionable(acc, el);

  if (el.localName == "option") {
    interaction.selectOption(el);
  } else {
    let rects = el.getClientRects();
    let centre = element.getInViewCentrePoint(rects[0], win);
    let opts = {};
    event.synthesizeMouseAtPoint(centre.x, centre.y, opts, win);
  }
}

/**
 * Select <tt>&lt;option&gt;</tt> element in a <tt>&lt;select&gt;</tt>
 * list.
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
 * @throws {TypeError}
 *     If <var>el</var> is a XUL element or not an <tt>&lt;option&gt;</tt>
 *     element.
 * @throws {Error}
 *     If unable to find <var>el</var>'s parent <tt>&lt;select&gt;</tt>
 *     element.
 */
interaction.selectOption = function(el) {
  if (element.isXULElement(el)) {
    throw new TypeError("XUL dropdowns not supported");
  }
  if (el.localName != "option") {
    throw new TypeError(pprint`Expected <option> element, got ${el}`);
  }

  let containerEl = element.getContainer(el);

  event.mouseover(containerEl);
  event.mousemove(containerEl);
  event.mousedown(containerEl);
  event.focus(containerEl);

  if (!el.disabled) {
    // Clicking <option> in <select> should not be deselected if selected.
    // However, clicking one in a <select multiple> should toggle
    // selectedness the way holding down Control works.
    if (containerEl.multiple) {
      el.selected = !el.selected;
    } else if (!el.selected) {
      el.selected = true;
    }
    event.input(containerEl);
    event.change(containerEl);
  }

  event.mouseup(containerEl);
  event.click(containerEl);
};

/**
 * Waits until the event loop has spun enough times to process the
 * DOM events generated by clicking an element, or until the document
 * is unloaded.
 *
 * @param {Element} el
 *     Element that is expected to receive the click.
 *
 * @return {Promise}
 *     Promise is resolved once <var>el</var> has been clicked
 *     (its <code>click</code> event fires), the document is unloaded,
 *     or a 500 ms timeout is reached.
 */
interaction.flushEventLoop = async function(el) {
  const win = el.ownerGlobal;
  let unloadEv, clickEv;

  let spinEventLoop = resolve => {
    unloadEv = resolve;
    clickEv = () => {
      if (win.closed) {
        resolve();
      } else {
        win.setTimeout(resolve, 0);
      }
    };

    win.addEventListener("unload", unloadEv, {mozSystemGroup: true});
    el.addEventListener("click", clickEv, {mozSystemGroup: true});
  };
  let removeListeners = () => {
    // only one event fires
    win.removeEventListener("unload", unloadEv);
    el.removeEventListener("click", clickEv);
  };

  return new TimedPromise(spinEventLoop, {timeout: 500, throws: null})
      .then(removeListeners);
};

/**
 * Focus element and, if a textual input field and no previous selection
 * state exists, move the caret to the end of the input field.
 *
 * @param {Element} element
 *     Element to focus.
 */
interaction.focusElement = function(el) {
  let t = el.type;
  if (t && (t == "text" || t == "textarea")) {
    if (el.selectionEnd == 0) {
      let len = el.value.length;
      el.setSelectionRange(len, len);
    }
  }
  el.focus();
};

/**
 * Performs checks if <var>el</var> is keyboard-interactable.
 *
 * To decide if an element is keyboard-interactable various properties,
 * and computed CSS styles have to be evaluated. Whereby it has to be taken
 * into account that the element can be part of a container (eg. option),
 * and as such the container has to be checked instead.
 *
 * @param {Element} el
 *     Element to check.
 *
 * @return {boolean}
 *     True if element is keyboard-interactable, false otherwise.
 */
interaction.isKeyboardInteractable = function(el) {
  const win = getWindow(el);

  // body and document element are always keyboard-interactable
  if (el.localName === "body" || el === win.document.documentElement) {
    return true;
  }

  el.focus();

  return el === win.document.activeElement;
};

/**
 * Appends <var>path</var> to an <tt>&lt;input type=file&gt;</tt>'s
 * file list.
 *
 * @param {HTMLInputElement} el
 *     An <tt>&lt;input type=file&gt;</tt> element.
 * @param {string} path
 *     Full path to file.
 *
 * @throws {InvalidArgumentError}
 *     If <var>path</var> can not be found.
 */
interaction.uploadFile = async function(el, path) {
  let file;
  try {
    file = await File.createFromFileName(path);
  } catch (e) {
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
 * Sets a form element's value.
 *
 * @param {DOMElement} el
 *     An form element, e.g. input, textarea, etc.
 * @param {string} value
 *     The value to be set.
 *
 * @throws {TypeError}
 *     If <var>el</var> is not an supported form element.
 */
interaction.setFormControlValue = function(el, value) {
  if (!COMMON_FORM_CONTROLS.has(el.localName)) {
    throw new TypeError("This function is for form elements only");
  }

  el.value = value;

  if (INPUT_TYPES_NO_EVENT.has(el.type)) {
    return;
  }

  event.input(el);
  event.change(el);
};

/**
 * Send keys to element.
 *
 * @param {DOMElement|XULElement} el
 *     Element to send key events to.
 * @param {Array.<string>} value
 *     Sequence of keystrokes to send to the element.
 * @param {boolean=} [strict=false] strict
 *     Enforce strict accessibility tests.
 * @param {boolean=} [specCompat=false] specCompat
 *     Use WebDriver specification compatible interactability definition.
 */
interaction.sendKeysToElement = async function(
    el, value, strict = false, specCompat = false) {
  const a11y = accessibility.get(strict);

  if (specCompat) {
    await webdriverSendKeysToElement(el, value, a11y);
  } else {
    await legacySendKeysToElement(el, value, a11y);
  }
};

async function webdriverSendKeysToElement(el, value, a11y) {
  const win = getWindow(el);

  let containerEl = element.getContainer(el);

  // TODO: Wait for element to be keyboard-interactible
  if (!interaction.isKeyboardInteractable(containerEl)) {
    throw new ElementNotInteractableError(
        pprint`Element ${el} is not reachable by keyboard`);
  }

  let acc = await a11y.getAccessible(el, true);
  a11y.assertActionable(acc, el);

  interaction.focusElement(el);

  if (el.type == "file") {
    await interaction.uploadFile(el, value);
  } else if ((el.type == "date" || el.type == "time") &&
      Preferences.get("dom.forms.datetime")) {
    interaction.setFormControlValue(el, value);
  } else {
    event.sendKeysToElement(value, el, win);
  }
}

async function legacySendKeysToElement(el, value, a11y) {
  const win = getWindow(el);

  if (el.type == "file") {
    await interaction.uploadFile(el, value);
  } else if ((el.type == "date" || el.type == "time") &&
      Preferences.get("dom.forms.datetime")) {
    interaction.setFormControlValue(el, value);
  } else {
    let visibilityCheckEl  = el;
    if (el.localName == "option") {
      visibilityCheckEl = element.getContainer(el);
    }

    if (!element.isVisible(visibilityCheckEl)) {
      throw new ElementNotInteractableError("Element is not visible");
    }

    let acc = await a11y.getAccessible(el, true);
    a11y.assertActionable(acc, el);

    interaction.focusElement(el);
    event.sendKeysToElement(value, el, win);
  }
}

/**
 * Determine the element displayedness of an element.
 *
 * @param {DOMElement|XULElement} el
 *     Element to determine displayedness of.
 * @param {boolean=} [strict=false] strict
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
interaction.isElementEnabled = function(el, strict = false) {
  let enabled = true;
  let win = getWindow(el);

  if (element.isXULElement(el)) {
    // check if XUL element supports disabled attribute
    if (DISABLED_ATTRIBUTE_SUPPORTED_XUL.has(el.tagName.toUpperCase())) {
      if (el.hasAttribute("disabled") && el.getAttribute("disabled") === "true") {
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
 * Determines if the referenced element is selected or not, with
 * an additional accessibility check if <var>strict</var> is true.
 *
 * This operation only makes sense on input elements of the checkbox-
 * and radio button states, and option elements.
 *
 * @param {(DOMElement|XULElement)} el
 *     Element to test if is selected.
 * @param {boolean=} [strict=false] strict
 *     Enforce strict accessibility tests.
 *
 * @return {boolean}
 *     True if element is selected, false otherwise.
 *
 * @throws {ElementNotAccessibleError}
 *     If <var>el</var> is not accessible when <var>strict</var> is true.
 */
interaction.isElementSelected = function(el, strict = false) {
  let selected = element.isSelected(el);

  let a11y = accessibility.get(strict);
  return a11y.getAccessible(el).then(acc => {
    a11y.assertSelected(acc, el, selected);
    return selected;
  });
};

function getWindow(el) {
  return el.ownerDocument.defaultView;  // eslint-disable-line
}
