/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  element: "chrome://remote/content/shared/webdriver/Element.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  ShadowRoot: "chrome://remote/content/marionette/web-reference.sys.mjs",
  WebElement: "chrome://remote/content/marionette/web-reference.sys.mjs",
  WebReference: "chrome://remote/content/marionette/web-reference.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.MARIONETTE)
);

/** @namespace */
export const json = {};

/**
 * Clone an object including collections.
 *
 * @param {object} value
 *     Object to be cloned.
 * @param {Set} seen
 *     List of objects already processed.
 * @param {Function} cloneAlgorithm
 *     The clone algorithm to invoke for individual list entries or object
 *     properties.
 *
 * @returns {Promise<object>}
 *     A promise that resolves to the cloned object.
 */
async function cloneObject(value, seen, cloneAlgorithm) {
  // Only proceed with cloning an object if it hasn't been seen yet.
  if (seen.has(value)) {
    throw new lazy.error.JavaScriptError("Cyclic object value");
  }
  seen.add(value);

  let result;

  if (lazy.element.isCollection(value)) {
    result = await Promise.all(
      [...value].map(entry => cloneAlgorithm(entry, seen))
    );
  } else {
    // arbitrary objects
    result = {};
    for (let prop in value) {
      try {
        result[prop] = await cloneAlgorithm(value[prop], seen);
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_IMPLEMENTED) {
          lazy.logger.debug(`Skipping ${prop}: ${e.message}`);
        } else {
          throw e;
        }
      }
    }
  }

  seen.delete(value);

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
 * - If a cyclic references is detected a JavaScriptError is thrown.
 *
 * @param {object} options
 * @param {BrowsingContext} options.browsingContext
 *     Current browsing context.
 * @param {Function} options.getOrCreateNodeReference
 *     Callback that tries to use a known node reference from the node cache,
 *     or creates a new one if not present yet.
 * @param {object} options.value
 *     Object to be cloned.
 *
 * @returns {Promise<object>}
 *     Promise that resolves to the same object as provided by `value` with
 *     the WebDriver specific elements replaced by WebReference's.
 *
 * @throws {JavaScriptError}
 *     If an object contains cyclic references.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM.
 */
json.clone = async function(options) {
  const { browsingContext, getOrCreateNodeReference, value } = options;

  if (typeof browsingContext === "undefined") {
    throw new TypeError("Browsing context not specified");
  }

  if (typeof getOrCreateNodeReference !== "function") {
    throw new TypeError("Invalid callback for 'getOrCreateNodeReference'");
  }

  async function cloneJSON(value, seen) {
    if (seen === undefined) {
      seen = new Set();
    }

    if ([undefined, null].includes(value)) {
      return null;
    }

    const type = typeof value;

    if (["boolean", "number", "string"].includes(type)) {
      // Primitive values
      return value;
    }

    // Evaluation of code might take place in mutable sandboxes, which are
    // created to waive XRays by default. As such DOM nodes would have to be
    // unwaived before accessing properties like "ownerGlobal" is possible.
    //
    // Until bug 1743788 is fixed there might be the possibility that more
    // objects might need to be unwaived as well.
    const isNode = Node.isInstance(value);
    if (isNode) {
      value = Cu.unwaiveXrays(value);
    }

    if (isNode && lazy.element.isElement(value)) {
      // Convert DOM elements to WebReference instances.

      if (lazy.element.isStale(value)) {
        // Don't create a reference for stale elements.
        throw new lazy.error.StaleElementReferenceError(
          lazy.pprint`The element ${value} is no longer attached to the DOM`
        );
      }

      const ref = await getOrCreateNodeReference(browsingContext, value);
      return ref.toJSON();
    }

    if (isNode && lazy.element.isShadowRoot(value)) {
      // Convert ShadowRoot instances to WebReference references.

      if (lazy.element.isDetached(value)) {
        // Don't create a reference for detached shadow roots.
        throw new lazy.error.DetachedShadowRootError(
          lazy.pprint`The ShadowRoot ${value} is no longer attached to the DOM`
        );
      }

      const ref = await getOrCreateNodeReference(browsingContext, value);
      return ref.toJSON();
    }

    if (typeof value.toJSON == "function") {
      // custom JSON representation
      let unsafeJSON;
      try {
        unsafeJSON = value.toJSON();
      } catch (e) {
        throw new lazy.error.JavaScriptError(`toJSON() failed with: ${e}`);
      }

      return cloneJSON(unsafeJSON, seen);
    }

    // Collections and arbitrary objects
    return cloneObject(value, seen, cloneJSON);
  }

  return cloneJSON(value, new Set());
};

/**
 * Deserialize an arbitrary object.
 *
 * @param {object} options
 * @param {BrowsingContext} options.browsingContext
 *     Current browsing context.
 * @param {Function} options.getKnownElement
 *     Callback that will try to resolve a WebElement reference to an Element node.
 * @param {Function} options.getKnownShadowRoot
 *     Callback that will try to resolve a ShadowRoot reference to a ShadowRoot node.
 * @param {object} options.value
 *     Arbitrary object.
 *
 * @returns {Promise<object>}
 *     Promise that resolves to the same object as provided by `value` with the
 *     WebDriver specific references replaced with real JavaScript objects.
 *
 * @throws {DetachedShadowRootError}
 *     If the ShadowRoot is detached, indicating it is no longer attached to the DOM.
 * @throws {NoSuchElementError}
 *     If the WebElement reference has not been seen before.
 * @throws {NoSuchShadowRootError}
 *     If the ShadowRoot reference has not been seen before.
 * @throws {StaleElementReferenceError}
 *     If the element is stale, indicating it is no longer attached to the DOM.
 */
json.deserialize = async function(options) {
  const {
    browsingContext,
    getKnownElement,
    getKnownShadowRoot,
    value,
  } = options;

  if (typeof browsingContext === "undefined") {
    throw new TypeError("Browsing context not specified");
  }

  if (typeof getKnownElement !== "function") {
    throw new TypeError("Invalid callback for 'getKnownElement'");
  }

  if (typeof getKnownShadowRoot !== "function") {
    throw new TypeError("Invalid callback for 'getKnownShadowRoot'");
  }

  async function deserializeJSON(value, seen) {
    if (seen === undefined) {
      seen = new Set();
    }

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

          if (webRef instanceof lazy.ShadowRoot) {
            return getKnownShadowRoot(browsingContext, webRef.uuid);
          }

          if (webRef instanceof lazy.WebElement) {
            return getKnownElement(browsingContext, webRef.uuid);
          }

          // WebFrame and WebWindow not supported yet
          throw new lazy.error.UnsupportedOperationError();
        }

        return cloneObject(value, seen, deserializeJSON);
    }
  }

  return deserializeJSON(value, new Set());
};
