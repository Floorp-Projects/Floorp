/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Realm } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/Realm.sys.mjs"
);
const { deserialize, serialize, setDefaultSerializationOptions, stringify } =
  ChromeUtils.importESModule(
    "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs"
  );

const PRIMITIVE_TYPES = [
  { value: undefined, serialized: { type: "undefined" } },
  { value: null, serialized: { type: "null" } },
  { value: "foo", serialized: { type: "string", value: "foo" } },
  { value: Number.NaN, serialized: { type: "number", value: "NaN" } },
  { value: -0, serialized: { type: "number", value: "-0" } },
  {
    value: Number.POSITIVE_INFINITY,
    serialized: { type: "number", value: "Infinity" },
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
  { value: Symbol("foo"), serialized: { type: "symbol" } },
  {
    value: [1],
    serialized: {
      type: "array",
      value: [{ type: "number", value: 1 }],
    },
  },
  {
    value: [1],
    serializationOptions: {
      maxObjectDepth: 0,
    },
    serialized: {
      type: "array",
    },
  },
  {
    value: [1, "2", true, new RegExp(/foo/g)],
    serializationOptions: {
      maxObjectDepth: 1,
    },
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
    serializationOptions: {
      maxObjectDepth: 1,
    },
    serialized: {
      type: "array",
      value: [{ type: "number", value: 1 }, { type: "array" }],
    },
  },
  {
    value: [1, [3, "4"]],
    serializationOptions: {
      maxObjectDepth: 2,
    },
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
    serializationOptions: {
      maxObjectDepth: 1,
    },
    serialized: {
      type: "map",
      value: [],
    },
    deserializable: true,
  },
  {
    value: new Map([]),
    serializationOptions: {
      maxObjectDepth: 1,
    },
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
    value: new Map([
      [1, 2],
      ["2", "3"],
      [true, false],
    ]),
    serializationOptions: {
      maxObjectDepth: 0,
    },
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
    serializationOptions: {
      maxObjectDepth: 1,
    },
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
    serializationOptions: {
      maxObjectDepth: 1,
    },
    serialized: {
      type: "set",
      value: [],
    },
    deserializable: true,
  },
  {
    value: new Set([]),
    serializationOptions: {
      maxObjectDepth: 1,
    },
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
      value: [
        { type: "number", value: 1 },
        { type: "string", value: "2" },
        { type: "boolean", value: true },
      ],
    },
  },
  {
    value: new Set([1, "2", true]),
    serializationOptions: {
      maxObjectDepth: 0,
    },
    serialized: {
      type: "set",
    },
  },
  {
    value: new Set([1, "2", true]),
    serializationOptions: {
      maxObjectDepth: 1,
    },
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
  { value: new WeakMap([[{}, 1]]), serialized: { type: "weakmap" } },
  { value: new WeakSet([{}]), serialized: { type: "weakset" } },
  {
    value: (function* () {
      yield "a";
    })(),
    serialized: { type: "generator" },
  },
  {
    value: (async function* () {
      yield await Promise.resolve(1);
    })(),
    serialized: { type: "generator" },
  },
  { value: new Error("error message"), serialized: { type: "error" } },
  {
    value: new SyntaxError("syntax error message"),
    serialized: { type: "error" },
  },
  {
    value: new TypeError("type error message"),
    serialized: { type: "error" },
  },
  { value: new Proxy({}, {}), serialized: { type: "proxy" } },
  { value: new Promise(() => true), serialized: { type: "promise" } },
  { value: new Int8Array(), serialized: { type: "typedarray" } },
  { value: new ArrayBuffer(), serialized: { type: "arraybuffer" } },
  { value: new URL("https://example.com"), serialized: { type: "object" } },
  { value: () => true, serialized: { type: "function" } },
  { value() {}, serialized: { type: "function" } },
  {
    value: {},
    serializationOptions: {
      maxObjectDepth: 1,
    },
    serialized: {
      type: "object",
      value: [],
    },
    deserializable: true,
  },
  {
    value: {
      1: 1,
      2: "2",
      foo: true,
    },
    serialized: {
      type: "object",
      value: [
        ["1", { type: "number", value: 1 }],
        ["2", { type: "string", value: "2" }],
        ["foo", { type: "boolean", value: true }],
      ],
    },
  },
  {
    value: {
      1: 1,
      2: "2",
      foo: true,
    },
    serializationOptions: {
      maxObjectDepth: 0,
    },
    serialized: {
      type: "object",
    },
  },
  {
    value: {
      1: 1,
      2: "2",
      foo: true,
    },
    serializationOptions: {
      maxObjectDepth: 1,
    },
    serialized: {
      type: "object",
      value: [
        ["1", { type: "number", value: 1 }],
        ["2", { type: "string", value: "2" }],
        ["foo", { type: "boolean", value: true }],
      ],
    },
    deserializable: true,
  },
  {
    value: {
      1: 1,
      2: "2",
      3: {
        bar: "foo",
      },
      foo: true,
    },
    serializationOptions: {
      maxObjectDepth: 2,
    },
    serialized: {
      type: "object",
      value: [
        ["1", { type: "number", value: 1 }],
        ["2", { type: "string", value: "2" }],
        [
          "3",
          {
            type: "object",
            value: [["bar", { type: "string", value: "foo" }]],
          },
        ],
        ["foo", { type: "boolean", value: true }],
      ],
    },
    deserializable: true,
  },
];

add_task(function test_deserializePrimitiveTypes() {
  const realm = new Realm();

  for (const type of PRIMITIVE_TYPES) {
    const { value: expectedValue, serialized } = type;

    info(`Checking '${serialized.type}'`);
    const value = deserialize(realm, serialized, {});

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
});

add_task(function test_deserializeDateLocalValue() {
  const realm = new Realm();

  const validaDateStrings = [
    "2009",
    "2009-05",
    "2009-05-19",
    "2022-02-29",
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
    const value = deserialize(realm, { type: "date", value: dateString }, {});

    Assert.equal(
      value.getTime(),
      new Date(dateString).getTime(),
      `Got expected value for ${dateString}`
    );
  }
});

add_task(function test_deserializeLocalValues() {
  const realm = new Realm();

  for (const type of REMOTE_SIMPLE_VALUES.concat(REMOTE_COMPLEX_VALUES)) {
    const { value: expectedValue, serialized, deserializable } = type;

    // Skip non deserializable cases
    if (!deserializable) {
      continue;
    }

    info(`Checking '${serialized.type}'`);
    const value = deserialize(realm, serialized, {});
    assertLocalValue(serialized.type, value, expectedValue);
  }
});

add_task(async function test_deserializeLocalValuesInWindowRealm() {
  for (const type of REMOTE_SIMPLE_VALUES.concat(REMOTE_COMPLEX_VALUES)) {
    const { value: expectedValue, serialized, deserializable } = type;

    // Skip non deserializable cases
    if (!deserializable) {
      continue;
    }

    const value = await deserializeInWindowRealm(serialized);
    assertLocalValue(serialized.type, value, expectedValue);
  }
});

add_task(async function test_deserializeChannel() {
  const realm = new Realm();
  const channel = {
    type: "channel",
    value: { channel: "channel_name" },
  };
  const deserializationOptions = {
    emitScriptMessage: (realm, channelProperties, message) => message,
  };

  info(`Checking 'channel'`);
  const value = deserialize(realm, channel, deserializationOptions, {});
  Assert.equal(
    Object.prototype.toString.call(value),
    "[object Function]",
    "Got expected type Function"
  );
  Assert.equal(value("foo"), "foo", "Got expected result");
});

add_task(function test_deserializeLocalValuesByHandle() {
  // Create two realms, realm1 will be used to serialize values, while realm2
  // will be used as a reference empty realm without any object reference.
  const realm1 = new Realm();
  const realm2 = new Realm();

  for (const type of REMOTE_SIMPLE_VALUES.concat(REMOTE_COMPLEX_VALUES)) {
    const { value: expectedValue, serialized } = type;

    // No need to skip non-deserializable cases here.

    info(`Checking '${serialized.type}'`);
    // Serialize the value once to get a handle.
    const serializedValue = serialize(
      expectedValue,
      { maxObjectDepth: 0 },
      "root",
      new Map(),
      realm1,
      {}
    );

    // Create a remote reference containing only the handle.
    // `deserialize` should not need any other property.
    const remoteReference = { handle: serializedValue.handle };

    // Check that the remote reference can be deserialized in realm1.
    const value = deserialize(realm1, remoteReference, {});
    assertLocalValue(serialized.type, value, expectedValue);

    Assert.throws(
      () => deserialize(realm2, remoteReference, {}),
      /NoSuchHandleError:/,
      `Got expected error when using the wrong realm for deserialize`
    );

    realm1.removeObjectHandle(serializedValue.handle);
    Assert.throws(
      () => deserialize(realm1, remoteReference, {}),
      /NoSuchHandleError:/,
      `Got expected error when after deleting the object handle`
    );
  }
});

add_task(function test_deserializeHandleInvalidTypes() {
  const realm = new Realm();

  for (const invalidType of [false, 42, {}, []]) {
    info(`Checking type: '${invalidType}'`);

    Assert.throws(
      () => deserialize(realm, { type: "object", handle: invalidType }, {}),
      /InvalidArgumentError:/,
      `Got expected error for type ${invalidType}`
    );
  }
});

add_task(function test_deserializePrimitiveTypesInvalidValues() {
  const realm = new Realm();

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
        () => deserialize(realm, { type, value }, {}),
        /InvalidArgument/,
        `Got expected error for type ${type} and value ${value}`
      );
    }
  }
});

