"use strict";

let json = [
  {
    namespace: "MV2",
    max_manifest_version: 2,

    properties: {
      PROP1: { value: 20 },
    },
  },
  {
    namespace: "MV3",
    min_manifest_version: 3,
    properties: {
      PROP1: { value: 20 },
    },
  },
  {
    namespace: "mixed",

    properties: {
      PROP_any: { value: 20 },
      PROP_mv3: {
        $ref: "submodule",
      },
    },
    types: [
      {
        id: "manifestTest",
        type: "object",
        properties: {
          // An example of extending the base type for permissions
          permissions: {
            type: "array",
            items: {
              $ref: "BaseType",
            },
            optional: true,
            default: [],
          },
          // An example of differentiating versions of a manifest entry
          multiple_choice: {
            optional: true,
            choices: [
              {
                max_manifest_version: 2,
                type: "array",
                items: {
                  type: "string",
                },
              },
              {
                min_manifest_version: 3,
                type: "array",
                items: {
                  type: "boolean",
                },
              },
              {
                type: "array",
                items: {
                  type: "object",
                  properties: {
                    value: { type: "boolean" },
                  },
                },
              },
            ],
          },
          accepting_unrecognized_props: {
            optional: true,
            type: "object",
            properties: {
              mv2_only_prop: {
                type: "string",
                optional: true,
                max_manifest_version: 2,
              },
              mv3_only_prop: {
                type: "string",
                optional: true,
                min_manifest_version: 3,
              },
              mv2_only_prop_with_default: {
                type: "string",
                optional: true,
                default: "only in MV2",
                max_manifest_version: 2,
              },
              mv3_only_prop_with_default: {
                type: "string",
                optional: true,
                default: "only in MV3",
                min_manifest_version: 3,
              },
            },
            additionalProperties: { $ref: "UnrecognizedProperty" },
          },
        },
      },
      {
        id: "submodule",
        type: "object",
        min_manifest_version: 3,
        functions: [
          {
            name: "sub_foo",
            type: "function",
            parameters: [],
            returns: { type: "integer" },
          },
          {
            name: "sub_no_match",
            type: "function",
            max_manifest_version: 2,
            parameters: [],
            returns: { type: "integer" },
          },
        ],
      },
      {
        id: "BaseType",
        choices: [
          {
            type: "string",
            enum: ["base"],
          },
        ],
      },
      {
        id: "type_any",
        type: "string",
        enum: ["value1", "value2", "value3"],
      },
      {
        id: "type_mv2",
        max_manifest_version: 2,
        type: "string",
        enum: ["value1", "value2", "value3"],
      },
      {
        id: "type_mv3",
        min_manifest_version: 3,
        type: "string",
        enum: ["value1", "value2", "value3"],
      },
      {
        id: "param_type_changed",
        type: "array",
        items: {
          choices: [
            { max_manifest_version: 2, type: "string" },
            {
              min_manifest_version: 3,
              type: "boolean",
            },
          ],
        },
      },
      {
        id: "object_type_changed",
        type: "object",
        properties: {
          prop_mv2: {
            type: "string",
            max_manifest_version: 2,
          },
          prop_mv3: {
            type: "string",
            min_manifest_version: 3,
          },
          prop_any: {
            type: "string",
          },
        },
      },
      {
        id: "no_valid_choices",
        type: "array",
        items: {
          choices: [
            { max_manifest_version: 1, type: "string" },
            {
              min_manifest_version: 4,
              type: "boolean",
            },
          ],
        },
      },
    ],

    functions: [
      {
        name: "fun_param_type_versioned",
        type: "function",
        parameters: [{ name: "arg1", $ref: "param_type_changed" }],
      },
      {
        name: "fun_mv2",
        max_manifest_version: 2,
        type: "function",
        parameters: [
          { name: "arg1", type: "integer", optional: true, default: 99 },
          { name: "arg2", type: "boolean", optional: true },
        ],
      },
      {
        name: "fun_mv3",
        min_manifest_version: 3,
        type: "function",
        parameters: [
          { name: "arg1", type: "integer", optional: true, default: 99 },
          { name: "arg2", type: "boolean", optional: true },
        ],
      },
      {
        name: "fun_param_change",
        type: "function",
        parameters: [{ name: "arg1", $ref: "object_type_changed" }],
      },
      {
        name: "fun_no_valid_param",
        type: "function",
        parameters: [{ name: "arg1", $ref: "no_valid_choices" }],
      },
    ],
    events: [
      {
        name: "onEvent_any",
        type: "function",
      },
      {
        name: "onEvent_mv2",
        max_manifest_version: 2,
        type: "function",
      },
      {
        name: "onEvent_mv3",
        min_manifest_version: 3,
        type: "function",
      },
    ],
  },
  {
    namespace: "mixed",
    types: [
      {
        $extend: "BaseType",
        choices: [
          {
            min_manifest_version: 3,
            type: "string",
            enum: ["extended"],
          },
        ],
      },
    ],
  },
  {
    namespace: "mixed",
    types: [
      {
        $extend: "manifestTest",
        properties: {
          versioned_extend: {
            optional: true,
            // just a simple type here
            type: "string",
            max_manifest_version: 2,
          },
        },
      },
    ],
  },
];

