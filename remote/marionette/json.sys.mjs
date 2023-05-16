/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  element: "chrome://remote/content/marionette/element.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
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
 * @param {object} value
 *     Object to be cloned.
 * @param {Set} seen
 *     List of objects already processed.
 * @param {Function} cloneAlgorithm
 *     The clone algorithm to invoke for individual list entries or object
 *     properties.
 *
 * @returns {object}
 *     The cloned object.
 */
function cloneObject(value, seen, cloneAlgorithm) {
  // Only proceed with cloning an object if it hasn't been seen yet.
  if (seen.has(value)) {
    throw new lazy.error.JavaScriptError("Cyclic object value");
  }
  seen.add(value);

  let result;

  if (lazy.element.isCollection(value)) {
    result = [...value].map(entry => cloneAlgorithm(entry, seen));
  } else {
    // arbitrary objects
    result = {};
    for (let prop in value) {
      try {
        result[prop] = cloneAlgorithm(value[prop], seen);
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
 * @param {object} value
 *     Object to be cloned.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 *
 * @returns {object}
 *     Same object as provided by `value` with the WebDriver specific
 *     elements replaced by WebReference's.
 *
 * @throws {JavaScriptError}
 *     If an object contains cyclic references.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM.
 */
json.clone = function(value, nodeCache) {
  function cloneJSON(value, seen) {
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

      const nodeRef = nodeCache.getOrCreateNodeReference(value);
      return lazy.WebReference.from(value, nodeRef).toJSON();
    }

    if (isNode && lazy.element.isShadowRoot(value)) {
      // Convert ShadowRoot instances to WebReference references.

      if (lazy.element.isDetached(value)) {
        // Don't create a reference for detached shadow roots.
        throw new lazy.error.DetachedShadowRootError(
          lazy.pprint`The ShadowRoot ${value} is no longer attached to the DOM`
        );
      }

      const nodeRef = nodeCache.getOrCreateNodeReference(value);
      return lazy.WebReference.from(value, nodeRef).toJSON();
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
 * @param {object} value
 *     Arbitrary object.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 * @param {WindowProxy} win
 *     Current window.
 *
 * @returns {object}
 *     Same object as provided by `value` with the WebDriver specific
 *     references replaced with real JavaScript objects.
 *
 * @throws {NoSuchElementError}
 *     If the WebElement reference has not been seen before.
 * @throws {StaleElementReferenceError}
 *     If the element is stale, indicating it is no longer attached to the DOM.
 */
json.deserialize = function(value, nodeCache, win) {
  function deserializeJSON(value, seen) {
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
            return lazy.element.getKnownShadowRoot(
              win.browsingContext,
              webRef.uuid,
              nodeCache
            );
          }

          if (webRef instanceof lazy.WebElement) {
            return lazy.element.getKnownElement(
              win.browsingContext,
              webRef.uuid,
              nodeCache
            );
          }

          // WebFrame and WebWindow not supported yet
          throw new lazy.error.UnsupportedOperationError();
        }

        return cloneObject(value, seen, deserializeJSON);
    }
  }

  return deserializeJSON(value, new Set());
};