add_task(function test_deserializeDateLocalValueInvalidValues() {
  const realm = new Realm();

  const invalidaDateStrings = [
    "10",
    "20009",
    "+20009",
    "2009-",
    "2009-0",
    "2009-15",
    "2009-02-1",
    "2009-02-50",
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
      () => deserialize(realm, { type: "date", value: dateString }, {}),
      /InvalidArgumentError:/,
      `Got expected error for date string: ${dateString}`
    );
  }
});

add_task(function test_deserializeLocalValuesInvalidType() {
  const realm = new Realm();

  const invalidTypes = [undefined, null, false, 42, {}];

  for (const invalidType of invalidTypes) {
    info(`Checking type: '${invalidType}'`);

    Assert.throws(
      () => deserialize(realm, { type: invalidType }, {}),
      /InvalidArgumentError:/,
      `Got expected error for type ${invalidType}`
    );

    Assert.throws(
      () =>
        deserialize(
          realm,
          {
            type: "array",
            value: [{ type: invalidType }],
          },
          {}
        ),
      /InvalidArgumentError:/,
      `Got expected error for nested type ${invalidType}`
    );
  }
});

add_task(function test_deserializeLocalValuesInvalidValues() {
  const realm = new Realm();

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
        [{ 1: "2" }],
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
        [{ 1: "2" }],
        [
          [
            { type: "number", value: "1" },
            { type: "number", value: "2" },
          ],
        ],
        [
          [
            { type: "object", value: [] },
            { type: "number", value: "1" },
          ],
        ],
        [
          [
            {
              type: "regexp",
              value: {
                pattern: "foo",
              },
            },
            { type: "number", value: "1" },
          ],
        ],
      ],
    },
  ];

  for (const invalidValue of invalidValues) {
    const { type, values } = invalidValue;

    for (const value of values) {
      info(`Checking '${type}' with value ${value}`);

      Assert.throws(
        () => deserialize(realm, { type, value }, {}),
        /InvalidArgumentError:/,
        `Got expected error for type ${type} and value ${value}`
      );
    }
  }
});

