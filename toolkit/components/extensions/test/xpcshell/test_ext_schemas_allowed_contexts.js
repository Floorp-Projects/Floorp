"use strict";

const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

const global = this;

let schemaJson = [
  {
    namespace: "noAllowedContexts",
    properties: {
      prop1: { type: "object" },
      prop2: { type: "object", allowedContexts: ["test_zero", "test_one"] },
      prop3: { type: "number", value: 1 },
      prop4: { type: "number", value: 1, allowedContexts: ["numeric_one"] },
    },
  },
  {
    namespace: "defaultContexts",
    defaultContexts: ["test_two"],
    properties: {
      prop1: { type: "object" },
      prop2: { type: "object", allowedContexts: ["test_three"] },
      prop3: { type: "number", value: 1 },
      prop4: { type: "number", value: 1, allowedContexts: ["numeric_two"] },
    },
  },
  {
    namespace: "withAllowedContexts",
    allowedContexts: ["test_four"],
    properties: {
      prop1: { type: "object" },
      prop2: { type: "object", allowedContexts: ["test_five"] },
      prop3: { type: "number", value: 1 },
      prop4: { type: "number", value: 1, allowedContexts: ["numeric_three"] },
    },
  },
  {
    namespace: "withAllowedContextsAndDefault",
    allowedContexts: ["test_six"],
    defaultContexts: ["test_seven"],
    properties: {
      prop1: { type: "object" },
      prop2: { type: "object", allowedContexts: ["test_eight"] },
      prop3: { type: "number", value: 1 },
      prop4: { type: "number", value: 1, allowedContexts: ["numeric_four"] },
    },
  },
  {
    namespace: "with_submodule",
    defaultContexts: ["test_nine"],
    types: [
      {
        id: "subtype",
        type: "object",
        functions: [
          {
            name: "noAllowedContexts",
            type: "function",
            parameters: [],
          },
          {
            name: "allowedContexts",
            allowedContexts: ["test_ten"],
            type: "function",
            parameters: [],
          },
        ],
      },
    ],
    properties: {
      prop1: { $ref: "subtype" },
      prop2: { $ref: "subtype", allowedContexts: ["test_eleven"] },
    },
  },
];

add_task(async function testRestrictions() {
  let url = "data:," + JSON.stringify(schemaJson);
  await Schemas.load(url);
  let results = {};
  let localWrapper = {
    manifestVersion: 2,
    cloneScope: global,
    shouldInject(ns, name, allowedContexts) {
      name = ns ? ns + "." + name : name;
      results[name] = allowedContexts.join(",");
      return true;
    },
    getImplementation() {
      // The actual implementation is not significant for this test.
      // Let's take this opportunity to see if schema generation is free of
      // exceptions even when somehow getImplementation does not return an
      // implementation.
    },
  };

  let root = {};
  Schemas.inject(root, localWrapper);

  function verify(path, expected) {
    let obj = root;
    for (let thing of path.split(".")) {
      try {
        obj = obj[thing];
      } catch (e) {
        // Blech.
      }
    }

    let result = results[path];
    equal(result, expected, path);
  }

  verify("noAllowedContexts", "");
  verify("noAllowedContexts.prop1", "");
  verify("noAllowedContexts.prop2", "test_zero,test_one");
  verify("noAllowedContexts.prop3", "");
  verify("noAllowedContexts.prop4", "numeric_one");

  verify("defaultContexts", "");
  verify("defaultContexts.prop1", "test_two");
  verify("defaultContexts.prop2", "test_three");
  verify("defaultContexts.prop3", "test_two");
  verify("defaultContexts.prop4", "numeric_two");

  verify("withAllowedContexts", "test_four");
  verify("withAllowedContexts.prop1", "");
  verify("withAllowedContexts.prop2", "test_five");
  verify("withAllowedContexts.prop3", "");
  verify("withAllowedContexts.prop4", "numeric_three");

  verify("withAllowedContextsAndDefault", "test_six");
  verify("withAllowedContextsAndDefault.prop1", "test_seven");
  verify("withAllowedContextsAndDefault.prop2", "test_eight");
  verify("withAllowedContextsAndDefault.prop3", "test_seven");
  verify("withAllowedContextsAndDefault.prop4", "numeric_four");

  verify("with_submodule", "");
  verify("with_submodule.prop1", "test_nine");
  verify("with_submodule.prop1.noAllowedContexts", "test_nine");
  verify("with_submodule.prop1.allowedContexts", "test_ten");
  verify("with_submodule.prop2", "test_eleven");
  // Note: test_nine inherits allowed contexts from the namespace, not from
  // submodule. There is no "defaultContexts" for submodule types to not
  // complicate things.
  verify("with_submodule.prop1.noAllowedContexts", "test_nine");
  verify("with_submodule.prop1.allowedContexts", "test_ten");

  // This is a constant, so it does not matter that getImplementation does not
  // return an implementation since the API injector should take care of it.
  equal(root.noAllowedContexts.prop3, 1);

  Assert.throws(
    () => root.noAllowedContexts.prop1,
    /undefined/,
    "Should throw when the implementation is absent."
  );
});
