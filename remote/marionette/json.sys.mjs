/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  element: "chrome://remote/content/marionette/element.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  evaluate: "chrome://remote/content/marionette/evaluate.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  ShadowRoot: "chrome://remote/content/marionette/element.sys.mjs",
  WebElement: "chrome://remote/content/marionette/element.sys.mjs",
  WebReference: "chrome://remote/content/marionette/element.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

/** @namespace */
export const json = {};

/**
 * Clone an object including collections.
 *
 * @param {Object} value
 *     Object to be cloned.
 * @param {NodeCache} seenEls
 *     Known node cache to look up WebElement and ShadowRoot instances from.
 * @param {Function} cloneAlgorithm
 *     The clone algorithm to invoke for individual list entries or object
 *     properties.
 *
 * @return {Object}
 *     The cloned object.
 */
function cloneObject(value, seenEls, cloneAlgorithm) {
  if (lazy.element.isCollection(value)) {
    lazy.evaluate.assertAcyclic(value);
    return [...value].map(entry => cloneAlgorithm(entry, seenEls));
  }

  // arbitrary objects
  let result = {};
  for (let prop in value) {
    lazy.evaluate.assertAcyclic(value[prop]);

    try {
      result[prop] = cloneAlgorithm(value[prop], seenEls);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
        lazy.logger.debug(`Skipping ${prop}: ${e.message}`);
      } else {
        throw e;
      }
    }
  }

  return result;
}

/**
 * Clone arbitrary objects to JSON-safe primitives that can be
 * transported across processes and over the Marionette protocol.
 *
 * The marshaling rules are as follows:
 *
 * - Primitives are returned as is.
 *
 * - Collections, such as `Array`, `NodeList`, `HTMLCollection`
 *   et al. are transformed to arrays and then recursed.
 *
 * - Elements and ShadowRoots that are not known WebReference's are added to
 *   the `NodeCache`. For both the associated unique web reference identifier
 *   is returned.
 *
 * - Objects with custom JSON representations, i.e. if they have
 *   a callable `toJSON` function, are returned verbatim.  This means
 *   their internal integrity _are not_ checked.  Be careful.
 *
 * -  Other arbitrary objects are first tested for cyclic references
 *    and then recursed into.
 *
 * @param {Object} value
 *     Object to be cloned.
 * @param {NodeCache} seenEls
 *     Known node cache to look up WebElement and ShadowRoot instances from.
 *
 * @return {Object}
 *     Same object as provided by `value` with the WebDriver specific
 *     elements replaced by WebReference's.
 *
 * @throws {JavaScriptError}
 *     If an object contains cyclic references.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM.
 */
json.clone = function(value, seenEls) {
  function cloneJSON(value, seenEls) {
    const type = typeof value;

    if ([undefined, null].includes(value)) {
      return null;
    } else if (["boolean", "number", "string"].includes(type)) {
      // Primitive values
      return value;
    } else if (
      lazy.element.isElement(value) ||
      lazy.element.isShadowRoot(value)
    ) {
      // Convert DOM elements (eg. HTMLElement, XULElement, et al) and
      // ShadowRoot instances to WebReference references.

      // Evaluation of code will take place in mutable sandboxes, which are
      // created to waive xrays by default. As such DOM nodes have to be unwaived
      // before accessing the ownerGlobal is possible, which is needed by
      // ContentDOMReference.
      const el = Cu.unwaiveXrays(value);

      // Don't create a reference for stale elements.
      if (lazy.element.isStale(el)) {
        throw new lazy.error.StaleElementReferenceError(
          lazy.pprint`The element ${el} is no longer attached to the DOM`
        );
      }

      const sharedId = seenEls.add(el);
      return lazy.WebReference.from(el, sharedId).toJSON();
    } else if (typeof value.toJSON == "function") {
      // custom JSON representation
      let unsafeJSON;
      try {
        unsafeJSON = value.toJSON();
      } catch (e) {
        throw new lazy.error.JavaScriptError(`toJSON() failed with: ${e}`);
      }
      return cloneJSON(unsafeJSON, seenEls);
    }

    // Collections and arbitrary objects
    return cloneObject(value, seenEls, cloneJSON);
  }

  return cloneJSON(value, seenEls);
};

/**
 * Deserialize an arbitrary object.
 *
 * @param {Object} value
 *     Arbitrary object.
 * @param {NodeCache} seenEls
 *     Known node cache to look up WebElement and ShadowRoot instances from.
 * @param {WindowProxy} win
 *     Current window.
 *
 * @return {Object}
 *     Same object as provided by `value` with the WebDriver specific
 *     references replaced with real JavaScript objects.
 *
 * @throws {NoSuchElementError}
 *     If the WebElement reference has not been seen before.
 * @throws {StaleElementReferenceError}
 *     If the element is stale, indicating it is no longer attached to the DOM.
 */
json.deserialize = function(value, seenEls, win) {
  function deserializeJSON(value, seenEls) {
    if (value === undefined || value === null) {
      return value;
    }

    switch (typeof value) {
      case "boolean":
      case "number":
      case "string":
      default:
        return value;

      case "object":
        if (lazy.WebReference.isReference(value)) {
          // Create a WebReference based on the WebElement identifier.
          const webRef = lazy.WebReference.fromJSON(value);

          if (
            webRef instanceof lazy.WebElement ||
            webRef instanceof lazy.ShadowRoot
          ) {
            return lazy.element.resolveElement(webRef.uuid, win, seenEls);
          }

          // WebFrame and WebWindow not supported yet
          throw new lazy.error.UnsupportedOperationError();
        }

        return cloneObject(value, seenEls, deserializeJSON);
    }
  }

  return deserializeJSON(value, seenEls);
};
