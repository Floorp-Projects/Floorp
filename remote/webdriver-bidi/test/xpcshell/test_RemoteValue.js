/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

class MockDate extends Date {}

// Mock `toString` to avoid timezone differences.
MockDate.prototype.toString = function() {
  return "Tue May 31 2022 15:47:29 GMT+0200 (Central European Summer Time)";
};

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

const REMOTE_SIMPLE_VALUES = [
  {
    value: new RegExp(/foo/g),
    serialized: {
      type: "regexp",
      value: {
        pattern: "foo",
        flags: "g",
      },
    },
  },
  {
    value: new MockDate(1654004849000),
    serialized: {
      type: "date",
      value: "Tue May 31 2022 15:47:29 GMT+0200 (Central European Summer Time)",
    },
  },
];

const REMOTE_COMPLEX_VALUES = [
  {
    value: [1],
    serialized: {
      type: "array",
    },
  },
  {
    value: [1],
    maxDepth: 0,
    serialized: {
      type: "array",
    },
  },
  {
    value: [1, "2", true, new RegExp(/foo/g)],
    maxDepth: 1,
    serialized: {
      type: "array",
      value: [
        { type: "number", value: 1 },
        { type: "string", value: "2" },
        { type: "boolean", value: true },
        {
          type: "regexp",
          value: {
            pattern: "foo",
            flags: "g",
          },
        },
      ],
    },
  },
  {
    value: [1, [3, "4"]],
    maxDepth: 1,
    serialized: {
      type: "array",
      value: [{ type: "number", value: 1 }, { type: "array" }],
    },
  },
  {
    value: [1, [3, "4"]],
    maxDepth: 2,
    serialized: {
      type: "array",
      value: [
        { type: "number", value: 1 },
        {
          type: "array",
          value: [
            { type: "number", value: 3 },
            { type: "string", value: "4" },
          ],
        },
      ],
    },
  },
  {
    value: new Map(),
    maxDepth: 1,
    serialized: {
      type: "map",
      value: [],
    },
  },
  {
    value: new Map([]),
    maxDepth: 1,
    serialized: {
      type: "map",
      value: [],
    },
  },
  {
    value: new Map([
      [1, 2],
      ["2", "3"],
      [true, false],
    ]),
    serialized: {
      type: "map",
    },
  },
  {
    value: new Map([
      [1, 2],
      ["2", "3"],
      [true, false],
    ]),
    maxDepth: 0,
    serialized: {
      type: "map",
    },
  },
  {
    value: new Map([
      [1, 2],
      ["2", "3"],
      [true, false],
    ]),
    maxDepth: 1,
    serialized: {
      type: "map",
      value: [
        [
          { type: "number", value: 1 },
          { type: "number", value: 2 },
        ],
        ["2", { type: "string", value: "3" }],
        [
          { type: "boolean", value: true },
          { type: "boolean", value: false },
        ],
      ],
    },
  },
  {
    value: new Set(),
    maxDepth: 1,
    serialized: {
      type: "set",
      value: [],
    },
  },
  {
    value: new Set([]),
    maxDepth: 1,
    serialized: {
      type: "set",
      value: [],
    },
  },
  {
    value: new Set([1, "2", true]),
    serialized: {
      type: "set",
    },
  },
  {
    value: new Set([1, "2", true]),
    maxDepth: 0,
    serialized: {
      type: "set",
    },
  },
  {
    value: new Set([1, "2", true]),
    maxDepth: 1,
    serialized: {
      type: "set",
      value: [
        { type: "number", value: 1 },
        { type: "string", value: "2" },
        { type: "boolean", value: true },
      ],
    },
  },
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

add_test(function test_serializeRemoteSimpleValues() {
  for (const type of REMOTE_SIMPLE_VALUES) {
    const { value, serialized } = type;

    info(`Checking '${serialized.type}'`);
    const serializedValue = serialize(value);

    Assert.deepEqual(serialized, serializedValue, "Got expected structure");
  }

  run_next_test();
});

add_test(function test_serializeRemoteComplexValues() {
  for (const type of REMOTE_COMPLEX_VALUES) {
    const { value, serialized, maxDepth } = type;

    info(`Checking '${serialized.type}'`);
    const serializedValue = serialize(value, maxDepth);

    Assert.deepEqual(serialized, serializedValue, "Got expected structure");
  }

  run_next_test();
});
