/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ContentDOMReference: "resource://gre/modules/ContentDOMReference.sys.mjs",

  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

/**
 * The class provides a mapping between DOM nodes and unique element
 * references by using `ContentDOMReference` identifiers.
 */
export class NodeCache {
  #domRefs;
  #sharedIds;

  constructor() {
    // ContentDOMReference id => shared unique id
    this.#sharedIds = new Map();

    // shared unique id => ContentDOMReference
    this.#domRefs = new Map();
  }

  /**
   * Get the number of elements in the cache.
   */
  get size() {
    return this.#sharedIds.size;
  }

  /**
   * Add a DOM element to the cache if not known yet.
   *
   * @param {Element} el
   *    The DOM Element to be added.
   *
   * @return {string}
   *     The shared id to uniquely identify the DOM element.
   */
  add(el) {
    let domRef, sharedId;

    try {
      // Evaluation of code will take place in mutable sandboxes, which are
      // created to waive xrays by default. As such DOM elements have to be
      // unwaived before accessing the ownerGlobal if possible, which is
      // needed by ContentDOMReference.
      domRef = lazy.ContentDOMReference.get(Cu.unwaiveXrays(el));
    } catch (e) {
      throw new lazy.error.UnknownError(
        lazy.pprint`Failed to create element reference for ${el}: ${e.message}`
      );
    }

    if (this.#sharedIds.has(domRef.id)) {
      // For already known elements retrieve the cached shared id.
      sharedId = this.#sharedIds.get(domRef.id);
    } else {
      // For new elements generate a unique id without curly braces.
      sharedId = Services.uuid
        .generateUUID()
        .toString()
        .slice(1, -1);

      this.#sharedIds.set(domRef.id, sharedId);
      this.#domRefs.set(sharedId, domRef);
    }

    return sharedId;
  }

  /**
   * Clears all known DOM elements.
   *
   * @param {Object=} options
   * @param {boolean=} options.all
   *     Clear all references from any browsing context. Defaults to false.
   * @param {BrowsingContext=} browsingContext
   *     Clear all references living in that browsing context.
   */
  clear(options = {}) {
    const { all = false, browsingContext } = options;

    if (all) {
      this.#sharedIds.clear();
      this.#domRefs.clear();
      return;
    }

    if (browsingContext) {
      for (const [sharedId, domRef] of this.#domRefs.entries()) {
        if (domRef.browsingContextId === browsingContext.id) {
          this.#sharedIds.delete(domRef.id);
          this.#domRefs.delete(sharedId);
        }
      }
      return;
    }

    throw new Error(`Requires "browsingContext" or "all" to be set.`);
  }

  /**
   * Wrapper around ContentDOMReference.resolve with additional error handling
   * specific to WebDriver.
   *
   * @param {string} sharedId
   *     The unique identifier for the DOM element.
   *
   * @return {Element|null}
   *     The DOM element that the unique identifier was generated for or
   *     `null` if the element does not exist anymore.
   *
   * @throws {NoSuchElementError}
   *     If the DOM element as represented by the unique WebElement reference
   *     <var>sharedId</var> isn't known.
   */
  resolve(sharedId) {
    const domRef = this.#domRefs.get(sharedId);
    if (domRef == undefined) {
      throw new lazy.error.NoSuchElementError(
        `Unknown element with id ${sharedId}`
      );
    }

    return lazy.ContentDOMReference.resolve(domRef);
  }
}
