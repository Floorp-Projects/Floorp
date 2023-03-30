/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

/**
 * @typedef {Object} OwnershipModel
 **/

/**
 * Enum of ownership models supported by the serialization.
 *
 * @readonly
 * @enum {OwnershipModel}
 **/
export const OwnershipModel = {
  None: "none",
  Root: "root",
};

function getUUID() {
  return Services.uuid
    .generateUUID()
    .toString()
    .slice(1, -1);
}

const TYPED_ARRAY_CLASSES = [
  "Uint8Array",
  "Uint8ClampedArray",
  "Uint16Array",
  "Uint32Array",
  "Int8Array",
  "Int16Array",
  "Int32Array",
  "Float32Array",
  "Float64Array",
  "BigInt64Array",
  "BigUint64Array",
];

/**
 * Build the serialized RemoteValue.
 *
 * @return {Object}
 *     An object with a mandatory `type` property, and optional `handle`,
 *     depending on the OwnershipModel, used for the serialization and
 *     on the value's type.
 */
function buildSerialized(type, handle = null) {
  const serialized = { type };

  if (handle !== null) {
    serialized.handle = handle;
  }

  return serialized;
}

/**
 * Helper to validate if a date string follows Date Time String format.
 *
 * @see https://tc39.es/ecma262/#sec-date-time-string-format
 *
 * @param {string} dateString
 *     String which needs to be validated.
 *
 * @throws {InvalidArgumentError}
 *     If <var>dateString</var> doesn't follow the format.
 */
function checkDateTimeString(dateString) {
  // Check if a date string follows a simplification of
  // the ISO 8601 calendar date extended format.
  const expandedYear = "[+-]\\d{6}";
  const year = "\\d{4}";
  const YYYY = `${expandedYear}|${year}`;
  const MM = "\\d{2}";
  const DD = "\\d{2}";
  const date = `${YYYY}(?:-${MM})?(?:-${DD})?`;
  const HH_mm = "\\d{2}:\\d{2}";
  const SS = "\\d{2}";
  const sss = "\\d{3}";
  const TZ = `Z|[+-]${HH_mm}`;
  const time = `T${HH_mm}(?::${SS}(?:\\.${sss})?(?:${TZ})?)?`;
  const iso8601Format = new RegExp(`^${date}(?:${time})?$`);

  // Check also if a date string is a valid date.
  if (Number.isNaN(Date.parse(dateString)) || !iso8601Format.test(dateString)) {
    throw new lazy.error.InvalidArgumentError(
      `Expected "value" for Date to be a Date Time string, got ${dateString}`
    );
  }
}

/**
 * Helper to deserialize value list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#deserialize-value-list
 *
 * @param {Realm} realm
 *     The Realm in which the value is deserialized.
 * @param {Array} serializedValueList
 *     List of serialized values.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Array} List of deserialized values.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedValueList</var> is not an array.
 */
function deserializeValueList(realm, serializedValueList, options = {}) {
  lazy.assert.array(
    serializedValueList,
    `Expected "serializedValueList" to be an array, got ${serializedValueList}`
  );

  const deserializedValues = [];

  for (const item of serializedValueList) {
    deserializedValues.push(deserialize(realm, item, options));
  }

  return deserializedValues;
}

/**
 * Helper to deserialize key-value list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#deserialize-key-value-list
 *
 * @param {Realm} realm
 *     The Realm in which the value is deserialized.
 * @param {Array} serializedKeyValueList
 *     List of serialized key-value.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Array} List of deserialized key-value.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedKeyValueList</var> is not an array or
 *     not an array of key-value arrays.
 */
function deserializeKeyValueList(realm, serializedKeyValueList, options = {}) {
  lazy.assert.array(
    serializedKeyValueList,
    `Expected "serializedKeyValueList" to be an array, got ${serializedKeyValueList}`
  );

  const deserializedKeyValueList = [];

  for (const serializedKeyValue of serializedKeyValueList) {
    if (!Array.isArray(serializedKeyValue) || serializedKeyValue.length != 2) {
      throw new lazy.error.InvalidArgumentError(
        `Expected key-value pair to be an array with 2 elements, got ${serializedKeyValue}`
      );
    }
    const [serializedKey, serializedValue] = serializedKeyValue;
    const deserializedKey =
      typeof serializedKey == "string"
        ? serializedKey
        : deserialize(realm, serializedKey, options);
    const deserializedValue = deserialize(realm, serializedValue, options);

    deserializedKeyValueList.push([deserializedKey, deserializedValue]);
  }

  return deserializedKeyValueList;
}