add_task(async function setup() {
  let url = "data:," + JSON.stringify(json);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  // We want the actual errors thrown here, and not warnings recast as errors.
  ExtensionTestUtils.failOnSchemaWarnings(false);
});

add_task(async function test_inject_V2() {
  // Test injecting into a V2 context.
  let wrapper = getContextWrapper(2);

  let root = {};
  Schemas.inject(root, wrapper);

  // Test elements available to both
  Assert.equal(root.mixed.type_any.VALUE1, "value1", "type_any exists");
  Assert.equal(root.mixed.PROP_any, 20, "mixed value property");

  // Test elements available to MV2
  Assert.equal(root.MV2.PROP1, 20, "MV2 value property");
  Assert.equal(root.mixed.type_mv2.VALUE2, "value2", "type_mv2 exists");

  // Test MV3 elements not available
  Assert.equal(root.MV3, undefined, "MV3 not injected");
  Assert.ok(!("MV3" in root), "MV3 not enumerable");
  Assert.equal(
    root.mixed.PROP_mv3,
    undefined,
    "mixed submodule property does not exist"
  );
  Assert.ok(
    !("PROP_mv3" in root.mixed),
    "mixed submodule property not enumerable"
  );
  Assert.equal(root.mixed.type_mv3, undefined, "type_mv3 does not exist");

  // Function tests
  Assert.ok(
    "fun_param_type_versioned" in root.mixed,
    "fun_param_type_versioned exists"
  );
  Assert.ok(
    !!root.mixed.fun_param_type_versioned,
    "fun_param_type_versioned exists"
  );
  Assert.ok("fun_mv2" in root.mixed, "fun_mv2 exists");
  Assert.ok(!!root.mixed.fun_mv2, "fun_mv2 exists");
  Assert.ok(!("fun_mv3" in root.mixed), "fun_mv3 does not exist");
  Assert.ok(!root.mixed.fun_mv3, "fun_mv3 does not exist");

  // Event tests
  Assert.ok("onEvent_any" in root.mixed, "onEvent_any exists");
  Assert.ok(!!root.mixed.onEvent_any, "onEvent_any exists");
  Assert.ok("onEvent_mv2" in root.mixed, "onEvent_mv2 exists");
  Assert.ok(!!root.mixed.onEvent_mv2, "onEvent_mv2 exists");
  Assert.ok(!("onEvent_mv3" in root.mixed), "onEvent_mv3 does not exist");
  Assert.ok(!root.mixed.onEvent_mv3, "onEvent_mv3 does not exist");

  // Function call tests
  root.mixed.fun_param_type_versioned(["hello"]);
  wrapper.verify("call", "mixed", "fun_param_type_versioned", [["hello"]]);
  Assert.throws(
    () => root.mixed.fun_param_type_versioned([true]),
    /Expected string instead of true/,
    "fun_param_type_versioned should throw for invalid type"
  );

  let propObj = { prop_any: "prop_any", prop_mv2: "prop_mv2" };
  root.mixed.fun_param_change(propObj);
  wrapper.verify("call", "mixed", "fun_param_change", [propObj]);

  // Still throw same error as we did before we knew of the MV3 property.
  Assert.throws(
    () => root.mixed.fun_param_change({ prop_mv3: "prop_mv3", ...propObj }),
    /Type error for parameter arg1 \(Unexpected property "prop_mv3"\)/,
    "generic unexpected property message for MV3 property in MV2 extension"
  );

  // But print the more specific and descriptive warning message to console.
  wrapper.checkErrors([
    `Property "prop_mv3" is unsupported in Manifest Version 2`,
  ]);

  Assert.throws(
    () => root.mixed.fun_no_valid_param("anything"),
    /Incorrect argument types for mixed.fun_no_valid_param/,
    "fun_no_valid_param should throw for versioned type"
  );
});

