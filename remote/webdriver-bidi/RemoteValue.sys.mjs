/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  dom: "chrome://remote/content/shared/DOM.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  generateUUID: "chrome://remote/content/shared/UUID.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

/**
 * @typedef {object} IncludeShadowTreeMode
 */

/**
 * Enum of include shadow tree modes supported by the serialization.
 *
 * @readonly
 * @enum {IncludeShadowTreeMode}
 */
export const IncludeShadowTreeMode = {
  All: "all",
  None: "none",
  Open: "open",
};

/**
 * @typedef {object} OwnershipModel
 */

/**
 * Enum of ownership models supported by the serialization.
 *
 * @readonly
 * @enum {OwnershipModel}
 */
export const OwnershipModel = {
  None: "none",
  Root: "root",
};

/**
 * Extra options for deserializing remote values.
 *
 * @typedef {object} ExtraDeserializationOptions
 *
 * @property {NodeCache=} nodeCache
 *     The cache containing DOM node references.
 * @property {Function=} emitScriptMessage
 *     The function to emit "script.message" event.
 */

/**
 * Extra options for serializing remote values.
 *
 * @typedef {object} ExtraSerializationOptions
 *
 * @property {NodeCache=} nodeCache
 *     The cache containing DOM node references.
 * @property {Map<BrowsingContext, Array<string>>} seenNodeIds
 *     Map of browsing contexts to their seen node ids during the current
 *     serialization.
 */

/**
 * An object which holds the information of how
 * ECMAScript objects should be serialized.
 *
 * @typedef {object} SerializationOptions
 *
 * @property {number} [maxDomDepth=0]
 *     Depth of a serialization of DOM Nodes. Defaults to 0.
 * @property {number} [maxObjectDepth=null]
 *     Depth of a serialization of objects. Defaults to null.
 * @property {IncludeShadowTreeMode} [includeShadowTree=IncludeShadowTreeMode.None]
 *     Mode of a serialization of shadow dom. Defaults to "none".
 */

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
 * @returns {object}
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
 * @param {ExtraDeserializationOptions} extraOptions
 *     Extra Remote Value deserialization options.
 *
 * @returns {Array} List of deserialized values.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedValueList</var> is not an array.
 */