/**
 * Deserialize a Node as referenced by the shared unique reference.
 *
 * This unique reference can be shared by WebDriver clients with the WebDriver
 * classic implementation (Marionette) if the reference is for an Element or
 * ShadowRoot.
 *
 * @param {string} sharedRef
 *     Shared unique reference of the Node.
 * @param {Realm} realm
 *     The Realm in which the value is deserialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Node} The deserialized DOM node.
 */
function deserializeSharedReference(sharedRef, realm, options = {}) {
  const { nodeCache } = options;

  const browsingContext = realm.browsingContext;
  if (!browsingContext) {
    throw new lazy.error.NoSuchNodeError("Realm isn't a Window global");
  }

  const node = nodeCache.getNode(browsingContext, sharedRef);

  if (node === null) {
    throw new lazy.error.NoSuchNodeError(
      `The node with the reference ${sharedRef} is not known`
    );
  }

  // Bug 1819902: Instead of a browsing context check compare the origin
  const isSameBrowsingContext = sharedRef => {
    const nodeDetails = nodeCache.getReferenceDetails(sharedRef);

    if (nodeDetails.isTopBrowsingContext && browsingContext.parent === null) {
      // As long as Navigables are not available any cross-group navigation will
      // cause a swap of the current top-level browsing context. The only unique
      // identifier in such a case is the browser id the top-level browsing
      // context actually lives in.
      return nodeDetails.browserId === browsingContext.browserId;
    }

    return nodeDetails.browsingContextId === browsingContext.id;
  };

  if (!isSameBrowsingContext(sharedRef)) {
    return null;
  }

  return node;
}

/**
 * Deserialize a local value.
 *
 * @see https://w3c.github.io/webdriver-bidi/#deserialize-local-value
 *
 * @param {Realm} realm
 *     The Realm in which the value is deserialized.
 * @param {Object} serializedValue
 *     Value of any type to be deserialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Object} Deserialized representation of the value.
 */
export function deserialize(realm, serializedValue, options = {}) {
  const { handle, sharedId, type, value } = serializedValue;

  // With a shared id present deserialize as node reference.
  if (sharedId !== undefined) {
    lazy.assert.string(
      sharedId,
      `Expected "sharedId" to be a string, got ${sharedId}`
    );

    return deserializeSharedReference(sharedId, realm, options);
  }

  // With a handle present deserialize as remote reference.
  if (handle !== undefined) {
    lazy.assert.string(
      handle,
      `Expected "handle" to be a string, got ${handle}`
    );

    const object = realm.getObjectForHandle(handle);
    if (!object) {
      throw new lazy.error.InvalidArgumentError(
        `Unable to find an object reference for "handle" ${handle}`
      );
    }

    return object;
  }

  lazy.assert.string(type, `Expected "type" to be a string, got ${type}`);

  // Primitive protocol values
  switch (type) {
    case "undefined":
      return undefined;
    case "null":
      return null;
    case "string":
      lazy.assert.string(
        value,
        `Expected "value" to be a string, got ${value}`
      );
      return value;
    case "number":
      // If value is already a number return its value.
      if (typeof value === "number") {
        return value;
      }

      // Otherwise it has to be one of the special strings
      lazy.assert.in(
        value,
        ["NaN", "-0", "Infinity", "-Infinity"],
        `Expected "value" to be one of "NaN", "-0", "Infinity", "-Infinity", got ${value}`
      );
      return Number(value);
    case "boolean":
      lazy.assert.boolean(
        value,
        `Expected "value" to be a boolean, got ${value}`
      );
      return value;
    case "bigint":
      lazy.assert.string(
        value,
        `Expected "value" to be a string, got ${value}`
      );
      try {
        return BigInt(value);
      } catch (e) {
        throw new lazy.error.InvalidArgumentError(
          `Failed to deserialize value as BigInt: ${value}`
        );
      }

    // Non-primitive protocol values
    case "array":
      const array = realm.cloneIntoRealm([]);
      deserializeValueList(realm, value, options).forEach(v => array.push(v));
      return array;
    case "date":
      // We want to support only Date Time String format,
      // check if the value follows it.
      checkDateTimeString(value);

      return realm.cloneIntoRealm(new Date(value));
    case "map":
      const map = realm.cloneIntoRealm(new Map());
      deserializeKeyValueList(realm, value, options).forEach(([k, v]) =>
        map.set(k, v)
      );

      return map;
    case "object":
      const object = realm.cloneIntoRealm({});
      deserializeKeyValueList(realm, value, options).forEach(
        ([k, v]) => (object[k] = v)
      );
      return object;
    case "regexp":
      lazy.assert.object(
        value,
        `Expected "value" for RegExp to be an object, got ${value}`
      );
      const { pattern, flags } = value;
      lazy.assert.string(
        pattern,
        `Expected "pattern" for RegExp to be a string, got ${pattern}`
      );
      if (flags !== undefined) {
        lazy.assert.string(
          flags,
          `Expected "flags" for RegExp to be a string, got ${flags}`
        );
      }
      try {
        return realm.cloneIntoRealm(new RegExp(pattern, flags));
      } catch (e) {
        throw new lazy.error.InvalidArgumentError(
          `Failed to deserialize value as RegExp: ${value}`
        );
      }
    case "set":
      const set = realm.cloneIntoRealm(new Set());
      deserializeValueList(realm, value, options).forEach(v => set.add(v));
      return set;
  }

  lazy.logger.warn(`Unsupported type for local value ${type}`);
  return undefined;
}