function normalizeTest(manifest, test, wrapper) {
  let normalized = Schemas.normalize(manifest, "mixed.manifestTest", wrapper);
  test(normalized);
  // The test function should call wrapper.checkErrors if it expected errors.
  // Here we call checkErrors again to ensure that there are not any unexpected
  // errors left.
  wrapper.checkErrors([]);
}

add_task(async function test_normalize_V2() {
  let wrapper = getContextWrapper(2);

  // Test normalize additions to the manifest structure
  normalizeTest(
    {
      versioned_extend: "test",
    },
    normalized => {
      Assert.equal(
        normalized.value.versioned_extend,
        "test",
        "resources normalized"
      );
    },
    wrapper
  );

  // Test normalizing baseType
  normalizeTest(
    {
      permissions: ["base"],
    },
    normalized => {
      Assert.equal(
        normalized.value.permissions[0],
        "base",
        "resources normalized"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      permissions: ["extended"],
    },
    normalized => {
      Assert.ok(
        normalized.error.startsWith("Error processing permissions.0"),
        `manifest normalized error ${normalized.error}`
      );
    },
    wrapper
  );

  // Test normalizing a value
  normalizeTest(
    {
      multiple_choice: ["foo.html"],
    },
    normalized => {
      Assert.equal(
        normalized.value.multiple_choice[0],
        "foo.html",
        "resources normalized"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      multiple_choice: [true],
    },
    normalized => {
      Assert.ok(
        normalized.error.startsWith("Error processing multiple_choice"),
        "manifest error"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      multiple_choice: [
        {
          value: true,
        },
      ],
    },
    normalized => {
      Assert.ok(
        normalized.value.multiple_choice[0].value,
        "resources normalized"
      );
    },
    wrapper
  );

  // Tests that object definitions including additionalProperties can
  // successfully accept objects from another manifest version, while ignoring
  // the actual value from the non-matching manifest value.
  normalizeTest(
    {
      accepting_unrecognized_props: {
        mv2_only_prop: "mv2 here",
        mv3_only_prop: "mv3 here",
      },
    },
    normalized => {
      equal(normalized.error, undefined, "no normalization error");
      Assert.deepEqual(
        normalized.value.accepting_unrecognized_props,
        {
          mv2_only_prop: "mv2 here",
          mv2_only_prop_with_default: "only in MV2",
        },
        "Normalized object for MV2, without MV3-specific props"
      );
      wrapper.checkErrors([
        `Property "mv3_only_prop" is unsupported in Manifest Version 2`,
      ]);
    },
    wrapper
  );
});

add_task(async function test_inject_V3() {
  // Test injecting into a V3 context.
  let wrapper = getContextWrapper(3);

  let root = {};
  Schemas.inject(root, wrapper);

  // Test elements available to both
  Assert.equal(root.mixed.type_any.VALUE1, "value1", "type_any exists");
  Assert.equal(root.mixed.PROP_any, 20, "mixed value property");

  // Test elements available to MV2
  Assert.equal(root.MV2, undefined, "MV2 value property");
  Assert.ok(!("MV2" in root), "MV2 not enumerable");
  Assert.equal(root.mixed.type_mv2, undefined, "type_mv2 does not exist");
  Assert.ok(!("type_mv2" in root.mixed), "type_mv2 not enumerable");

  // Test MV3 elements not available
  Assert.equal(root.MV3.PROP1, 20, "MV3 injected");
  Assert.ok(!!root.mixed.PROP_mv3, "mixed submodule property exists");
  Assert.equal(root.mixed.type_mv3.VALUE3, "value3", "type_mv3 exists");

  // Versioned submodule
  Assert.ok(!!root.mixed.PROP_mv3.sub_foo, "mixed submodule sub_foo exists");
  Assert.ok(
    !root.mixed.PROP_mv3.sub_no_match,
    "mixed submodule sub_no_match does not exist"
  );
  Assert.ok(
    !("sub_no_match" in root.mixed.PROP_mv3),
    "mixed submodule sub_no_match is not enumerable"
  );

  // Function tests
  Assert.ok(
    "fun_param_type_versioned" in root.mixed,
    "fun_param_type_versioned exists"
  );
  Assert.ok(
    !!root.mixed.fun_param_type_versioned,
    "fun_param_type_versioned exists"
  );
  Assert.ok(!("fun_mv2" in root.mixed), "fun_mv2 does not exist");
  Assert.ok(!root.mixed.fun_mv2, "fun_mv2 does not exist");
  Assert.ok("fun_mv3" in root.mixed, "fun_mv3 exists");
  Assert.ok(!!root.mixed.fun_mv3, "fun_mv3 exists");

  // Event tests
  Assert.ok("onEvent_any" in root.mixed, "onEvent_any exists");
  Assert.ok(!!root.mixed.onEvent_any, "onEvent_any exists");
  Assert.ok(!("onEvent_mv2" in root.mixed), "onEvent_mv2 not enumerable");
  Assert.ok(!root.mixed.onEvent_mv2, "onEvent_mv2 does not exist");
  Assert.ok("onEvent_mv3" in root.mixed, "onEvent_mv3 exists");
  Assert.ok(!!root.mixed.onEvent_mv3, "onEvent_mv3 exists");

  // Function call tests
  root.mixed.fun_param_type_versioned([true]);
  wrapper.verify("call", "mixed", "fun_param_type_versioned", [[true]]);
  Assert.throws(
    () => root.mixed.fun_param_type_versioned(["hello"]),
    /Expected boolean instead of "hello"/,
    "should throw for invalid type"
  );

  let propObj = { prop_any: "prop_any", prop_mv3: "prop_mv3" };
  root.mixed.fun_param_change(propObj);
  wrapper.verify("call", "mixed", "fun_param_change", [propObj]);
  Assert.throws(
    () => root.mixed.fun_param_change({ prop_mv2: "prop_mv2", ...propObj }),
    /Unexpected property "prop_mv2"/,
    "should throw for versioned type"
  );
  wrapper.checkErrors([
    `Property "prop_mv2" is unsupported in Manifest Version 3`,
  ]);

  root.mixed.PROP_mv3.sub_foo();
  wrapper.verify("call", "mixed.PROP_mv3", "sub_foo", []);
  Assert.throws(
    () => root.mixed.PROP_mv3.sub_no_match(),
    /TypeError: root.mixed.PROP_mv3.sub_no_match is not a function/,
    "sub_no_match should throw"
  );
});

add_task(async function test_normalize_V3() {
  let wrapper = getContextWrapper(3);

  // Test normalize additions to the manifest structure
  normalizeTest(
    {
      versioned_extend: "test",
    },
    normalized => {
      Assert.equal(
        normalized.error,
        `Unexpected property "versioned_extend"`,
        "expected manifest error"
      );
      wrapper.checkErrors([
        `Property "versioned_extend" is unsupported in Manifest Version 3`,
      ]);
    },
    wrapper
  );

  // Test normalizing baseType
  normalizeTest(
    {
      permissions: ["base"],
    },
    normalized => {
      Assert.equal(
        normalized.value.permissions[0],
        "base",
        "resources normalized"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      permissions: ["extended"],
    },
    normalized => {
      Assert.equal(
        normalized.value.permissions[0],
        "extended",
        "resources normalized"
      );
    },
    wrapper
  );

  // Test normalizing a value
  normalizeTest(
    {
      multiple_choice: ["foo.html"],
    },
    normalized => {
      Assert.ok(
        normalized.error.startsWith("Error processing multiple_choice"),
        "manifest error"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      multiple_choice: [true],
    },
    normalized => {
      Assert.equal(
        normalized.value.multiple_choice[0],
        true,
        "resources normalized"
      );
    },
    wrapper
  );

  normalizeTest(
    {
      multiple_choice: [
        {
          value: true,
        },
      ],
    },
    normalized => {
      Assert.ok(
        normalized.value.multiple_choice[0].value,
        "resources normalized"
      );
    },
    wrapper
  );

  wrapper.tallied = null;

  normalizeTest(
    {},
    normalized => {
      ok(!normalized.error, "manifest normalized");
    },
    wrapper
  );

  // Tests that object definitions including additionalProperties can
  // successfully accept objects from another manifest version, while ignoring
  // the actual value from the non-matching manifest value.
  normalizeTest(
    {
      accepting_unrecognized_props: {
        mv2_only_prop: "mv2 here",
        mv3_only_prop: "mv3 here",
      },
    },
    normalized => {
      equal(normalized.error, undefined, "no normalization error");
      Assert.deepEqual(
        normalized.value.accepting_unrecognized_props,
        {
          mv3_only_prop: "mv3 here",
          mv3_only_prop_with_default: "only in MV3",
        },
        "Normalized object for MV3, without MV2-specific props"
      );
      wrapper.checkErrors([
        `Property "mv2_only_prop" is unsupported in Manifest Version 3`,
      ]);
    },
    wrapper
  );
});
