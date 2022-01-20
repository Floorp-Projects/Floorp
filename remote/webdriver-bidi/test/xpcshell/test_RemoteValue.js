/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRIMITIVE_TYPES = [
  { value: undefined, serialized: { type: "undefined" } },
  { value: null, serialized: { type: "null" } },
  { value: "foo", serialized: { type: "string", value: "foo" } },
  { value: Number.NaN, serialized: { type: "number", value: "NaN" } },
  { value: -0, serialized: { type: "number", value: "-0" } },
  {
    value: Number.POSITIVE_INFINITY,
    serialized: { type: "number", value: "+Infinity" },
  },
  {
    value: Number.NEGATIVE_INFINITY,
    serialized: { type: "number", value: "-Infinity" },
  },
  { value: 42, serialized: { type: "number", value: 42 } },
  { value: false, serialized: { type: "boolean", value: false } },
  { value: 42n, serialized: { type: "bigint", value: "42" } },
];

const { deserialize, serialize } = ChromeUtils.import(
  "chrome://remote/content/webdriver-bidi/RemoteValue.jsm"
);

add_test(function test_deserializePrimitiveTypes() {
  for (const type of PRIMITIVE_TYPES) {
    const { value: expectedValue, serialized } = type;
    const value = deserialize(serialized);

    info(`Checking '${type}'`);

    if (serialized.value == "NaN") {
      ok(Number.isNaN(value), `Got expected value for ${serialized}`);
    } else {
      Assert.strictEqual(
        value,
        expectedValue,
        `Got expected value for ${serialized}`
      );
    }
  }

  run_next_test();
});

add_test(function test_deserializePrimitiveTypesInvalidValues() {
  const invalidValues = [
    { type: "bigint", values: [undefined, null, false, "foo", [], {}] },
    { type: "boolean", values: [undefined, null, 42, "foo", [], {}] },
    {
      type: "number",
      values: [undefined, null, false, "43", [], {}],
    },
    { type: "string", values: [undefined, null, false, 42, [], {}] },
  ];

  for (const invalidValue of invalidValues) {
    const { type, values } = invalidValue;

    for (const value of values) {
      info(`Checking '${type}' with value ${value}`);

      Assert.throws(
        () => deserialize({ type, value }),
        /InvalidArgument/,
        `Got expected error for type ${type} and value ${value}`
      );
    }
  }

  run_next_test();
});

add_test(function test_serializePrimitiveTypes() {
  for (const type of PRIMITIVE_TYPES) {
    const { value, serialized } = type;

    const serializedValue = serialize(value);
    for (const prop in serialized) {
      info(`Checking '${serialized.type}'`);

      Assert.strictEqual(
        serializedValue[prop],
        serialized[prop],
        `Got expected value for property ${prop}`
      );
    }
  }

  run_next_test();
});
