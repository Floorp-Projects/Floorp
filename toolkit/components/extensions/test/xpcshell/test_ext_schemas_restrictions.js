"use strict";

Components.utils.import("resource://gre/modules/Schemas.jsm");

let schemaJson = [
  {
    namespace: "noRestrict",
    properties: {
      prop1: {type: "object"},
      prop2: {type: "object", restrictions: ["test_zero", "test_one"]},
      prop3: {type: "number", value: 1},
      prop4: {type: "number", value: 1, restrictions: ["numeric_one"]},
    },
  },
  {
    namespace: "defaultRestrict",
    defaultRestrictions: ["test_two"],
    properties: {
      prop1: {type: "object"},
      prop2: {type: "object", restrictions: ["test_three"]},
      prop3: {type: "number", value: 1},
      prop4: {type: "number", value: 1, restrictions: ["numeric_two"]},
    },
  },
  {
    namespace: "withRestrict",
    restrictions: ["test_four"],
    properties: {
      prop1: {type: "object"},
      prop2: {type: "object", restrictions: ["test_five"]},
      prop3: {type: "number", value: 1},
      prop4: {type: "number", value: 1, restrictions: ["numeric_three"]},
    },
  },
  {
    namespace: "withRestrictAndDefault",
    restrictions: ["test_six"],
    defaultRestrictions: ["test_seven"],
    properties: {
      prop1: {type: "object"},
      prop2: {type: "object", restrictions: ["test_eight"]},
      prop3: {type: "number", value: 1},
      prop4: {type: "number", value: 1, restrictions: ["numeric_four"]},
    },
  },
  {
    namespace: "with_submodule",
    defaultRestrictions: ["test_nine"],
    types: [{
      id: "subtype",
      type: "object",
      functions: [{
        name: "restrictNo",
        type: "function",
        parameters: [],
      }, {
        name: "restrictYes",
        restrictions: ["test_ten"],
        type: "function",
        parameters: [],
      }],
    }],
    properties: {
      prop1: {$ref: "subtype"},
      prop2: {$ref: "subtype", restrictions: ["test_eleven"]},
    },
  },
];
add_task(function* testRestrictions() {
  let url = "data:," + JSON.stringify(schemaJson);
  yield Schemas.load(url);
  let results = {};
  let localWrapper = {
    shouldInject(ns, name, restrictions) {
      restrictions = restrictions ? restrictions.join() : "none";
      name = name === null ? ns : ns + "." + name;
      results[name] = restrictions;
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
    let result = results[path];
    do_check_eq(result, expected);
  }

  verify("noRestrict", "none");
  verify("noRestrict.prop1", "none");
  verify("noRestrict.prop2", "test_zero,test_one");
  verify("noRestrict.prop3", "none");
  verify("noRestrict.prop4", "numeric_one");

  verify("defaultRestrict", "none");
  verify("defaultRestrict.prop1", "test_two");
  verify("defaultRestrict.prop2", "test_three");
  verify("defaultRestrict.prop3", "test_two");
  verify("defaultRestrict.prop4", "numeric_two");

  verify("withRestrict", "test_four");
  verify("withRestrict.prop1", "none");
  verify("withRestrict.prop2", "test_five");
  verify("withRestrict.prop3", "none");
  verify("withRestrict.prop4", "numeric_three");

  verify("withRestrictAndDefault", "test_six");
  verify("withRestrictAndDefault.prop1", "test_seven");
  verify("withRestrictAndDefault.prop2", "test_eight");
  verify("withRestrictAndDefault.prop3", "test_seven");
  verify("withRestrictAndDefault.prop4", "numeric_four");

  verify("with_submodule", "none");
  verify("with_submodule.prop1", "test_nine");
  verify("with_submodule.prop1.restrictNo", "test_nine");
  verify("with_submodule.prop1.restrictYes", "test_ten");
  verify("with_submodule.prop2", "test_eleven");
  // Note: test_nine inherits restrictions from the namespace, not from
  // submodule. There is no "defaultRestrictions" for submodule types to not
  // complicate things.
  verify("with_submodule.prop1.restrictNo", "test_nine");
  verify("with_submodule.prop1.restrictYes", "test_ten");

  // This is a constant, so it does not matter that getImplementation does not
  // return an implementation since the API injector should take care of it.
  do_check_eq(root.noRestrict.prop3, 1);

  Assert.throws(() => root.noRestrict.prop1,
                /undefined/,
                "Should throw when the implementation is absent.");
});

