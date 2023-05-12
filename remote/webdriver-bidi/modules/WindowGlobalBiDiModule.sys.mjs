/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  deserialize: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
  serialize: "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs",
});

/**
 * Base class for all WindowGlobal BiDi MessageHandler modules.
 */
export class WindowGlobalBiDiModule extends Module {
  get #nodeCache() {
    return this.#processActor.getNodeCache();
  }

  get #processActor() {
    return ChromeUtils.domProcessChild.getActor("WebDriverProcessData");
  }

  /**
   * Wrapper to deserialize a local / remote value.
   *
   * @param {Realm} realm
   *     The Realm in which the value is deserialized.
   * @param {object} serializedValue
   *     Value of any type to be deserialized.
   * @param {RemoteValueOptions=} options
   *     Extra Remote Value deserialization options.
   *
   * @returns {Promise<object>}
   *     Promise that resolves to the deserialized representation of the value.
   */
  deserialize(realm, serializedValue, options = {}) {
    options.getNode = this.#getNode.bind(this);

    return lazy.deserialize(realm, serializedValue, options);
  }

  /**
   * Wrapper to serialize a value as a remote value.
   *
   * @param {object} value
   *     Value of any type to be serialized.
   * @param {SerializationOptions} serializationOptions
   *     Options which define how ECMAScript objects should be serialized.
   * @param {OwnershipModel} ownershipType
   *     The ownership model to use for this serialization.
   * @param {Realm} realm
   *     The Realm from which comes the value being serialized.
   * @param {RemoteValueOptions=} options
   *     Extra Remote Value serialization options.
   *
   * @returns {Promise<object>}
   *     Promise that resolves to the serialized representation of the value.
   */
  serialize(value, serializationOptions, ownershipType, realm, options = {}) {
    options.getOrCreateNodeReference = this.#getOrCreateNodeReference.bind(
      this
    );

    return lazy.serialize(
      value,
      serializationOptions,
      ownershipType,
      new Map(),
      realm,
      options
    );
  }

  // Private methods

  /**
   * Resolve node from specified web reference identifier.
   *
   * @param {BrowsingContext} browsingContext
   *     The browsing context to retrieve the node from.
   * @param {string} nodeId
   *     The WebReference uuid for a DOM node.
   *
   * @returns {Node|null}
   *     The DOM node that the identifier was generated for, or null if the
   *     node has not been found.
   */
  #getNode(browsingContext, nodeId) {
    const node = this.#nodeCache.getNode(browsingContext, nodeId);

    if (node === null) {
      return null;
    }

    // Bug 1819902: Instead of a browsing context check compare the origin
    const isSameBrowsingContext = nodeId => {
      const nodeDetails = this.#nodeCache.getReferenceDetails(nodeId);

      if (nodeDetails.isTopBrowsingContext && browsingContext.parent === null) {
        // As long as Navigables are not available any cross-group navigation will
        // cause a swap of the current top-level browsing context. The only unique
        // identifier in such a case is the browser id the top-level browsing
        // context actually lives in.
        return nodeDetails.browserId === browsingContext.browserId;
      }

      return nodeDetails.browsingContextId === browsingContext.id;
    };

    if (!isSameBrowsingContext(nodeId)) {
      return null;
    }

    return node;
  }

  /**
   * Returns the WebReference for the given node.
   *
   * Hereby it tries to find a known node reference for that node in the
   * node cache, and returns it. Otherwise it creates a new reference and
   * adds it to the cache.
   *
   * @param {BrowsingContext} browsingContext
   *     The browsing context the node is part of.
   * @param {Node} node
   *     The node to create or get a WebReference for.
   *
   * @returns {string}
   *     The unique shared id for the node.
   */
  async #getOrCreateNodeReference(browsingContext, node) {
    const nodeId = this.#nodeCache.getOrCreateNodeReference(node);

    // Update the seen nodes map for WebDriver classic compatibility
    await this.messageHandler.sendRootCommand({
      moduleName: "script",
      commandName: "_addNodeToSeenNodes",
      params: {
        browsingContext,
        nodeId,
      },
    });

    return nodeId;
  }
}
