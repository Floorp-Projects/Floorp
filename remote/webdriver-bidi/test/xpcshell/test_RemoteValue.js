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

const REMOTE_SIMPLE_VALUES = [
  {
    value: new RegExp(/foo/),
    serialized: {
      type: "regexp",
      value: {
        pattern: "foo",
        flags: "",
      },
    },
    deserializable: true,
  },
  {
    value: new RegExp(/foo/g),
    serialized: {
      type: "regexp",
      value: {
        pattern: "foo",
        flags: "g",
      },
    },
    deserializable: true,
  },
  {
    value: new Date(1654004849000),
    serialized: {
      type: "date",
      value: "2022-05-31T13:47:29.000Z",
    },
    deserializable: true,
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
    deserializable: true,
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
    deserializable: true,
  },
  {
    value: new Map(),
    maxDepth: 1,
    serialized: {
      type: "map",
      value: [],
    },
    deserializable: true,
  },
  {
    value: new Map([]),
    maxDepth: 1,
    serialized: {
      type: "map",
      value: [],
    },
    deserializable: true,
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
    deserializable: true,
  },
  {
    value: new Set(),
    maxDepth: 1,
    serialized: {
      type: "set",
      value: [],
    },
    deserializable: true,
  },
  {
    value: new Set([]),
    maxDepth: 1,
    serialized: {
      type: "set",
      value: [],
    },
    deserializable: true,
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
    deserializable: true,
  },
];

const { deserialize, serialize } = ChromeUtils.import(
  "chrome://remote/content/webdriver-bidi/RemoteValue.jsm"
);

add_test(function test_deserializePrimitiveTypes() {
  for (const type of PRIMITIVE_TYPES) {
    const { value: expectedValue, serialized } = type;

    info(`Checking '${serialized.type}'`);
    const value = deserialize(serialized);

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

add_test(function test_deserializeDateLocalValue() {
  const validaDateStrings = [
    "2009",
    "2009-05",
    "2009-05-19",
    "2009T15:00",
    "2009-05T15:00",
    "2009-05-19T15:00",
    "2009-05-19T15:00:15",
    "2009-05-19T15:00:15.452",
    "2009-05-19T15:00:15.452Z",
    "2009-05-19T15:00:15.452+02:00",
    "2009-05-19T15:00:15.452-02:00",
    "-271821-04-20T00:00:00Z",
    "+000000-01-01T00:00:00Z",
  ];
  for (const dateString of validaDateStrings) {
    info(`Checking '${dateString}'`);
    const value = deserialize({ type: "date", value: dateString });

    Assert.equal(
      value.getTime(),
      new Date(dateString).getTime(),
      `Got expected value for ${dateString}`
    );
  }

  run_next_test();
});

add_test(function test_deserializeLocalValues() {
  for (const type of REMOTE_SIMPLE_VALUES.concat(REMOTE_COMPLEX_VALUES)) {
    const { value: expectedValue, serialized, deserializable } = type;

    // Skip non deserializable cases
    if (!deserializable) {
      continue;
    }

    info(`Checking '${serialized.type}'`);
    const value = deserialize(serialized);

    let formattedValue = value;
    let formattedExpectedValue = expectedValue;

    // Format certain types for easier assertion
    if (serialized.type == "map") {
      Assert.equal(
        Object.prototype.toString.call(expectedValue),
        "[object Map]",
        "Got expected type Map"
      );

      formattedValue = Array.from(value.values());
      formattedExpectedValue = Array.from(expectedValue.values());
    } else if (serialized.type == "set") {
      Assert.equal(
        Object.prototype.toString.call(expectedValue),
        "[object Set]",
        "Got expected type Set"
      );

      formattedValue = Array.from(value);
      formattedExpectedValue = Array.from(expectedValue);
    }

    Assert.deepEqual(
      formattedValue,
      formattedExpectedValue,
      "Got expected structure"
    );
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

add_test(function test_deserializeDateLocalValueInvalidValues() {
  const invalidaDateStrings = [
    "10",
    "20009",
    "+20009",
    "2009-",
    "2009-0",
    "2009-15",
    "2009-02-1",
    "2009-02-50",
    "2022-02-29",
    "15:00",
    "T15:00",
    "9-05-19T15:00",
    "2009-5-19T15:00",
    "2009-05-1T15:00",
    "2009-02-10T15",
    "2009-05-19T15:",
    "2009-05-19T1:00",
    "2009-05-19T10:1",
    "2022-06-31T15:00",
    "2009-05-19T60:00",
    "2009-05-19T15:70",
    "2009-05-19T15:00.25",
    "2009-05-19+10:00",
    "2009-05-19Z",
    "2009-05-19 15:00",
    "2009-05-19t15:00Z",
    "2009-05-19T15:00z",
    "2009-05-19T15:00+01",
    "2009-05-19T10:10+1:00",
    "2009-05-19T10:10+01:1",
    "2009-05-19T15:00+75:00",
    "2009-05-19T15:00+02:80",
    "2009-05-19T15:00-00:00",
    "02009-05-19T15:00",
  ];
  for (const dateString of invalidaDateStrings) {
    info(`Checking '${dateString}'`);

    Assert.throws(
      () => deserialize({ type: "date", value: dateString }),
      /InvalidArgument/,
      `Got expected error for date string: ${dateString}`
    );
  }

  run_next_test();
});

add_test(function test_deserializeLocalValuesInvalidValues() {
  const invalidValues = [
    { type: "array", values: [undefined, null, false, 42, "foo", {}] },
    {
      type: "regexp",
      values: [
        undefined,
        null,
        false,
        "foo",
        42,
        [],
        {},
        { pattern: null },
        { pattern: 1 },
        { pattern: true },
        { pattern: "foo", flags: null },
        { pattern: "foo", flags: 1 },
        { pattern: "foo", flags: false },
        { pattern: "foo", flags: "foo" },
      ],
    },
    {
      type: "date",
      values: [
        undefined,
        null,
        false,
        "foo",
        "05 October 2011 14:48 UTC",
        "Tue Jun 14 2022 10:46:50 GMT+0200!",
        42,
        [],
        {},
      ],
    },
    {
      type: "map",
      values: [
        undefined,
        null,
        false,
        "foo",
        42,
        ["1"],
        [[]],
        [["1"]],
        [{ "1": "2" }],
        {},
      ],
    },
    {
      type: "set",
      values: [undefined, null, false, "foo", 42, {}],
    },
    {
      type: "object",
      values: [
        undefined,
        null,
        false,
        "foo",
        42,
        {},
        ["1"],
        [[]],
        [["1"]],
        [{ "1": "2" }],
      ],
    },
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