add_task(function test_serializePrimitiveTypes() {
  const realm = new Realm();

  for (const type of PRIMITIVE_TYPES) {
    const { value, serialized } = type;
    const defaultSerializationOptions = setDefaultSerializationOptions();

    const serializationInternalMap = new Map();
    const serializedValue = serialize(
      value,
      defaultSerializationOptions,
      "none",
      serializationInternalMap,
      realm,
      {}
    );
    assertInternalIds(serializationInternalMap, 0);
    Assert.deepEqual(serialized, serializedValue, "Got expected structure");

    // For primitive values, the serialization with ownershipType=root should
    // be exactly identical to the one with ownershipType=none.
    const serializationInternalMapWithRoot = new Map();
    const serializedWithRoot = serialize(
      value,
      defaultSerializationOptions,
      "root",
      serializationInternalMapWithRoot,
      realm,
      {}
    );
    assertInternalIds(serializationInternalMapWithRoot, 0);
    Assert.deepEqual(serialized, serializedWithRoot, "Got expected structure");
  }
});

add_task(function test_serializeRemoteSimpleValues() {
  const realm = new Realm();

  for (const type of REMOTE_SIMPLE_VALUES) {
    const { value, serialized } = type;
    const defaultSerializationOptions = setDefaultSerializationOptions();

    info(`Checking '${serialized.type}' with none ownershipType`);
    const serializationInternalMapWithNone = new Map();
    const serializedValue = serialize(
      value,
      defaultSerializationOptions,
      "none",
      serializationInternalMapWithNone,
      realm,
      {}
    );

    assertInternalIds(serializationInternalMapWithNone, 0);
    Assert.deepEqual(serialized, serializedValue, "Got expected structure");

    info(`Checking '${serialized.type}' with root ownershipType`);
    const serializationInternalMapWithRoot = new Map();
    const serializedWithRoot = serialize(
      value,
      defaultSerializationOptions,
      "root",
      serializationInternalMapWithRoot,
      realm,
      {}
    );

    assertInternalIds(serializationInternalMapWithRoot, 0);
    Assert.equal(
      typeof serializedWithRoot.handle,
      "string",
      "Got a handle property"
    );
    Assert.deepEqual(
      Object.assign({}, serialized, { handle: serializedWithRoot.handle }),
      serializedWithRoot,
      "Got expected structure, plus a generated handle id"
    );
  }
});