function deserializeValueList(realm, serializedValueList, extraOptions) {
  lazy.assert.array(
    serializedValueList,
    `Expected "serializedValueList" to be an array, got ${serializedValueList}`
  );

  const deserializedValues = [];

  for (const item of serializedValueList) {
    deserializedValues.push(deserialize(realm, item, extraOptions));
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
 * @param {ExtraDeserializationOptions} extraOptions
 *     Extra Remote Value deserialization options.
 *
 * @returns {Array} List of deserialized key-value.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedKeyValueList</var> is not an array or
 *     not an array of key-value arrays.
 */
function deserializeKeyValueList(realm, serializedKeyValueList, extraOptions) {
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
        : deserialize(realm, serializedKey, extraOptions);
    const deserializedValue = deserialize(realm, serializedValue, extraOptions);

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
 * @param {ExtraDeserializationOptions} extraOptions
 *     Extra Remote Value deserialization options.
 *
 * @returns {Node} The deserialized DOM node.
 */
function deserializeSharedReference(sharedRef, realm, extraOptions) {
  const { nodeCache } = extraOptions;

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
 * @param {object} serializedValue
 *     Value of any type to be deserialized.
 * @param {ExtraDeserializationOptions} extraOptions
 *     Extra Remote Value deserialization options.
 *
 * @returns {object} Deserialized representation of the value.
 */
export function deserialize(realm, serializedValue, extraOptions) {
  const { handle, sharedId, type, value } = serializedValue;

  // With a shared id present deserialize as node reference.
  if (sharedId !== undefined) {
    lazy.assert.string(
      sharedId,
      `Expected "sharedId" to be a string, got ${sharedId}`
    );

    return deserializeSharedReference(sharedId, realm, extraOptions);
  }

  // With a handle present deserialize as remote reference.
  if (handle !== undefined) {
    lazy.assert.string(
      handle,
      `Expected "handle" to be a string, got ${handle}`
    );

    const object = realm.getObjectForHandle(handle);
    if (!object) {
      throw new lazy.error.NoSuchHandleError(
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

    // Script channel
    case "channel": {
      const channel = message =>
        extraOptions.emitScriptMessage(realm, value, message);
      return realm.cloneIntoRealm(channel);
    }

    // Non-primitive protocol values
    case "array":
      const array = realm.cloneIntoRealm([]);
      deserializeValueList(realm, value, extraOptions).forEach(v =>
        array.push(v)
      );
      return array;
    case "date":
      // We want to support only Date Time String format,
      // check if the value follows it.
      checkDateTimeString(value);

      return realm.cloneIntoRealm(new Date(value));
    case "map":
      const map = realm.cloneIntoRealm(new Map());
      deserializeKeyValueList(realm, value, extraOptions).forEach(([k, v]) =>
        map.set(k, v)
      );

      return map;
    case "object":
      const object = realm.cloneIntoRealm({});
      deserializeKeyValueList(realm, value, extraOptions).forEach(
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
      deserializeValueList(realm, value, extraOptions).forEach(v => set.add(v));
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
 * @param {object} object
 *     The object being serialized.
 *
 * @returns {string} The unique handle id for the object. Will be null if the
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
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {string}
 *    Shared unique reference for the Node.
 */
function getSharedIdForNode(node, extraOptions) {
  const { nodeCache, seenNodeIds } = extraOptions;

  if (!Node.isInstance(node)) {
    return null;
  }

  const browsingContext = node.ownerGlobal.browsingContext;
  if (!browsingContext) {
    return null;
  }

  return nodeCache.getOrCreateNodeReference(node, seenNodeIds);
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
 * @param {object} value
 *     The Array-like object to serialize.
 * @param {SerializationOptions} serializationOptions
 *     Options which define how ECMAScript objects should be serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {object} Object for serialized values.
 */
function serializeArrayLike(
  production,
  handleId,
  knownObject,
  value,
  serializationOptions,
  ownershipType,
  serializationInternalMap,
  realm,
  extraOptions
) {
  const serialized = buildSerialized(production, handleId);
  setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

  if (!knownObject && serializationOptions.maxObjectDepth !== 0) {
    serialized.value = serializeList(
      value,
      serializationOptions,
      ownershipType,
      serializationInternalMap,
      realm,
      extraOptions
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
 * @param {SerializationOptions} serializationOptions
 *     Options which define how ECMAScript objects should be serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {Array} List of serialized values.
 */
function serializeList(
  iterable,
  serializationOptions,
  ownershipType,
  serializationInternalMap,
  realm,
  extraOptions
) {
  const { maxObjectDepth } = serializationOptions;
  const serialized = [];
  const childSerializationOptions = {
    ...serializationOptions,
  };
  if (maxObjectDepth !== null) {
    childSerializationOptions.maxObjectDepth = maxObjectDepth - 1;
  }

  for (const item of iterable) {
    serialized.push(
      serialize(
        item,
        childSerializationOptions,
        ownershipType,
        serializationInternalMap,
        realm,
        extraOptions
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
 * @param {SerializationOptions} serializationOptions
 *     Options which define how ECMAScript objects should be serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {Array} List of serialized values.
 */
function serializeMapping(
  iterable,
  serializationOptions,
  ownershipType,
  serializationInternalMap,
  realm,
  extraOptions
) {
  const { maxObjectDepth } = serializationOptions;
  const serialized = [];
  const childSerializationOptions = {
    ...serializationOptions,
  };
  if (maxObjectDepth !== null) {
    childSerializationOptions.maxObjectDepth = maxObjectDepth - 1;
  }

  for (const [key, item] of iterable) {
    const serializedKey =
      typeof key == "string"
        ? key
        : serialize(
            key,
            childSerializationOptions,
            ownershipType,
            serializationInternalMap,
            realm,
            extraOptions
          );
    const serializedValue = serialize(
      item,
      childSerializationOptions,
      ownershipType,
      serializationInternalMap,
      realm,
      extraOptions
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
 * @param {SerializationOptions} serializationOptions
 *     Options which define how ECMAScript objects should be serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {object} Serialized value.
 */
function serializeNode(
  node,
  serializationOptions,
  ownershipType,
  serializationInternalMap,
  realm,
  extraOptions
) {
  const { includeShadowTree, maxDomDepth } = serializationOptions;
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
  if (
    maxDomDepth !== 0 &&
    (!lazy.dom.isShadowRoot(node) ||
      (includeShadowTree === IncludeShadowTreeMode.Open &&
        node.mode === "open") ||
      includeShadowTree === IncludeShadowTreeMode.All)
  ) {
    const children = [];
    const childSerializationOptions = {
      ...serializationOptions,
    };
    if (maxDomDepth !== null) {
      childSerializationOptions.maxDomDepth = maxDomDepth - 1;
    }
    for (const child of node.childNodes) {
      children.push(
        serialize(
          child,
          childSerializationOptions,
          ownershipType,
          serializationInternalMap,
          realm,
          extraOptions
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

    const shadowRoot = node.openOrClosedShadowRoot;
    serialized.shadowRoot = null;
    if (shadowRoot !== null) {
      serialized.shadowRoot = serialize(
        shadowRoot,
        serializationOptions,
        ownershipType,
        serializationInternalMap,
        realm,
        extraOptions
      );
    }
  }

  if (lazy.dom.isShadowRoot(node)) {
    serialized.mode = node.mode;
  }

  return serialized;
}

/**
 * Serialize a value as a remote value.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-remote-value
 *
 * @param {object} value
 *     Value of any type to be serialized.
 * @param {SerializationOptions} serializationOptions
 *     Options which define how ECMAScript objects should be serialized.
 * @param {OwnershipModel} ownershipType
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 * @param {ExtraSerializationOptions} extraOptions
 *     Extra Remote Value serialization options.
 *
 * @returns {object} Serialized representation of the value.
 */
export function serialize(
  value,
  serializationOptions,
  ownershipType,
  serializationInternalMap,
  realm,
  extraOptions
) {
  const { maxObjectDepth } = serializationOptions;
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
      serializationOptions,
      ownershipType,
      serializationInternalMap,
      realm,
      extraOptions
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

    if (!knownObject && maxObjectDepth !== 0) {
      serialized.value = serializeMapping(
        value.entries(),
        serializationOptions,
        ownershipType,
        serializationInternalMap,
        realm,
        extraOptions
      );
    }
    return serialized;
  } else if (className == "Set") {
    const serialized = buildSerialized("set", handleId);
    setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

    if (!knownObject && maxObjectDepth !== 0) {
      serialized.value = serializeList(
        value.values(),
        serializationOptions,
        ownershipType,
        serializationInternalMap,
        realm,
        extraOptions
      );
    }
    return serialized;
  } else if (
    ["ArrayBuffer", "Function", "Promise", "WeakMap", "WeakSet"].includes(
      className
    )
  ) {
    return buildSerialized(className.toLowerCase(), handleId);
  } else if (className.includes("Generator")) {
    return buildSerialized("generator", handleId);
  } else if (lazy.error.isError(value)) {
    return buildSerialized("error", handleId);
  } else if (Cu.isProxy(value)) {
    return buildSerialized("proxy", handleId);
  } else if (TYPED_ARRAY_CLASSES.includes(className)) {
    return buildSerialized("typedarray", handleId);
  } else if (Node.isInstance(value)) {
    const serialized = buildSerialized("node", handleId);

    value = Cu.unwaiveXrays(value);

    // Get or create the shared id for WebDriver classic compat from the node.
    const sharedId = getSharedIdForNode(value, extraOptions);
    if (sharedId !== null) {
      serialized.sharedId = sharedId;
    }

    setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

    if (!knownObject) {
      serialized.value = serializeNode(
        value,
        serializationOptions,
        ownershipType,
        serializationInternalMap,
        realm,
        extraOptions
      );
    }

    return serialized;
  } else if (className === "Window") {
    const serialized = buildSerialized("window", handleId);
    const window = Cu.unwaiveXrays(value);

    if (window.browsingContext.parent == null) {
      serialized.value = {
        context: window.browsingContext.browserId.toString(),
        isTopBrowsingContext: true,
      };
    } else {
      serialized.value = {
        context: window.browsingContext.id.toString(),
      };
    }

    return serialized;
  } else if (ChromeUtils.isDOMObject(value)) {
    const serialized = buildSerialized("object", handleId);
    return serialized;
  }

  // Otherwise serialize the JavaScript object as generic object.
  const serialized = buildSerialized("object", handleId);
  setInternalIdsIfNeeded(serializationInternalMap, serialized, value);

  if (!knownObject && maxObjectDepth !== 0) {
    serialized.value = serializeMapping(
      Object.entries(value),
      serializationOptions,
      ownershipType,
      serializationInternalMap,
      realm,
      extraOptions
    );
  }
  return serialized;
}

/**
 * Set default serialization options.
 *
 * @param {SerializationOptions} options
 *    Options which define how ECMAScript objects should be serialized.
 * @returns {SerializationOptions}
 *    Serialiation options with default value added.
 */
export function setDefaultSerializationOptions(options = {}) {
  const serializationOptions = { ...options };
  if (!("maxDomDepth" in serializationOptions)) {
    serializationOptions.maxDomDepth = 0;
  }
  if (!("maxObjectDepth" in serializationOptions)) {
    serializationOptions.maxObjectDepth = null;
  }
  if (!("includeShadowTree" in serializationOptions)) {
    serializationOptions.includeShadowTree = IncludeShadowTreeMode.None;
  }

  return serializationOptions;
}

/**
 * Set default values and assert if serialization options have
 * expected types.
 *
 * @param {SerializationOptions} options
 *    Options which define how ECMAScript objects should be serialized.
 * @returns {SerializationOptions}
 *    Serialiation options with default value added.
 */
export function setDefaultAndAssertSerializationOptions(options = {}) {
  lazy.assert.object(options);

  const serializationOptions = setDefaultSerializationOptions(options);

  const { includeShadowTree, maxDomDepth, maxObjectDepth } =
    serializationOptions;

  if (maxDomDepth !== null) {
    lazy.assert.positiveInteger(maxDomDepth);
  }
  if (maxObjectDepth !== null) {
    lazy.assert.positiveInteger(maxObjectDepth);
  }
  const includeShadowTreeModesValues = Object.values(IncludeShadowTreeMode);
  lazy.assert.that(
    includeShadowTree =>
      includeShadowTreeModesValues.includes(includeShadowTree),
    `includeShadowTree ${includeShadowTree} doesn't match allowed values "${includeShadowTreeModesValues.join(
      "/"
    )}"`
  )(includeShadowTree);

  return serializationOptions;
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
 * @param {object} remoteValue
 *     A serialized RemoteValue for the provided object.
 * @param {object} object
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
      previousRemoteValue.internalId = lazy.generateUUID();
    }

    // Copy the internalId of the original remote value to the new remote value.
    remoteValue.internalId = previousRemoteValue.internalId;
  }
}

/**
 * Safely stringify a value.
 *
 * @param {object} obj
 *     Value of any type to be stringified.
 *
 * @returns {string} String representation of the value.
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