/**
 * Helper to retrieve the handle id for a given object, for the provided realm
 * and ownership type.
 *
 * See https://w3c.github.io/webdriver-bidi/#handle-for-an-object
 *
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Object} object
 *     The object being serialized.
 *
 * @return {string} The unique handle id for the object. Will be null if the
 *     Ownership type is "none".
 */
function getHandleForObject(realm, ownershipType, object) {
  if (ownershipType === OwnershipModel.None) {
    return null;
  }
  return realm.getHandleForObject(object);
}

/**
 * Gets or creates a new shared unique reference for the DOM node.
 *
 * This unique reference can be shared by WebDriver clients with the WebDriver
 * classic implementation (Marionette) if the reference is for an Element or
 * ShadowRoot.
 *
 * @param {Node} node
 *    Node to create the unique reference for.
 * @param {Realm} realm
 *     The Realm in which the value is serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @returns {string}
 *    Shared unique reference for the Node.
 */
function getSharedIdForNode(node, realm, options = {}) {
  const { nodeCache } = options;

  if (!Node.isInstance(node)) {
    return null;
  }

  const browsingContext = realm.browsingContext;
  if (!browsingContext) {
    return null;
  }

  const unwrapped = Cu.unwaiveXrays(node);
  return nodeCache.getOrCreateNodeReference(unwrapped);
}

/**
 * Determines if <var>node</var> is shadow root.
 *
 * @param {Node} node
 *    Node to check.
 *
 * @returns {boolean}
 *    True if <var>node</var> is shadow root, false otherwise.
 */
function isShadowRoot(node) {
  const DOCUMENT_FRAGMENT_NODE = 11;
  return (
    node &&
    node.nodeType === DOCUMENT_FRAGMENT_NODE &&
    node.containingShadowRoot == node
  );
}

/**
 * Helper to serialize an Array-like object.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-an-array-like
 *
 * @param {string} production
 *     Type of object
 * @param {string} handleId
 *     The unique id of the <var>value</var>.
 * @param {boolean} knownObject
 *     Indicates if the <var>value</var> has already been serialized.
 * @param {Object} value
 *     The Array-like object to serialize.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Object} Object for serialized values.
 */
function serializeArrayLike(
  production,
  handleId,
  knownObject,
  value,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm,
  options
) {
  const serialized = buildSerialized(production, handleId);
  setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

  if (!knownObject && maxDepth !== null && maxDepth > 0) {
    serialized.value = serializeList(
      value,
      maxDepth,
      ownershipType,
      serializationInternalMap,
      realm,
      options
    );
  }

  return serialized;
}

/**
 * Helper to serialize as a list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-list
 *
 * @param {Iterable} iterable
 *     List of values to be serialized.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Array} List of serialized values.
 */
function serializeList(
  iterable,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm,
  options
) {
  const serialized = [];
  const childDepth = maxDepth !== null ? maxDepth - 1 : null;

  for (const item of iterable) {
    serialized.push(
      serialize(
        item,
        childDepth,
        ownershipType,
        serializationInternalMap,
        realm,
        options
      )
    );
  }

  return serialized;
}

