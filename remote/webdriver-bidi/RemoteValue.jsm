/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "deserialize",
  "OwnershipModel",
  "serialize",
  "stringify",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
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
const OwnershipModel = {
  None: "none",
  Root: "root",
};

/**
 * Simplified representation of WebDriver BiDi's RemoteValue.
 * https://w3c.github.io/webdriver-bidi/#data-types-protocolValue-RemoteValue
 */
class RemoteValue {
  #handle;
  #value;
  #type;

  /**
   * Create a RemoteValue instance for the provided type and handle.
   *
   * @param {string} type
   *     RemoteValue type.
   * @param {string|null=}
   *     Optional unique handle id. Defaults to null, otherwise should be a
   *     string UUID.
   */
  constructor(type, handle = null) {
    this.#type = type;
    this.#handle = handle;
    this.#value = null;
  }

  /**
   * Serialize the RemoteValue.
   *
   * @return {Object}
   *     An object with a mandatory `type` property, and optional `handle` and
   *     `value` properties, depending on the OwnershipModel used for the
   *     serialization and on the value's type.
   */
  serialize() {
    const serialized = { type: this.#type };
    if (this.#handle !== null) {
      serialized.handle = this.#handle;
    }
    if (this.#value !== null) {
      serialized.value = this.#value;
    }
    return serialized;
  }

  /**
   * Set the `value` property of this RemoteValue.
   *
   * @param {Object}
   *     An object representing the value of this RemoteValue. Actual properties
   *     depend on the RemoteValue's type.
   */
  setValue(value) {
    this.#value = value;
  }
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
 * @param {Array} serializedValueList
 *     List of serialized values.
 *
 * @return {Array} List of deserialized values.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedValueList</var> is not an array.
 */
function deserializeValueList(realm, serializedValueList) {
  lazy.assert.array(
    serializedValueList,
    `Expected "serializedValueList" to be an array, got ${serializedValueList}`
  );

  const deserializedValues = [];

  for (const item of serializedValueList) {
    deserializedValues.push(deserialize(realm, item));
  }

  return deserializedValues;
}

/**
 * Helper to deserialize key-value list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#deserialize-key-value-list
 *
 * @param {Array} serializedKeyValueList
 *     List of serialized key-value.
 *
 * @return {Array} List of deserialized key-value.
 *
 * @throws {InvalidArgumentError}
 *     If <var>serializedKeyValueList</var> is not an array or
 *     not an array of key-value arrays.
 */
function deserializeKeyValueList(realm, serializedKeyValueList) {
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
        : deserialize(realm, serializedKey);
    const deserializedValue = deserialize(realm, serializedValue);

    deserializedKeyValueList.push([deserializedKey, deserializedValue]);
  }

  return deserializedKeyValueList;
}

/**
 * Deserialize a local value.
 *
 * @see https://w3c.github.io/webdriver-bidi/#deserialize-local-value
 *
 * @param {Object} serializedValue
 *     Value of any type to be deserialized.
 *
 * @return {Object} Deserialized representation of the value.
 */
function deserialize(realm, serializedValue) {
  const { handle, type, value } = serializedValue;

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
      return deserializeValueList(realm, value);
    case "date":
      // We want to support only Date Time String format,
      // check if the value follows it.
      checkDateTimeString(value);

      return new Date(value);
    case "map":
      return new Map(deserializeKeyValueList(realm, value));
    case "object":
      return Object.fromEntries(deserializeKeyValueList(realm, value));
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
        return new RegExp(pattern, flags);
      } catch (e) {
        throw new lazy.error.InvalidArgumentError(
          `Failed to deserialize value as RegExp: ${value}`
        );
      }
    case "set":
      return new Set(deserializeValueList(realm, value));
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
 * Helper to serialize as a list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-list
 *
 * @param {Iterable} iterable
 *     List of values to be serialized.
 * @param {number|null} maxDepth
 *     Depth of a serialization.
 * @param {OwnershipModel} childOwnership
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 *
 * @return {Array} List of serialized values.
 */
function serializeList(
  iterable,
  maxDepth,
  childOwnership,
  serializationInternalMap,
  realm
) {
  const serialized = [];
  const childDepth = maxDepth !== null ? maxDepth - 1 : null;

  for (const item of iterable) {
    serialized.push(
      serialize(
        item,
        childDepth,
        childOwnership,
        serializationInternalMap,
        realm
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
 * @param {OwnershipModel} childOwnership
 *     The ownership model to use for this serialization.
 * @param {Map} serializationInternalMap
 *     Map of internal ids.
 * @param {Realm} realm
 *     The Realm from which comes the value being serialized.
 *
 * @return {Array} List of serialized values.
 */
function serializeMapping(
  iterable,
  maxDepth,
  childOwnership,
  serializationInternalMap,
  realm
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
            childOwnership,
            serializationInternalMap,
            realm
          );
    const serializedValue = serialize(
      item,
      childDepth,
      childOwnership,
      serializationInternalMap,
      realm
    );

    serialized.push([serializedKey, serializedValue]);
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
 *
 * @return {Object} Serialized representation of the value.
 */
function serialize(
  value,
  maxDepth,
  ownershipType,
  serializationInternalMap,
  realm
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

  const className = ChromeUtils.getClassName(value);

  const handleId = getHandleForObject(realm, ownershipType, value);

  // Set the OwnershipModel to use for all complex object serializations.
  const childOwnership = OwnershipModel.None;

  // Remote values
  if (className == "Array") {
    const remoteValue = new RemoteValue("array", handleId);

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.setValue(
        serializeList(
          value,
          maxDepth,
          childOwnership,
          serializationInternalMap,
          realm
        )
      );
    }
    return remoteValue.serialize();
  } else if (className == "RegExp") {
    const remoteValue = new RemoteValue("regexp", handleId);
    remoteValue.setValue({ pattern: value.source, flags: value.flags });
    return remoteValue.serialize();
  } else if (className == "Date") {
    const remoteValue = new RemoteValue("date", handleId);
    remoteValue.setValue(value.toISOString());
    return remoteValue.serialize();
  } else if (className == "Map") {
    const remoteValue = new RemoteValue("map", handleId);

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.setValue(
        serializeMapping(
          value.entries(),
          maxDepth,
          childOwnership,
          serializationInternalMap,
          realm
        )
      );
    }
    return remoteValue.serialize();
  } else if (className == "Set") {
    const remoteValue = new RemoteValue("set", handleId);

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.setValue(
        serializeList(
          value.values(),
          maxDepth,
          childOwnership,
          serializationInternalMap,
          realm
        )
      );
    }
    return remoteValue.serialize();
  }
  // TODO: Bug 1770754. Remove the if condition when the serialization of all the other types is implemented,
  // since then the serialization of plain objects should be the fallback.
  else if (className == "Object") {
    const remoteValue = new RemoteValue("object", handleId);

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.setValue(
        serializeMapping(
          Object.entries(value),
          maxDepth,
          childOwnership,
          serializationInternalMap,
          realm
        )
      );
    }
    return remoteValue.serialize();
  }

  lazy.logger.warn(
    `Unsupported type: ${type} for remote value: ${stringify(value)}`
  );

  return undefined;
}

/**
 * Safely stringify a value.
 *
 * @param {Object} value
 *     Value of any type to be stringified.
 *
 * @return {string} String representation of the value.
 */
function stringify(obj) {
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
