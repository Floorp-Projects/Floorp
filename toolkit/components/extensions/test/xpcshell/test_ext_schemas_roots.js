/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { SchemaRoot } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

let { SchemaAPIInterface } = ExtensionCommon;

const global = this;

let baseSchemaJSON = [
  {
    namespace: "base",

    properties: {
      PROP1: { value: 42 },
    },

    types: [
      {
        id: "type1",
        type: "string",
        enum: ["value1", "value2", "value3"],
      },
    ],

    functions: [
      {
        name: "foo",
        type: "function",
        parameters: [{ name: "arg1", $ref: "type1" }],
      },
    ],
  },
];

let experimentFooJSON = [
  {
    namespace: "experiments.foo",
    types: [
      {
        id: "typeFoo",
        type: "string",
        enum: ["foo1", "foo2", "foo3"],
      },
    ],

    functions: [
      {
        name: "foo",
        type: "function",
        parameters: [
          { name: "arg1", $ref: "typeFoo" },
          { name: "arg2", $ref: "base.type1" },
        ],
      },
    ],
  },
];

let experimentBarJSON = [
  {
    namespace: "experiments.bar",
    types: [
      {
        id: "typeBar",
        type: "string",
        enum: ["bar1", "bar2", "bar3"],
      },
    ],

    functions: [
      {
        name: "bar",
        type: "function",
        parameters: [
          { name: "arg1", $ref: "typeBar" },
          { name: "arg2", $ref: "base.type1" },
        ],
      },
    ],
  },
];

let tallied = null;

function tally(kind, ns, name, args) {
  tallied = [kind, ns, name, args];
}

function verify(...args) {
  equal(JSON.stringify(tallied), JSON.stringify(args));
  tallied = null;
}

let talliedErrors = [];

let permissions = new Set();

class TallyingAPIImplementation extends SchemaAPIInterface {
  constructor(namespace, name) {
    super();
    this.namespace = namespace;
    this.name = name;
  }

  callFunction(args) {
    tally("call", this.namespace, this.name, args);
    if (this.name === "sub_foo") {
      return 13;
    }
  }

  callFunctionNoReturn(args) {
    tally("call", this.namespace, this.name, args);
  }

  getProperty() {
    tally("get", this.namespace, this.name);
  }

  setProperty(value) {
    tally("set", this.namespace, this.name, value);
  }

  addListener(listener, args) {
    tally("addListener", this.namespace, this.name, [listener, args]);
  }

  removeListener(listener) {
    tally("removeListener", this.namespace, this.name, [listener]);
  }

  hasListener(listener) {
    tally("hasListener", this.namespace, this.name, [listener]);
  }
}

let wrapper = {
  url: "moz-extension://b66e3509-cdb3-44f6-8eb8-c8b39b3a1d27/",
  manifestVersion: 2,

  cloneScope: global,

  checkLoadURL(url) {
    return !url.startsWith("chrome:");
  },

  preprocessors: {
    localize(value, context) {
      return value.replace(/__MSG_(.*?)__/g, (m0, m1) => `${m1.toUpperCase()}`);
    },
  },

  logError(message) {
    talliedErrors.push(message);
  },

  hasPermission(permission) {
    return permissions.has(permission);
  },

  shouldInject(ns, name) {
    return name != "do-not-inject";
  },

  getImplementation(namespace, name) {
    return new TallyingAPIImplementation(namespace, name);
  },
};

add_task(async function() {
  let baseSchemas = new Map([["resource://schemas/base.json", baseSchemaJSON]]);
  let experimentSchemas = new Map([
    ["resource://experiment-foo/schema.json", experimentFooJSON],
    ["resource://experiment-bar/schema.json", experimentBarJSON],
  ]);

  let baseSchema = new SchemaRoot(null, baseSchemas);
  let schema = new SchemaRoot(baseSchema, experimentSchemas);

  baseSchema.parseSchemas();
  schema.parseSchemas();

  let root = {};
  let base = {};

  tallied = null;

  baseSchema.inject(base, wrapper);
  schema.inject(root, wrapper);

  equal(typeof base.base, "object", "base.base exists");
  equal(typeof root.base, "object", "root.base exists");
  equal(typeof base.experiments, "undefined", "base.experiments exists not");
  equal(typeof root.experiments, "object", "root.experiments exists");
  equal(typeof root.experiments.foo, "object", "root.experiments.foo exists");
  equal(typeof root.experiments.bar, "object", "root.experiments.bar exists");

  equal(tallied, null);

  equal(root.base.PROP1, 42, "root.base.PROP1");
  equal(base.base.PROP1, 42, "root.base.PROP1");

  root.base.foo("value2");
  verify("call", "base", "foo", ["value2"]);

  base.base.foo("value3");
  verify("call", "base", "foo", ["value3"]);

  root.experiments.foo.foo("foo2", "value1");
  verify("call", "experiments.foo", "foo", ["foo2", "value1"]);

  root.experiments.bar.bar("bar2", "value1");
  verify("call", "experiments.bar", "bar", ["bar2", "value1"]);

  Assert.throws(
    () => root.base.foo("Meh."),
    /Type error for parameter arg1/,
    "root.base.foo()"
  );

  Assert.throws(
    () => base.base.foo("Meh."),
    /Type error for parameter arg1/,
    "base.base.foo()"
  );

  Assert.throws(
    () => root.experiments.foo.foo("Meh."),
    /Incorrect argument types/,
    "root.experiments.foo.foo()"
  );

  Assert.throws(
    () => root.experiments.bar.bar("Meh."),
    /Incorrect argument types/,
    "root.experiments.bar.bar()"
  );
});