add_task(function test_serializeRemoteComplexValues() {
  for (const type of REMOTE_COMPLEX_VALUES) {
    const { value, serialized, serializationOptions } = type;
    const serializationOptionsWithDefaults =
      setDefaultSerializationOptions(serializationOptions);

    info(`Checking '${serialized.type}' with none ownershipType`);
    const realm = new Realm();
    const serializationInternalMapWithNone = new Map();

    const serializedValue = serialize(
      value,
      serializationOptionsWithDefaults,
      "none",
      serializationInternalMapWithNone,
      realm,
      {}
    );

    assertInternalIds(serializationInternalMapWithNone, 0);
    Assert.deepEqual(serialized, serializedValue, "Got expected structure");

    info(`Checking '${serialized.type}' with root ownershipType`);
    const serializationInternalMapWithRoot = new Map();
    const serializedWithRoot = serialize(
      value,
      serializationOptionsWithDefaults,
      "root",
      serializationInternalMapWithRoot,
      realm,
      {}
    );

    assertInternalIds(serializationInternalMapWithRoot, 0);
    Assert.equal(
      typeof serializedWithRoot.handle,
      "string",
      "Got a handle property"
    );
    Assert.deepEqual(
      Object.assign({}, serialized, { handle: serializedWithRoot.handle }),
      serializedWithRoot,
      "Got expected structure, plus a generated handle id"
    );
  }
});

add_task(function test_serializeWithSerializationInternalMap() {
  const dataSet = [
    {
      data: [1],
      serializedData: [{ type: "number", value: 1 }],
      type: "array",
    },
    {
      data: new Map([[true, false]]),
      serializedData: [
        [
          { type: "boolean", value: true },
          { type: "boolean", value: false },
        ],
      ],
      type: "map",
    },
    {
      data: new Set(["foo"]),
      serializedData: [{ type: "string", value: "foo" }],
      type: "set",
    },
    {
      data: { foo: "bar" },
      serializedData: [["foo", { type: "string", value: "bar" }]],
      type: "object",
    },
  ];
  const realm = new Realm();

  for (const { type, data, serializedData } of dataSet) {
    info(`Checking '${type}' with serializationInternalMap`);

    const serializationInternalMap = new Map();
    const value = [
      data,
      data,
      [data],
      new Set([data]),
      new Map([["bar", data]]),
      { bar: data },
    ];

    const serializedValue = serialize(
      value,
      { maxObjectDepth: 2 },
      "none",
      serializationInternalMap,
      realm,
      {}
    );

    assertInternalIds(serializationInternalMap, 1);

    const internalId = serializationInternalMap.get(data).internalId;

    const serialized = {
      type: "array",
      value: [
        {
          type,
          value: serializedData,
          internalId,
        },
        {
          type,
          internalId,
        },
        {
          type: "array",
          value: [{ type, internalId }],
        },
        {
          type: "set",
          value: [{ type, internalId }],
        },
        {
          type: "map",
          value: [["bar", { type, internalId }]],
        },
        {
          type: "object",
          value: [["bar", { type, internalId }]],
        },
      ],
    };

    Assert.deepEqual(serialized, serializedValue, "Got expected structure");
  }
});