/**
 * Helper to serialize as a mapping.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-mapping
 *
 * @param {Iterable} iterable
 *     List of values to be serialized.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Array} List of serialized values.
 */
function serializeMapping(
  iterable,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm,
  options
) {
  const serialized = [];
  const childDepth = maxDepth !== null ? maxDepth - 1 : null;

  for (const [key, item] of iterable) {
    const serializedKey =
      typeof key == "string"
        ? key
        : serialize(
            key,
            childDepth,
            ownershipType,
            serializationInternalMap,
            realm,
            options
          );
    const serializedValue = serialize(
      item,
      childDepth,
      ownershipType,
      serializationInternalMap,
      realm,
      options
    );

    serialized.push([serializedKey, serializedValue]);
  }

  return serialized;
}

/**
 * Helper to serialize as a Node.
 *
 * @param {Node} node
 *     Node to be serialized.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Object} Serialized value.
 */
function serializeNode(
  node,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm,
  options
) {
  const isAttribute = Attr.isInstance(node);
  const isElement = Element.isInstance(node);

  const serialized = {
    nodeType: node.nodeType,
  };

  if (node.nodeValue !== null) {
    serialized.nodeValue = node.nodeValue;
  }

  if (isElement || isAttribute) {
    serialized.localName = node.localName;
    serialized.namespaceURI = node.namespaceURI;
  }

  serialized.childNodeCount = node.childNodes.length;

  // Bug 1824953: Add support for shadow root children serialization.
  if (maxDepth !== 0 && !isShadowRoot(node)) {
    const children = [];
    const childDepth = maxDepth !== null ? maxDepth - 1 : null;
    for (const child of node.childNodes) {
      children.push(
        serialize(
          child,
          childDepth,
          ownershipType,
          serializationInternalMap,
          realm,
          options
        )
      );
    }

    serialized.children = children;
  }

  if (isElement) {
    serialized.attributes = [...node.attributes].reduce((map, attr) => {
      map[attr.name] = attr.value;
      return map;
    }, {});

    const shadowRoot = Cu.unwaiveXrays(node).openOrClosedShadowRoot;
    serialized.shadowRoot = null;
    if (shadowRoot !== null) {
      serialized.shadowRoot = serialize(
        shadowRoot,
        maxDepth,
        ownershipType,
        serializationInternalMap,
        realm,
        options
      );
    }
  }

  if (isShadowRoot(node)) {
    serialized.mode = node.mode;
  }

  return serialized;
}

/**
 * Serialize a value as a remote value.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-remote-value
 *
 * @param {Object} value
 *     Value of any type to be serialized.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {Object} options
 * @param {NodeCache} options.nodeCache
 *     The cache containing DOM node references.
 *
 * @return {Object} Serialized representation of the value.
 */
