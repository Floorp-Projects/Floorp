/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["deserialize", "serialize"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  InvalidArgumentError: "chrome://remote/content/shared/webdriver/Errors.jsm",
  Log: "chrome://remote/content/shared/Log.jsm",
});

XPCOMUtils.defineLazyGetter(lazy, "logger", () =>
  lazy.Log.get(lazy.Log.TYPES.WEBDRIVER_BIDI)
);

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
    throw new lazy.InvalidArgumentError(
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
function deserializeValueList(/* realm, */ serializedValueList) {
  lazy.assert.array(
    serializedValueList,
    `Expected "serializedValueList" to be an array, got ${serializedValueList}`
  );

  const deserializedValues = [];

  for (const item of serializedValueList) {
    deserializedValues.push(deserialize(/*realm, */ item));
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
function deserializeKeyValueList(/* realm, */ serializedKeyValueList) {
  lazy.assert.array(
    serializedKeyValueList,
    `Expected "serializedKeyValueList" to be an array, got ${serializedKeyValueList}`
  );

  const deserializedKeyValueList = [];

  for (const serializedKeyValue of serializedKeyValueList) {
    if (!Array.isArray(serializedKeyValue) || serializedKeyValue.length != 2) {
      throw new lazy.InvalidArgumentError(
        `Expected key-value pair to be an array with 2 elements, got ${serializedKeyValue}`
      );
    }
    const [serializedKey, serializedValue] = serializedKeyValue;
    const deserializedKey =
      typeof serializedKey == "string"
        ? serializedKey
        : deserialize(/* realm, */ serializedKey);
    const deserializedValue = deserialize(/* realm, */ serializedValue);

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
 * @returns {Object} Deserialized representation of the value.
 */
function deserialize(/* realm, */ serializedValue) {
  const { objectId, type, value } = serializedValue;

  if (type !== undefined) {
    lazy.assert.string(type, `Expected "type" to be a string, got ${type}`);
  }

  // With an objectId present deserialize as remote reference.
  if (objectId !== undefined) {
    lazy.assert.string(
      objectId,
      `Expected "objectId" to be a string, got ${objectId}`
    );

    // TODO: Implement deserialization of remote references (bug 1693838)
    lazy.logger.warn(
      `Unsupported type remote reference with objectId ${objectId}`
    );
    return undefined;
  }

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
      lazy.assert.in(value, ["NaN", "-0", "+Infinity", "-Infinity"]);
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
        throw new lazy.InvalidArgumentError(
          `Failed to deserialize value as BigInt: ${value}`
        );
      }

    // Non-primitive protocol values
    case "array":
      return deserializeValueList(/* realm, */ value);
    case "date":
      // We want to support only Date Time String format,
      // check if the value follows it.
      checkDateTimeString(value);

      return new Date(value);
    case "map":
      return new Map(deserializeKeyValueList(value));
    case "object":
      return Object.fromEntries(deserializeKeyValueList(value));
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
        throw new lazy.InvalidArgumentError(
          `Failed to deserialize value as RegExp: ${value}`
        );
      }
    case "set":
      return new Set(deserializeValueList(/* realm, */ value));
  }

  lazy.logger.warn(`Unsupported type for local value ${type}`);
  return undefined;
}

/**
 * Helper to serialize as a list.
 *
 * @see https://w3c.github.io/webdriver-bidi/#serialize-as-a-list
 *
 * @param {Iterable} iterable
 *     List of values to be serialized.
 *
 * @param {number=} maxDepth
 *     Depth of a serialization.
 *
 * @return {Array} List of serialized values.
 */
function serializeList(
  iterable,
  maxDepth /*,
  childOwnership,
  serializationInternalMap,
  realm*/
) {
  const serialized = [];
  const childDepth = maxDepth !== null ? maxDepth - 1 : null;

  for (const item of iterable) {
    serialized.push(
      serialize(
        item,
        childDepth /*, childOwnership, serializationInternalMap, realm*/
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
 *
 * @param {number=} maxDepth
 *     Depth of a serialization.
 *
 * @return {Array} List of serialized values.
 */
function serializeMapping(
  iterable,
  maxDepth /*,
  childOwnership,
  serializationInternalMap,
  realm*/
) {
  const serialized = [];
  const childDepth = maxDepth !== null ? maxDepth - 1 : null;

  for (const [key, item] of iterable) {
    const serializedKey =
      typeof key == "string"
        ? key
        : serialize(
            key,
            childDepth /*,
            childOwnership,
            serializationInternalMap,
            realm*/
          );
    const serializedValue = serialize(
      item,
      childDepth /*,
      childOwnership,
      serializationInternalMap,
      realm*/
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
 *
 * @param {number=} maxDepth
 *     Depth of a serialization.
 *
 * @returns {Object} Serialized representation of the value.
 */
function serialize(
  value,
  maxDepth /*,
  ownershipType,
  serializationInternalMap,
  realm */
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
    return { type: "number", value: "+Infinity" };
  } else if (Object.is(value, -Infinity)) {
    return { type: "number", value: "-Infinity" };
  } else if (type == "bigint") {
    return { type, value: value.toString() };
  } else if (["boolean", "number", "string"].includes(type)) {
    return { type, value };
  }

  const className = ChromeUtils.getClassName(value);

  // Remote values
  if (className == "Array") {
    const remoteValue = { type: "array" };

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.value = serializeList(
        value,
        maxDepth /*,
        ownershipType,
        serializationInternalMap,
        realm */
      );
    }

    return remoteValue;
  } else if (className == "RegExp") {
    return {
      type: "regexp",
      value: { pattern: value.source, flags: value.flags },
    };
  } else if (className == "Date") {
    return { type: "date", value: value.toISOString() };
  } else if (className == "Map") {
    const remoteValue = { type: "map" };

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.value = serializeMapping(
        value.entries(),
        maxDepth /*,
          ownershipType,
          serializationInternalMap,
          realm */
      );
    }

    return remoteValue;
  } else if (className == "Set") {
    const remoteValue = { type: "set" };

    if (maxDepth !== null && maxDepth > 0) {
      remoteValue.value = serializeList(
        value.values(),
        maxDepth /*,
        ownershipType,
        serializationInternalMap,
        realm */
      );
    }

    return remoteValue;
  }

  lazy.logger.warn(
    `Unsupported type: ${type} for remote value: ${value.toString()}`
  );

  return undefined;
}