add_task(function test_serializeMultipleValuesWithSerializationInternalMap() {
  const realm = new Realm();
  const serializationInternalMap = new Map();
  const obj1 = { foo: "bar" };
  const obj2 = [1, 2];
  const value = [obj1, obj2, obj1, obj2];

  serialize(
    value,
    { maxObjectDepth: 2 },
    "none",
    serializationInternalMap,
    realm,
    {}
  );

  assertInternalIds(serializationInternalMap, 2);

  const internalId1 = serializationInternalMap.get(obj1).internalId;
  const internalId2 = serializationInternalMap.get(obj2).internalId;

  Assert.notEqual(
    internalId1,
    internalId2,
    "Internal ids for different object are also different"
  );
});

add_task(function test_stringify() {
  const STRINGIFY_TEST_CASES = [
    [undefined, "undefined"],
    [null, "null"],
    ["foobar", "foobar"],
    ["2", "2"],
    [-0, "0"],
    [Infinity, "Infinity"],
    [-Infinity, "-Infinity"],
    [3, "3"],
    [1.4, "1.4"],
    [true, "true"],
    [42n, "42"],
    [{ toString: () => "bar" }, "bar", "toString: () => 'bar'"],
    [{ toString: () => 4 }, "[object Object]", "toString: () => 4"],
    [{ toString: undefined }, "[object Object]", "toString: undefined"],
    [{ toString: null }, "[object Object]", "toString: null"],
    [
      {
        toString: () => {
          throw new Error("toString error");
        },
      },
      "[object Object]",
      "toString: () => { throw new Error('toString error'); }",
    ],
  ];

  for (const [value, expectedString, description] of STRINGIFY_TEST_CASES) {
    info(`Checking '${description || value}'`);
    const stringifiedValue = stringify(value);

    Assert.strictEqual(expectedString, stringifiedValue, "Got expected string");
  }
});

function assertLocalValue(type, value, expectedValue) {
  let formattedValue = value;
  let formattedExpectedValue = expectedValue;

  // Format certain types for easier assertion
  if (type == "map") {
    Assert.equal(
      Object.prototype.toString.call(expectedValue),
      "[object Map]",
      "Got expected type Map"
    );

    formattedValue = Array.from(value.values());
    formattedExpectedValue = Array.from(expectedValue.values());
  } else if (type == "set") {
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

function assertInternalIds(serializationInternalMap, amount) {
  const remoteValuesWithInternalIds = Array.from(
    serializationInternalMap.values()
  ).filter(remoteValue => !!remoteValue.internalId);

  Assert.equal(
    remoteValuesWithInternalIds.length,
    amount,
    "Got expected amount of internalIds in serializationInternalMap"
  );
}

function deserializeInWindowRealm(serialized) {
  return SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [serialized],
    async _serialized => {
      const { WindowRealm } = ChromeUtils.importESModule(
        "chrome://remote/content/shared/Realm.sys.mjs"
      );
      const { deserialize } = ChromeUtils.importESModule(
        "chrome://remote/content/webdriver-bidi/RemoteValue.sys.mjs"
      );
      const realm = new WindowRealm(content);
      info(`Checking '${_serialized.type}'`);
      return deserialize(realm, _serialized, {});
    }
  );
}
