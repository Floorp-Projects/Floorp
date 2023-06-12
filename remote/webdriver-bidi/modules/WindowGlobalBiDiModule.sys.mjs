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
   * @param {ExtraSerializationOptions=} extraOptions
   *     Extra Remote Value deserialization options.
   *
   * @returns {object}
   *     Deserialized representation of the value.
   */
  deserialize(realm, serializedValue, extraOptions = {}) {
    extraOptions.nodeCache = this.#nodeCache;

    return lazy.deserialize(realm, serializedValue, extraOptions);
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
   * @param {ExtraSerializationOptions} extraOptions
   *     Extra Remote Value serialization options.
   *
   * @returns {object}
   *     Promise that resolves to the serialized representation of the value.
   */
  serialize(
    value,
    serializationOptions,
    ownershipType,
    realm,
    extraOptions = {}
  ) {
    const { nodeCache = this.#nodeCache, seenNodeIds = new Map() } =
      extraOptions;

    const serializedValue = lazy.serialize(
      value,
      serializationOptions,
      ownershipType,
      new Map(),
      realm,
      { nodeCache, seenNodeIds }
    );

    return serializedValue;
  }
}
