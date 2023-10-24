/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { WebFrame, WebWindow } from "./web-reference.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
  ShadowRoot: "chrome://remote/content/marionette/web-reference.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  WebElement: "chrome://remote/content/marionette/web-reference.sys.mjs",
  WebFrame: "chrome://remote/content/marionette/web-reference.sys.mjs",
  WebReference: "chrome://remote/content/marionette/web-reference.sys.mjs",
  WebWindow: "chrome://remote/content/marionette/web-reference.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
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

  if (lazy.dom.isCollection(value)) {
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
 * @returns {Object<Map<BrowsingContext, Array<string>, object>>}
 *     Object that contains a list of browsing contexts each with a list of
 *     shared ids for collected elements and shadow root nodes, and second the
 *     same object as provided by `value` with the WebDriver classic supported
 *     DOM nodes replaced by WebReference's.
 *
 * @throws {JavaScriptError}
 *     If an object contains cyclic references.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM.
 */
json.clone = function (value, nodeCache) {
  const seenNodeIds = new Map();
  let hasSerializedWindows = false;

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
    // created to waive XRays by default. As such DOM nodes and windows
    // have to be unwaived before accessing properties like "ownerGlobal"
    // is possible.
    //
    // Until bug 1743788 is fixed there might be the possibility that more
    // objects might need to be unwaived as well.
    const isNode = Node.isInstance(value);
    const isWindow = Window.isInstance(value);
    if (isNode || isWindow) {
      value = Cu.unwaiveXrays(value);
    }

    if (isNode && lazy.dom.isElement(value)) {
      // Convert DOM elements to WebReference instances.

      if (lazy.dom.isStale(value)) {
        // Don't create a reference for stale elements.
        throw new lazy.error.StaleElementReferenceError(
          lazy.pprint`The element ${value} is no longer attached to the DOM`
        );
      }

      const nodeRef = nodeCache.getOrCreateNodeReference(value, seenNodeIds);

      return lazy.WebReference.from(value, nodeRef).toJSON();
    }

    if (isNode && lazy.dom.isShadowRoot(value)) {
      // Convert ShadowRoot instances to WebReference references.

      if (lazy.dom.isDetached(value)) {
        // Don't create a reference for detached shadow roots.
        throw new lazy.error.DetachedShadowRootError(
          lazy.pprint`The ShadowRoot ${value} is no longer attached to the DOM`
        );
      }

      const nodeRef = nodeCache.getOrCreateNodeReference(value, seenNodeIds);

      return lazy.WebReference.from(value, nodeRef).toJSON();
    }

    if (isWindow) {
      // Convert window instances to WebReference references.
      let reference;

      if (value.browsingContext.parent == null) {
        reference = new WebWindow(value.browsingContext.browserId.toString());
        hasSerializedWindows = true;
      } else {
        reference = new WebFrame(value.browsingContext.id.toString());
      }

      return reference.toJSON();
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

  return {
    seenNodeIds,
    serializedValue: cloneJSON(value, new Set()),
    hasSerializedWindows,
  };
};

/**
 * Deserialize an arbitrary object.
 *
 * @param {object} value
 *     Arbitrary object.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 * @param {BrowsingContext} browsingContext
 *     The browsing context to check.
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
json.deserialize = function (value, nodeCache, browsingContext) {
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
            return getKnownShadowRoot(browsingContext, webRef.uuid, nodeCache);
          }

          if (webRef instanceof lazy.WebElement) {
            return getKnownElement(browsingContext, webRef.uuid, nodeCache);
          }

          if (webRef instanceof lazy.WebFrame) {
            const browsingContext = BrowsingContext.get(webRef.uuid);

            if (browsingContext === null || browsingContext.parent === null) {
              throw new lazy.error.NoSuchWindowError(
                `Unable to locate frame with id: ${webRef.uuid}`
              );
            }

            return browsingContext.window;
          }

          if (webRef instanceof lazy.WebWindow) {
            const browsingContext = BrowsingContext.getCurrentTopByBrowserId(
              webRef.uuid
            );

            if (browsingContext === null) {
              throw new lazy.error.NoSuchWindowError(
                `Unable to locate window with id: ${webRef.uuid}`
              );
            }

            return browsingContext.window;
          }
        }

        return cloneObject(value, seen, deserializeJSON);
    }
  }

  return deserializeJSON(value, new Set());
};

/**
 * Convert unique navigable ids to internal browser ids.
 *
 * @param {object} serializedData
 *     The data to process.
 *
 * @returns {object}
 *     The processed data.
 */
json.mapFromNavigableIds = function (serializedData) {
  function _processData(data) {
    if (lazy.WebReference.isReference(data)) {
      const webRef = lazy.WebReference.fromJSON(data);

      if (webRef instanceof lazy.WebWindow) {
        const browser = lazy.TabManager.getBrowserById(webRef.uuid);
        if (browser) {
          webRef.uuid = browser?.browserId.toString();
          data = webRef.toJSON();
        }
      }
    } else if (typeof data === "object") {
      for (const entry in data) {
        data[entry] = _processData(data[entry]);
      }
    }

    return data;
  }

  return _processData(serializedData);
};

/**
 * Convert browser ids to unique navigable ids.
 *
 * @param {object} serializedData
 *     The data to process.
 *
 * @returns {object}
 *     The processed data.
 */
json.mapToNavigableIds = function (serializedData) {
  function _processData(data) {
    if (lazy.WebReference.isReference(data)) {
      const webRef = lazy.WebReference.fromJSON(data);
      if (webRef instanceof lazy.WebWindow) {
        const browsingContext = BrowsingContext.getCurrentTopByBrowserId(
          webRef.uuid
        );

        webRef.uuid = lazy.TabManager.getIdForBrowsingContext(browsingContext);
        data = webRef.toJSON();
      }
    } else if (typeof data == "object") {
      for (const entry in data) {
        data[entry] = _processData(data[entry]);
      }
    }

    return data;
  }

  return _processData(serializedData);
};

/**
 * Resolve element from specified web reference identifier.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to retrieve the element from.
 * @param {string} nodeId
 *     The WebReference uuid for a DOM element.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 *
 * @returns {Element}
 *     The DOM element that the identifier was generated for.
 *
 * @throws {NoSuchElementError}
 *     If the element doesn't exist in the current browsing context.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating its node document is no
 *     longer the active document or it is no longer attached to the DOM.
 */
export function getKnownElement(browsingContext, nodeId, nodeCache) {
  if (!isNodeReferenceKnown(browsingContext, nodeId, nodeCache)) {
    throw new lazy.error.NoSuchElementError(
      `The element with the reference ${nodeId} is not known in the current browsing context`,
      { elementId: nodeId }
    );
  }

  const node = nodeCache.getNode(browsingContext, nodeId);

  // Ensure the node is of the correct Node type.
  if (node !== null && !lazy.dom.isElement(node)) {
    throw new lazy.error.NoSuchElementError(
      `The element with the reference ${nodeId} is not of type HTMLElement`
    );
  }

  // If null, which may be the case if the element has been unwrapped from a
  // weak reference, it is always considered stale.
  if (node === null || lazy.dom.isStale(node)) {
    throw new lazy.error.StaleElementReferenceError(
      `The element with the reference ${nodeId} ` +
        "is stale; either its node document is not the active document, " +
        "or it is no longer connected to the DOM"
    );
  }

  return node;
}

/**
 * Resolve ShadowRoot from specified web reference identifier.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to retrieve the shadow root from.
 * @param {string} nodeId
 *     The WebReference uuid for a ShadowRoot.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 *
 * @returns {ShadowRoot}
 *     The ShadowRoot that the identifier was generated for.
 *
 * @throws {NoSuchShadowRootError}
 *     If the ShadowRoot doesn't exist in the current browsing context.
 * @throws {DetachedShadowRootError}
 *     If the ShadowRoot is detached, indicating its node document is no
 *     longer the active document or it is no longer attached to the DOM.
 */
export function getKnownShadowRoot(browsingContext, nodeId, nodeCache) {
  if (!isNodeReferenceKnown(browsingContext, nodeId, nodeCache)) {
    throw new lazy.error.NoSuchShadowRootError(
      `The shadow root with the reference ${nodeId} is not known in the current browsing context`,
      { shadowId: nodeId }
    );
  }

  const node = nodeCache.getNode(browsingContext, nodeId);

  // Ensure the node is of the correct Node type.
  if (node !== null && !lazy.dom.isShadowRoot(node)) {
    throw new lazy.error.NoSuchShadowRootError(
      `The shadow root with the reference ${nodeId} is not of type ShadowRoot`
    );
  }

  // If null, which may be the case if the element has been unwrapped from a
  // weak reference, it is always considered stale.
  if (node === null || lazy.dom.isDetached(node)) {
    throw new lazy.error.DetachedShadowRootError(
      `The shadow root with the reference ${nodeId} ` +
        "is detached; either its node document is not the active document, " +
        "or it is no longer connected to the DOM"
    );
  }

  return node;
}

/**
 * Determines if the node reference is known for the given browsing context.
 *
 * For WebDriver classic only nodes from the same browsing context are
 * allowed to be accessed.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context the element has to be part of.
 * @param {ElementIdentifier} nodeId
 *     The WebElement reference identifier for a DOM element.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen node references.
 *
 * @returns {boolean}
 *     True if the element is known in the given browsing context.
 */
function isNodeReferenceKnown(browsingContext, nodeId, nodeCache) {
  const nodeDetails = nodeCache.getReferenceDetails(nodeId);
  if (nodeDetails === null) {
    return false;
  }

  if (nodeDetails.isTopBrowsingContext) {
    // As long as Navigables are not available any cross-group navigation will
    // cause a swap of the current top-level browsing context. The only unique
    // identifier in such a case is the browser id the top-level browsing
    // context actually lives in.
    return nodeDetails.browserId === browsingContext.browserId;
  }

  return nodeDetails.browsingContextId === browsingContext.id;
}