export function serialize(
  value,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm,
  options
) {
  const type = typeof value;

  // Primitive protocol values
  if (type == "undefined") {
    return { type };
  } else if (Object.is(value, null)) {
    return { type: "null" };
  } else if (Object.is(value, NaN)) {
    return { type: "number", value: "NaN" };
  } else if (Object.is(value, -0)) {
    return { type: "number", value: "-0" };
  } else if (Object.is(value, Infinity)) {
    return { type: "number", value: "Infinity" };
  } else if (Object.is(value, -Infinity)) {
    return { type: "number", value: "-Infinity" };
  } else if (type == "bigint") {
    return { type, value: value.toString() };
  } else if (["boolean", "number", "string"].includes(type)) {
    return { type, value };
  }

  const handleId = getHandleForObject(realm, ownershipType, value);
  const knownObject = serializationInternalMap.has(value);

  // Set the OwnershipModel to use for all complex object serializations.
  ownershipType = OwnershipModel.None;

  // Remote values

  // symbols are primitive JS values which can only be serialized
  // as remote values.
  if (type == "symbol") {
    return buildSerialized("symbol", handleId);
  }

  // All other remote values are non-primitives and their
  // className can be extracted with ChromeUtils.getClassName
  const className = ChromeUtils.getClassName(value);
  if (["Array", "HTMLCollection", "NodeList"].includes(className)) {
    return serializeArrayLike(
      className.toLowerCase(),
      handleId,
      knownObject,
      value,
      maxDepth,
      ownershipType,
      serializationInternalMap,
      realm,
      options
    );
  } else if (className == "RegExp") {
    const serialized = buildSerialized("regexp", handleId);
    serialized.value = { pattern: value.source, flags: value.flags };
    return serialized;
  } else if (className == "Date") {
    const serialized = buildSerialized("date", handleId);
    serialized.value = value.toISOString();
    return serialized;
  } else if (className == "Map") {
    const serialized = buildSerialized("map", handleId);
    setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

    if (!knownObject && maxDepth !== null && maxDepth > 0) {
      serialized.value = serializeMapping(
        value.entries(),
        maxDepth,
        ownershipType,
        serializationInternalMap,
        realm
      );
    }
    return serialized;
  } else if (className == "Set") {
    const serialized = buildSerialized("set", handleId);
    setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

    if (!knownObject && maxDepth !== null && maxDepth > 0) {
      serialized.value = serializeList(
        value.values(),
        maxDepth,
        ownershipType,
        serializationInternalMap,
        realm
      );
    }
    return serialized;
  } else if (
    [
      "ArrayBuffer",
      "Function",
      "Promise",
      "WeakMap",
      "WeakSet",
      "Window",
    ].includes(className)
  ) {
    return buildSerialized(className.toLowerCase(), handleId);
  } else if (lazy.error.isError(value)) {
    return buildSerialized("error", handleId);
  } else if (TYPED_ARRAY_CLASSES.includes(className)) {
    return buildSerialized("typedarray", handleId);
  } else if (Node.isInstance(value)) {
    const serialized = buildSerialized("node", handleId);

    // Get or create the shared id for WebDriver classic compat from the node.
    const sharedId = getSharedIdForNode(value, realm, options);
    if (sharedId !== null) {
      serialized.sharedId = sharedId;
    }

    setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

    if (!knownObject) {
      serialized.value = serializeNode(
        value,
        maxDepth,
        ownershipType,
        serializationInternalMap,
        realm,
        options
      );
    }

    return serialized;
  } else if (ChromeUtils.isDOMObject(value)) {
    const serialized = buildSerialized("object", handleId);
    return serialized;
  }

  // Otherwise serialize the JavaScript object as generic object.
  const serialized = buildSerialized("object", handleId);
  setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

  if (!knownObject && maxDepth !== null && maxDepth > 0) {
    serialized.value = serializeMapping(
      Object.entries(value),
      maxDepth,
      ownershipType,
      serializationInternalMap,
      realm,
      options
    );
  }
  return serialized;
}

/**
 * Set the internalId property of a provided serialized RemoteValue,
 * and potentially of a previously created serialized RemoteValue,
 * corresponding to the same provided object.
 *
 * @see https://w3c.github.io/webdriver-bidi/#set-internal-ids-if-needed
 *
 * @param {Map} serializationInternalMap
 *     Map of objects to remote values.
 * @param {Object} remoteValue
 *     A serialized RemoteValue for the provided object.
 * @param {Object} object
 *     Object of any type to be serialized.
 */
function setInternalIdsIfNeeded(serializationInternalMap, remoteValue, object) {
  if (!serializationInternalMap.has(object)) {
    // If the object was not tracked yet in the current serialization, add
    // a new entry in the serialization internal map. An internal id will only
    // be generated if the same object is encountered again.
    serializationInternalMap.set(object, remoteValue);
  } else {
    // This is at least the second time this object is encountered, retrieve the
    // original remote value stored for this object.
    const previousRemoteValue = serializationInternalMap.get(object);

    if (!previousRemoteValue.internalId) {
      // If the original remote value has no internal id yet, generate a uuid
      // and update the internalId of the original remote value with it.
      previousRemoteValue.internalId = getUUID();
    }

    // Copy the internalId of the original remote value to the new remote value.
    remoteValue.internalId = previousRemoteValue.internalId;
  }
}

/**
 * Safely stringify a value.
 *
 * @param {Object} value
 *     Value of any type to be stringified.
 *
 * @return {string} String representation of the value.
 */
export function stringify(obj) {
  let text;
  try {
    text =
      obj !== null && typeof obj === "object" ? obj.toString() : String(obj);
  } catch (e) {
    // The error-case will also be handled in `finally {}`.
  } finally {
    if (typeof text != "string") {
      text = Object.prototype.toString.apply(obj);
    }
  }

  return text;
}
