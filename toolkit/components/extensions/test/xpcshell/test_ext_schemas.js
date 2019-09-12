"use strict";

const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");
const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);

let { LocalAPIImplementation, SchemaAPIInterface } = ExtensionCommon;

const global = this;

let json = [
  {
    namespace: "testing",

    properties: {
      PROP1: { value: 20 },
      prop2: { type: "string" },
      prop3: {
        $ref: "submodule",
      },
      prop4: {
        $ref: "submodule",
        unsupported: true,
      },
    },

    types: [
      {
        id: "type1",
        type: "string",
        enum: ["value1", "value2", "value3"],
      },

      {
        id: "type2",
        type: "object",
        properties: {
          prop1: { type: "integer" },
          prop2: { type: "array", items: { $ref: "type1" } },
        },
      },

      {
        id: "basetype1",
        type: "object",
        properties: {
          prop1: { type: "string" },
        },
      },

      {
        id: "basetype2",
        choices: [{ type: "integer" }],
      },

      {
        $extend: "basetype1",
        properties: {
          prop2: { type: "string" },
        },
      },

      {
        $extend: "basetype2",
        choices: [{ type: "string" }],
      },

      {
        id: "basetype3",
        type: "object",
        properties: {
          baseprop: { type: "string" },
        },
      },

      {
        id: "derivedtype1",
        type: "object",
        $import: "basetype3",
        properties: {
          derivedprop: { type: "string" },
        },
      },

      {
        id: "derivedtype2",
        type: "object",
        $import: "basetype3",
        properties: {
          derivedprop: { type: "integer" },
        },
      },

      {
        id: "submodule",
        type: "object",
        functions: [
          {
            name: "sub_foo",
            type: "function",
            parameters: [],
            returns: { type: "integer" },
          },
        ],
      },
    ],

    functions: [
      {
        name: "foo",
        type: "function",
        parameters: [
          { name: "arg1", type: "integer", optional: true, default: 99 },
          { name: "arg2", type: "boolean", optional: true },
        ],
      },

      {
        name: "bar",
        type: "function",
        parameters: [
          { name: "arg1", type: "integer", optional: true },
          { name: "arg2", type: "boolean" },
        ],
      },

      {
        name: "baz",
        type: "function",
        parameters: [
          {
            name: "arg1",
            type: "object",
            properties: {
              prop1: { type: "string" },
              prop2: { type: "integer", optional: true },
              prop3: { type: "integer", unsupported: true },
            },
          },
        ],
      },

      {
        name: "qux",
        type: "function",
        parameters: [{ name: "arg1", $ref: "type1" }],
      },

      {
        name: "quack",
        type: "function",
        parameters: [{ name: "arg1", $ref: "type2" }],
      },

      {
        name: "quora",
        type: "function",
        parameters: [{ name: "arg1", type: "function" }],
      },

      {
        name: "quileute",
        type: "function",
        parameters: [
          { name: "arg1", type: "integer", optional: true },
          { name: "arg2", type: "integer" },
        ],
      },

      {
        name: "queets",
        type: "function",
        unsupported: true,
        parameters: [],
      },

      {
        name: "quintuplets",
        type: "function",
        parameters: [
          {
            name: "obj",
            type: "object",
            properties: [],
            additionalProperties: { type: "integer" },
          },
        ],
      },

      {
        name: "quasar",
        type: "function",
        parameters: [
          {
            name: "abc",
            type: "object",
            properties: {
              func: {
                type: "function",
                parameters: [{ name: "x", type: "integer" }],
              },
            },
          },
        ],
      },

      {
        name: "quosimodo",
        type: "function",
        parameters: [
          {
            name: "xyz",
            type: "object",
            additionalProperties: { type: "any" },
          },
        ],
      },

      {
        name: "patternprop",
        type: "function",
        parameters: [
          {
            name: "obj",
            type: "object",
            properties: { prop1: { type: "string", pattern: "^\\d+$" } },
            patternProperties: {
              "(?i)^prop\\d+$": { type: "string" },
              "^foo\\d+$": { type: "string" },
            },
          },
        ],
      },

      {
        name: "pattern",
        type: "function",
        parameters: [
          { name: "arg", type: "string", pattern: "(?i)^[0-9a-f]+$" },
        ],
      },

      {
        name: "format",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              hostname: { type: "string", format: "hostname", optional: true },
              url: { type: "string", format: "url", optional: true },
              relativeUrl: {
                type: "string",
                format: "relativeUrl",
                optional: true,
              },
              strictRelativeUrl: {
                type: "string",
                format: "strictRelativeUrl",
                optional: true,
              },
              imageDataOrStrictRelativeUrl: {
                type: "string",
                format: "imageDataOrStrictRelativeUrl",
                optional: true,
              },
            },
          },
        ],
      },

      {
        name: "formatDate",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              date: { type: "string", format: "date", optional: true },
            },
          },
        ],
      },

      {
        name: "deep",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              foo: {
                type: "object",
                properties: {
                  bar: {
                    type: "array",
                    items: {
                      type: "object",
                      properties: {
                        baz: {
                          type: "object",
                          properties: {
                            required: { type: "integer" },
                            optional: { type: "string", optional: true },
                          },
                        },
                      },
                    },
                  },
                },
              },
            },
          },
        ],
      },

      {
        name: "errors",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              warn: {
                type: "string",
                pattern: "^\\d+$",
                optional: true,
                onError: "warn",
              },
              ignore: {
                type: "string",
                pattern: "^\\d+$",
                optional: true,
                onError: "ignore",
              },
              default: {
                type: "string",
                pattern: "^\\d+$",
                optional: true,
              },
            },
          },
        ],
      },

      {
        name: "localize",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              foo: { type: "string", preprocess: "localize", optional: true },
              bar: { type: "string", optional: true },
              url: {
                type: "string",
                preprocess: "localize",
                format: "url",
                optional: true,
              },
            },
          },
        ],
      },

      {
        name: "extended1",
        type: "function",
        parameters: [{ name: "val", $ref: "basetype1" }],
      },

      {
        name: "extended2",
        type: "function",
        parameters: [{ name: "val", $ref: "basetype2" }],
      },

      {
        name: "callderived1",
        type: "function",
        parameters: [{ name: "value", $ref: "derivedtype1" }],
      },

      {
        name: "callderived2",
        type: "function",
        parameters: [{ name: "value", $ref: "derivedtype2" }],
      },
    ],

    events: [
      {
        name: "onFoo",
        type: "function",
      },

      {
        name: "onBar",
        type: "function",
        extraParameters: [
          {
            name: "filter",
            type: "integer",
            optional: true,
            default: 1,
          },
        ],
      },
    ],
  },
  {
    namespace: "foreign",
    properties: {
      foreignRef: { $ref: "testing.submodule" },
    },
  },
  {
    namespace: "inject",
    properties: {
      PROP1: { value: "should inject" },
    },
  },
  {
    namespace: "do-not-inject",
    properties: {
      PROP1: { value: "should not inject" },
    },
  },
];

let tallied = null;

function tally(kind, ns, name, args) {
  tallied = [kind, ns, name, args];
}

function verify(...args) {
  Assert.equal(JSON.stringify(tallied), JSON.stringify(args));
  tallied = null;
}

let talliedErrors = [];

function checkErrors(errors) {
  Assert.equal(
    talliedErrors.length,
    errors.length,
    "Got expected number of errors"
  );
  for (let [i, error] of errors.entries()) {
    Assert.ok(
      i in talliedErrors && String(talliedErrors[i]).includes(error),
      `${JSON.stringify(error)} is a substring of error ${JSON.stringify(
        talliedErrors[i]
      )}`
    );
  }

  talliedErrors.length = 0;
}

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
  let url = "data:," + JSON.stringify(json);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  tallied = null;
  Schemas.inject(root, wrapper);
  Assert.equal(tallied, null);

  Assert.equal(root.testing.PROP1, 20, "simple value property");
  Assert.equal(root.testing.type1.VALUE1, "value1", "enum type");
  Assert.equal(root.testing.type1.VALUE2, "value2", "enum type");

  Assert.equal("inject" in root, true, "namespace 'inject' should be injected");
  Assert.equal(
    root["do-not-inject"],
    undefined,
    "namespace 'do-not-inject' should not be injected"
  );

  root.testing.foo(11, true);
  verify("call", "testing", "foo", [11, true]);

  root.testing.foo(true);
  verify("call", "testing", "foo", [99, true]);

  root.testing.foo(null, true);
  verify("call", "testing", "foo", [99, true]);

  root.testing.foo(undefined, true);
  verify("call", "testing", "foo", [99, true]);

  root.testing.foo(11);
  verify("call", "testing", "foo", [11, null]);

  Assert.throws(
    () => root.testing.bar(11),
    /Incorrect argument types/,
    "should throw without required arg"
  );

  Assert.throws(
    () => root.testing.bar(11, true, 10),
    /Incorrect argument types/,
    "should throw with too many arguments"
  );

  root.testing.bar(true);
  verify("call", "testing", "bar", [null, true]);

  root.testing.baz({ prop1: "hello", prop2: 22 });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: 22 }]);

  root.testing.baz({ prop1: "hello" });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: null }]);

  root.testing.baz({ prop1: "hello", prop2: null });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: null }]);

  Assert.throws(
    () => root.testing.baz({ prop2: 12 }),
    /Property "prop1" is required/,
    "should throw without required property"
  );

  Assert.throws(
    () => root.testing.baz({ prop1: "hi", prop3: 12 }),
    /Property "prop3" is unsupported by Firefox/,
    "should throw with unsupported property"
  );

  Assert.throws(
    () => root.testing.baz({ prop1: "hi", prop4: 12 }),
    /Unexpected property "prop4"/,
    "should throw with unexpected property"
  );

  Assert.throws(
    () => root.testing.baz({ prop1: 12 }),
    /Expected string instead of 12/,
    "should throw with wrong type"
  );

  root.testing.qux("value2");
  verify("call", "testing", "qux", ["value2"]);

  Assert.throws(
    () => root.testing.qux("value4"),
    /Invalid enumeration value "value4"/,
    "should throw for invalid enum value"
  );

  root.testing.quack({ prop1: 12, prop2: ["value1", "value3"] });
  verify("call", "testing", "quack", [
    { prop1: 12, prop2: ["value1", "value3"] },
  ]);

  Assert.throws(
    () =>
      root.testing.quack({ prop1: 12, prop2: ["value1", "value3", "value4"] }),
    /Invalid enumeration value "value4"/,
    "should throw for invalid array type"
  );

  function f() {}
  root.testing.quora(f);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["call", "testing", "quora"])
  );
  Assert.equal(tallied[3][0], f);
  tallied = null;

  let g = () => 0;
  root.testing.quora(g);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["call", "testing", "quora"])
  );
  Assert.equal(tallied[3][0], g);
  tallied = null;

  root.testing.quileute(10);
  verify("call", "testing", "quileute", [null, 10]);

  Assert.throws(
    () => root.testing.queets(),
    /queets is not a function/,
    "should throw for unsupported functions"
  );

  root.testing.quintuplets({ a: 10, b: 20, c: 30 });
  verify("call", "testing", "quintuplets", [{ a: 10, b: 20, c: 30 }]);

  Assert.throws(
    () => root.testing.quintuplets({ a: 10, b: 20, c: 30, d: "hi" }),
    /Expected integer instead of "hi"/,
    "should throw for wrong additionalProperties type"
  );

  root.testing.quasar({ func: f });
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["call", "testing", "quasar"])
  );
  Assert.equal(tallied[3][0].func, f);
  tallied = null;

  root.testing.quosimodo({ a: 10, b: 20, c: 30 });
  verify("call", "testing", "quosimodo", [{ a: 10, b: 20, c: 30 }]);
  tallied = null;

  Assert.throws(
    () => root.testing.quosimodo(10),
    /Incorrect argument types/,
    "should throw for wrong type"
  );

  root.testing.patternprop({
    prop1: "12",
    prop2: "42",
    Prop3: "43",
    foo1: "x",
  });
  verify("call", "testing", "patternprop", [
    { prop1: "12", prop2: "42", Prop3: "43", foo1: "x" },
  ]);
  tallied = null;

  root.testing.patternprop({ prop1: "12" });
  verify("call", "testing", "patternprop", [{ prop1: "12" }]);
  tallied = null;

  Assert.throws(
    () => root.testing.patternprop({ prop1: "12", foo1: null }),
    /Expected string instead of null/,
    "should throw for wrong property type"
  );

  Assert.throws(
    () => root.testing.patternprop({ prop1: "xx", prop2: "yy" }),
    /String "xx" must match \/\^\\d\+\$\//,
    "should throw for wrong property type"
  );

  Assert.throws(
    () => root.testing.patternprop({ prop1: "12", prop2: 42 }),
    /Expected string instead of 42/,
    "should throw for wrong property type"
  );

  Assert.throws(
    () => root.testing.patternprop({ prop1: "12", prop2: null }),
    /Expected string instead of null/,
    "should throw for wrong property type"
  );

  Assert.throws(
    () => root.testing.patternprop({ prop1: "12", propx: "42" }),
    /Unexpected property "propx"/,
    "should throw for unexpected property"
  );

  Assert.throws(
    () => root.testing.patternprop({ prop1: "12", Foo1: "x" }),
    /Unexpected property "Foo1"/,
    "should throw for unexpected property"
  );

  root.testing.pattern("DEADbeef");
  verify("call", "testing", "pattern", ["DEADbeef"]);
  tallied = null;

  Assert.throws(
    () => root.testing.pattern("DEADcow"),
    /String "DEADcow" must match \/\^\[0-9a-f\]\+\$\/i/,
    "should throw for non-match"
  );

  root.testing.format({ hostname: "foo" });
  verify("call", "testing", "format", [
    {
      hostname: "foo",
      imageDataOrStrictRelativeUrl: null,
      relativeUrl: null,
      strictRelativeUrl: null,
      url: null,
    },
  ]);
  tallied = null;

  for (let invalid of ["", " ", "http://foo", "foo/bar", "foo.com/", "foo?"]) {
    Assert.throws(
      () => root.testing.format({ hostname: invalid }),
      /Invalid hostname/,
      "should throw for invalid hostname"
    );
  }

  root.testing.format({ url: "http://foo/bar", relativeUrl: "http://foo/bar" });
  verify("call", "testing", "format", [
    {
      hostname: null,
      imageDataOrStrictRelativeUrl: null,
      relativeUrl: "http://foo/bar",
      strictRelativeUrl: null,
      url: "http://foo/bar",
    },
  ]);
  tallied = null;

  root.testing.format({
    relativeUrl: "foo.html",
    strictRelativeUrl: "foo.html",
  });
  verify("call", "testing", "format", [
    {
      hostname: null,
      imageDataOrStrictRelativeUrl: null,
      relativeUrl: `${wrapper.url}foo.html`,
      strictRelativeUrl: `${wrapper.url}foo.html`,
      url: null,
    },
  ]);
  tallied = null;

  root.testing.format({
    imageDataOrStrictRelativeUrl: "data:image/png;base64,A",
  });
  verify("call", "testing", "format", [
    {
      hostname: null,
      imageDataOrStrictRelativeUrl: "data:image/png;base64,A",
      relativeUrl: null,
      strictRelativeUrl: null,
      url: null,
    },
  ]);
  tallied = null;

  root.testing.format({
    imageDataOrStrictRelativeUrl: "data:image/jpeg;base64,A",
  });
  verify("call", "testing", "format", [
    {
      hostname: null,
      imageDataOrStrictRelativeUrl: "data:image/jpeg;base64,A",
      relativeUrl: null,
      strictRelativeUrl: null,
      url: null,
    },
  ]);
  tallied = null;

  root.testing.format({ imageDataOrStrictRelativeUrl: "foo.html" });
  verify("call", "testing", "format", [
    {
      hostname: null,
      imageDataOrStrictRelativeUrl: `${wrapper.url}foo.html`,
      relativeUrl: null,
      strictRelativeUrl: null,
      url: null,
    },
  ]);

  tallied = null;

  for (let format of ["url", "relativeUrl"]) {
    Assert.throws(
      () => root.testing.format({ [format]: "chrome://foo/content/" }),
      /Access denied/,
      "should throw for access denied"
    );
  }

  for (let urlString of ["//foo.html", "http://foo/bar.html"]) {
    Assert.throws(
      () => root.testing.format({ strictRelativeUrl: urlString }),
      /must be a relative URL/,
      "should throw for non-relative URL"
    );
  }

  Assert.throws(
    () =>
      root.testing.format({
        imageDataOrStrictRelativeUrl: "data:image/svg+xml;utf8,A",
      }),
    /must be a relative or PNG or JPG data:image URL/,
    "should throw for non-relative or non PNG/JPG data URL"
  );

  const dates = [
    "2016-03-04",
    "2016-03-04T08:00:00Z",
    "2016-03-04T08:00:00.000Z",
    "2016-03-04T08:00:00-08:00",
    "2016-03-04T08:00:00.000-08:00",
    "2016-03-04T08:00:00+08:00",
    "2016-03-04T08:00:00.000+08:00",
    "2016-03-04T08:00:00+0800",
    "2016-03-04T08:00:00-0800",
  ];
  dates.forEach(str => {
    root.testing.formatDate({ date: str });
    verify("call", "testing", "formatDate", [{ date: str }]);
  });

  // Make sure that a trivial change to a valid date invalidates it.
  dates.forEach(str => {
    Assert.throws(
      () => root.testing.formatDate({ date: "0" + str }),
      /Invalid date string/,
      "should throw for invalid iso date string"
    );
    Assert.throws(
      () => root.testing.formatDate({ date: str + "0" }),
      /Invalid date string/,
      "should throw for invalid iso date string"
    );
  });

  const badDates = [
    "I do not look anything like a date string",
    "2016-99-99",
    "2016-03-04T25:00:00Z",
  ];
  badDates.forEach(str => {
    Assert.throws(
      () => root.testing.formatDate({ date: str }),
      /Invalid date string/,
      "should throw for invalid iso date string"
    );
  });

  root.testing.deep({
    foo: { bar: [{ baz: { required: 12, optional: "42" } }] },
  });
  verify("call", "testing", "deep", [
    { foo: { bar: [{ baz: { optional: "42", required: 12 } }] } },
  ]);
  tallied = null;

  Assert.throws(
    () => root.testing.deep({ foo: { bar: [{ baz: { optional: "42" } }] } }),
    /Type error for parameter arg \(Error processing foo\.bar\.0\.baz: Property "required" is required\) for testing\.deep/,
    "should throw with the correct object path"
  );

  Assert.throws(
    () =>
      root.testing.deep({
        foo: { bar: [{ baz: { optional: 42, required: 12 } }] },
      }),
    /Type error for parameter arg \(Error processing foo\.bar\.0\.baz\.optional: Expected string instead of 42\) for testing\.deep/,
    "should throw with the correct object path"
  );

  talliedErrors.length = 0;

  root.testing.errors({ default: "0123", ignore: "0123", warn: "0123" });
  verify("call", "testing", "errors", [
    { default: "0123", ignore: "0123", warn: "0123" },
  ]);
  checkErrors([]);

  root.testing.errors({ default: "0123", ignore: "x123", warn: "0123" });
  verify("call", "testing", "errors", [
    { default: "0123", ignore: null, warn: "0123" },
  ]);
  checkErrors([]);

  ExtensionTestUtils.failOnSchemaWarnings(false);
  root.testing.errors({ default: "0123", ignore: "0123", warn: "x123" });
  ExtensionTestUtils.failOnSchemaWarnings(true);
  verify("call", "testing", "errors", [
    { default: "0123", ignore: "0123", warn: null },
  ]);
  checkErrors(['String "x123" must match /^\\d+$/']);

  root.testing.onFoo.addListener(f);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["addListener", "testing", "onFoo"])
  );
  Assert.equal(tallied[3][0], f);
  Assert.equal(JSON.stringify(tallied[3][1]), JSON.stringify([]));
  tallied = null;

  root.testing.onFoo.removeListener(f);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["removeListener", "testing", "onFoo"])
  );
  Assert.equal(tallied[3][0], f);
  tallied = null;

  root.testing.onFoo.hasListener(f);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["hasListener", "testing", "onFoo"])
  );
  Assert.equal(tallied[3][0], f);
  tallied = null;

  Assert.throws(
    () => root.testing.onFoo.addListener(10),
    /Invalid listener/,
    "addListener with non-function should throw"
  );

  root.testing.onBar.addListener(f, 10);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["addListener", "testing", "onBar"])
  );
  Assert.equal(tallied[3][0], f);
  Assert.equal(JSON.stringify(tallied[3][1]), JSON.stringify([10]));
  tallied = null;

  root.testing.onBar.addListener(f);
  Assert.equal(
    JSON.stringify(tallied.slice(0, -1)),
    JSON.stringify(["addListener", "testing", "onBar"])
  );
  Assert.equal(tallied[3][0], f);
  Assert.equal(JSON.stringify(tallied[3][1]), JSON.stringify([1]));
  tallied = null;

  Assert.throws(
    () => root.testing.onBar.addListener(f, "hi"),
    /Incorrect argument types/,
    "addListener with wrong extra parameter should throw"
  );

  let target = { prop1: 12, prop2: ["value1", "value3"] };
  let proxy = new Proxy(target, {});
  Assert.throws(
    () => root.testing.quack(proxy),
    /Expected a plain JavaScript object, got a Proxy/,
    "should throw when passing a Proxy"
  );

  if (Symbol.toStringTag) {
    let stringTarget = { prop1: 12, prop2: ["value1", "value3"] };
    stringTarget[Symbol.toStringTag] = () => "[object Object]";
    let stringProxy = new Proxy(stringTarget, {});
    Assert.throws(
      () => root.testing.quack(stringProxy),
      /Expected a plain JavaScript object, got a Proxy/,
      "should throw when passing a Proxy"
    );
  }

  root.testing.localize({
    foo: "__MSG_foo__",
    bar: "__MSG_foo__",
    url: "__MSG_http://example.com/__",
  });
  verify("call", "testing", "localize", [
    { bar: "__MSG_foo__", foo: "FOO", url: "http://example.com/" },
  ]);
  tallied = null;

  Assert.throws(
    () => root.testing.localize({ url: "__MSG_/foo/bar__" }),
    /\/FOO\/BAR is not a valid URL\./,
    "should throw for invalid URL"
  );

  root.testing.extended1({ prop1: "foo", prop2: "bar" });
  verify("call", "testing", "extended1", [{ prop1: "foo", prop2: "bar" }]);
  tallied = null;

  Assert.throws(
    () => root.testing.extended1({ prop1: "foo", prop2: 12 }),
    /Expected string instead of 12/,
    "should throw for wrong property type"
  );

  Assert.throws(
    () => root.testing.extended1({ prop1: "foo" }),
    /Property "prop2" is required/,
    "should throw for missing property"
  );

  Assert.throws(
    () => root.testing.extended1({ prop1: "foo", prop2: "bar", prop3: "xxx" }),
    /Unexpected property "prop3"/,
    "should throw for extra property"
  );

  root.testing.extended2("foo");
  verify("call", "testing", "extended2", ["foo"]);
  tallied = null;

  root.testing.extended2(12);
  verify("call", "testing", "extended2", [12]);
  tallied = null;

  Assert.throws(
    () => root.testing.extended2(true),
    /Incorrect argument types/,
    "should throw for wrong argument type"
  );

  root.testing.prop3.sub_foo();
  verify("call", "testing.prop3", "sub_foo", []);
  tallied = null;

  Assert.throws(
    () => root.testing.prop4.sub_foo(),
    /root.testing.prop4 is undefined/,
    "should throw for unsupported submodule"
  );

  root.foreign.foreignRef.sub_foo();
  verify("call", "foreign.foreignRef", "sub_foo", []);
  tallied = null;

  root.testing.callderived1({ baseprop: "s1", derivedprop: "s2" });
  verify("call", "testing", "callderived1", [
    { baseprop: "s1", derivedprop: "s2" },
  ]);
  tallied = null;

  Assert.throws(
    () => root.testing.callderived1({ baseprop: "s1", derivedprop: 42 }),
    /Error processing derivedprop: Expected string/,
    "Two different objects may $import the same base object"
  );
  Assert.throws(
    () => root.testing.callderived1({ baseprop: "s1" }),
    /Property "derivedprop" is required/,
    "Object using $import has its local properites"
  );
  Assert.throws(
    () => root.testing.callderived1({ derivedprop: "s2" }),
    /Property "baseprop" is required/,
    "Object using $import has imported properites"
  );

  root.testing.callderived2({ baseprop: "s1", derivedprop: 42 });
  verify("call", "testing", "callderived2", [
    { baseprop: "s1", derivedprop: 42 },
  ]);
  tallied = null;

  Assert.throws(
    () => root.testing.callderived2({ baseprop: "s1", derivedprop: "s2" }),
    /Error processing derivedprop: Expected integer/,
    "Two different objects may $import the same base object"
  );
  Assert.throws(
    () => root.testing.callderived2({ baseprop: "s1" }),
    /Property "derivedprop" is required/,
    "Object using $import has its local properites"
  );
  Assert.throws(
    () => root.testing.callderived2({ derivedprop: 42 }),
    /Property "baseprop" is required/,
    "Object using $import has imported properites"
  );
});

let deprecatedJson = [
  {
    namespace: "deprecated",

    properties: {
      accessor: {
        type: "string",
        writable: true,
        deprecated: "This is not the property you are looking for",
      },
    },

    types: [
      {
        id: "Type",
        type: "string",
      },
    ],

    functions: [
      {
        name: "property",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            properties: {
              foo: {
                type: "string",
              },
            },
            additionalProperties: {
              type: "any",
              deprecated: "Unknown property",
            },
          },
        ],
      },

      {
        name: "value",
        type: "function",
        parameters: [
          {
            name: "arg",
            choices: [
              {
                type: "integer",
              },
              {
                type: "string",
                deprecated: "Please use an integer, not ${value}",
              },
            ],
          },
        ],
      },

      {
        name: "choices",
        type: "function",
        parameters: [
          {
            name: "arg",
            deprecated: "You have no choices",
            choices: [
              {
                type: "integer",
              },
            ],
          },
        ],
      },

      {
        name: "ref",
        type: "function",
        parameters: [
          {
            name: "arg",
            choices: [
              {
                $ref: "Type",
                deprecated: "Deprecated alias",
              },
            ],
          },
        ],
      },

      {
        name: "method",
        type: "function",
        deprecated: "Do not call this method",
        parameters: [],
      },
    ],

    events: [
      {
        name: "onDeprecated",
        type: "function",
        deprecated: "This event does not work",
      },
    ],
  },
];

add_task(async function testDeprecation() {
  // This whole test expects deprecation warnings.
  ExtensionTestUtils.failOnSchemaWarnings(false);

  let url = "data:," + JSON.stringify(deprecatedJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  Schemas.inject(root, wrapper);

  talliedErrors.length = 0;

  root.deprecated.property({ foo: "bar", xxx: "any", yyy: "property" });
  verify("call", "deprecated", "property", [
    { foo: "bar", xxx: "any", yyy: "property" },
  ]);
  checkErrors([
    "Warning processing xxx: Unknown property",
    "Warning processing yyy: Unknown property",
  ]);

  root.deprecated.value(12);
  verify("call", "deprecated", "value", [12]);
  checkErrors([]);

  root.deprecated.value("12");
  verify("call", "deprecated", "value", ["12"]);
  checkErrors(['Please use an integer, not "12"']);

  root.deprecated.choices(12);
  verify("call", "deprecated", "choices", [12]);
  checkErrors(["You have no choices"]);

  root.deprecated.ref("12");
  verify("call", "deprecated", "ref", ["12"]);
  checkErrors(["Deprecated alias"]);

  root.deprecated.method();
  verify("call", "deprecated", "method", []);
  checkErrors(["Do not call this method"]);

  void root.deprecated.accessor;
  verify("get", "deprecated", "accessor", null);
  checkErrors(["This is not the property you are looking for"]);

  root.deprecated.accessor = "x";
  verify("set", "deprecated", "accessor", "x");
  checkErrors(["This is not the property you are looking for"]);

  root.deprecated.onDeprecated.addListener(() => {});
  checkErrors(["This event does not work"]);

  root.deprecated.onDeprecated.removeListener(() => {});
  checkErrors(["This event does not work"]);

  root.deprecated.onDeprecated.hasListener(() => {});
  checkErrors(["This event does not work"]);

  ExtensionTestUtils.failOnSchemaWarnings(true);

  Assert.throws(
    () => root.deprecated.onDeprecated.hasListener(() => {}),
    /This event does not work/,
    "Deprecation warning with extensions.webextensions.warnings-as-errors=true"
  );
});

let choicesJson = [
  {
    namespace: "choices",

    types: [],

    functions: [
      {
        name: "meh",
        type: "function",
        parameters: [
          {
            name: "arg",
            choices: [
              {
                type: "string",
                enum: ["foo", "bar", "baz"],
              },
              {
                type: "string",
                pattern: "florg.*meh",
              },
              {
                type: "integer",
                minimum: 12,
                maximum: 42,
              },
            ],
          },
        ],
      },

      {
        name: "foo",
        type: "function",
        parameters: [
          {
            name: "arg",
            choices: [
              {
                type: "object",
                properties: {
                  blurg: {
                    type: "string",
                    unsupported: true,
                    optional: true,
                  },
                },
                additionalProperties: {
                  type: "string",
                },
              },
              {
                type: "string",
              },
              {
                type: "array",
                minItems: 2,
                maxItems: 3,
                items: {
                  type: "integer",
                },
              },
            ],
          },
        ],
      },

      {
        name: "bar",
        type: "function",
        parameters: [
          {
            name: "arg",
            choices: [
              {
                type: "object",
                properties: {
                  baz: {
                    type: "string",
                  },
                },
              },
              {
                type: "array",
                items: {
                  type: "integer",
                },
              },
            ],
          },
        ],
      },
    ],
  },
];

add_task(async function testChoices() {
  let url = "data:," + JSON.stringify(choicesJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  Schemas.inject(root, wrapper);

  talliedErrors.length = 0;

  Assert.throws(
    () => root.choices.meh("frog"),
    /Value "frog" must either: be one of \["foo", "bar", "baz"\], match the pattern \/florg\.\*meh\/, or be an integer value/
  );

  Assert.throws(
    () => root.choices.meh(4),
    /be a string value, or be at least 12/
  );

  Assert.throws(
    () => root.choices.meh(43),
    /be a string value, or be no greater than 42/
  );

  Assert.throws(
    () => root.choices.foo([]),
    /be an object value, be a string value, or have at least 2 items/
  );

  Assert.throws(
    () => root.choices.foo([1, 2, 3, 4]),
    /be an object value, be a string value, or have at most 3 items/
  );

  Assert.throws(
    () => root.choices.foo({ foo: 12 }),
    /.foo must be a string value, be a string value, or be an array value/
  );

  Assert.throws(
    () => root.choices.foo({ blurg: "foo" }),
    /not contain an unsupported "blurg" property, be a string value, or be an array value/
  );

  Assert.throws(
    () => root.choices.bar({}),
    /contain the required "baz" property, or be an array value/
  );

  Assert.throws(
    () => root.choices.bar({ baz: "x", quux: "y" }),
    /not contain an unexpected "quux" property, or be an array value/
  );

  Assert.throws(
    () => root.choices.bar({ baz: "x", quux: "y", foo: "z" }),
    /not contain the unexpected properties \[foo, quux\], or be an array value/
  );
});

let permissionsJson = [
  {
    namespace: "noPerms",

    types: [],

    functions: [
      {
        name: "noPerms",
        type: "function",
        parameters: [],
      },

      {
        name: "fooPerm",
        type: "function",
        permissions: ["foo"],
        parameters: [],
      },
    ],
  },

  {
    namespace: "fooPerm",

    permissions: ["foo"],

    types: [],

    functions: [
      {
        name: "noPerms",
        type: "function",
        parameters: [],
      },

      {
        name: "fooBarPerm",
        type: "function",
        permissions: ["foo.bar"],
        parameters: [],
      },
    ],
  },
];

add_task(async function testPermissions() {
  let url = "data:," + JSON.stringify(permissionsJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  Schemas.inject(root, wrapper);

  equal(typeof root.noPerms, "object", "noPerms namespace should exist");
  equal(
    typeof root.noPerms.noPerms,
    "function",
    "noPerms.noPerms method should exist"
  );

  equal(
    root.noPerms.fooPerm,
    undefined,
    "noPerms.fooPerm should not method exist"
  );

  equal(root.fooPerm, undefined, "fooPerm namespace should not exist");

  info('Add "foo" permission');
  permissions.add("foo");

  root = {};
  Schemas.inject(root, wrapper);

  equal(typeof root.noPerms, "object", "noPerms namespace should exist");
  equal(
    typeof root.noPerms.noPerms,
    "function",
    "noPerms.noPerms method should exist"
  );
  equal(
    typeof root.noPerms.fooPerm,
    "function",
    "noPerms.fooPerm method should exist"
  );

  equal(typeof root.fooPerm, "object", "fooPerm namespace should exist");
  equal(
    typeof root.fooPerm.noPerms,
    "function",
    "noPerms.noPerms method should exist"
  );

  equal(
    root.fooPerm.fooBarPerm,
    undefined,
    "fooPerm.fooBarPerm method should not exist"
  );

  info('Add "foo.bar" permission');
  permissions.add("foo.bar");

  root = {};
  Schemas.inject(root, wrapper);

  equal(typeof root.noPerms, "object", "noPerms namespace should exist");
  equal(
    typeof root.noPerms.noPerms,
    "function",
    "noPerms.noPerms method should exist"
  );
  equal(
    typeof root.noPerms.fooPerm,
    "function",
    "noPerms.fooPerm method should exist"
  );

  equal(typeof root.fooPerm, "object", "fooPerm namespace should exist");
  equal(
    typeof root.fooPerm.noPerms,
    "function",
    "noPerms.noPerms method should exist"
  );
  equal(
    typeof root.fooPerm.fooBarPerm,
    "function",
    "noPerms.fooBarPerm method should exist"
  );
});

let nestedNamespaceJson = [
  {
    namespace: "nested.namespace",
    types: [
      {
        id: "CustomType",
        type: "object",
        events: [
          {
            name: "onEvent",
            type: "function",
          },
        ],
        properties: {
          url: {
            type: "string",
          },
        },
        functions: [
          {
            name: "functionOnCustomType",
            type: "function",
            parameters: [
              {
                name: "title",
                type: "string",
              },
            ],
          },
        ],
      },
    ],
    properties: {
      instanceOfCustomType: {
        $ref: "CustomType",
      },
    },
    functions: [
      {
        name: "create",
        type: "function",
        parameters: [
          {
            name: "title",
            type: "string",
          },
        ],
      },
    ],
  },
];

add_task(async function testNestedNamespace() {
  let url = "data:," + JSON.stringify(nestedNamespaceJson);

  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  Schemas.inject(root, wrapper);

  talliedErrors.length = 0;

  ok(root.nested, "The root object contains the first namespace level");
  ok(
    root.nested.namespace,
    "The first level object contains the second namespace level"
  );

  ok(
    root.nested.namespace.create,
    "Got the expected function in the nested namespace"
  );
  equal(
    typeof root.nested.namespace.create,
    "function",
    "The property is a function as expected"
  );

  let { instanceOfCustomType } = root.nested.namespace;

  ok(
    instanceOfCustomType,
    "Got the expected instance of the CustomType defined in the schema"
  );
  ok(
    instanceOfCustomType.functionOnCustomType,
    "Got the expected method in the CustomType instance"
  );
  ok(
    instanceOfCustomType.onEvent &&
      instanceOfCustomType.onEvent.addListener &&
      typeof instanceOfCustomType.onEvent.addListener == "function",
    "Got the expected event defined in the CustomType instance"
  );

  instanceOfCustomType.functionOnCustomType("param_value");
  verify(
    "call",
    "nested.namespace.instanceOfCustomType",
    "functionOnCustomType",
    ["param_value"]
  );

  let fakeListener = () => {};
  instanceOfCustomType.onEvent.addListener(fakeListener);
  verify("addListener", "nested.namespace.instanceOfCustomType", "onEvent", [
    fakeListener,
    [],
  ]);
  instanceOfCustomType.onEvent.removeListener(fakeListener);
  verify("removeListener", "nested.namespace.instanceOfCustomType", "onEvent", [
    fakeListener,
  ]);

  // TODO: test support properties in a SubModuleType defined in the schema,
  // once implemented, e.g.:
  // ok("url" in instanceOfCustomType,
  //   "Got the expected property defined in the CustomType instance");
});

let $importJson = [
  {
    namespace: "from_the",
    $import: "future",
  },
  {
    namespace: "future",
    properties: {
      PROP1: { value: "original value" },
      PROP2: { value: "second original" },
    },
    types: [
      {
        id: "Colour",
        type: "string",
        enum: ["red", "white", "blue"],
      },
    ],
    functions: [
      {
        name: "dye",
        type: "function",
        parameters: [{ name: "arg", $ref: "Colour" }],
      },
    ],
  },
  {
    namespace: "embrace",
    $import: "future",
    properties: {
      PROP2: { value: "overridden value" },
    },
    types: [
      {
        id: "Colour",
        type: "string",
        enum: ["blue", "orange"],
      },
    ],
  },
];

add_task(async function test_$import() {
  let url = "data:," + JSON.stringify($importJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  tallied = null;
  Schemas.inject(root, wrapper);
  equal(tallied, null);

  equal(root.from_the.PROP1, "original value", "imported property");
  equal(root.from_the.PROP2, "second original", "second imported property");
  equal(root.from_the.Colour.RED, "red", "imported enum type");
  equal(typeof root.from_the.dye, "function", "imported function");

  root.from_the.dye("white");
  verify("call", "from_the", "dye", ["white"]);

  Assert.throws(
    () => root.from_the.dye("orange"),
    /Invalid enumeration value/,
    "original imported argument type Colour doesn't include 'orange'"
  );

  equal(root.embrace.PROP1, "original value", "imported property");
  equal(root.embrace.PROP2, "overridden value", "overridden property");
  equal(root.embrace.Colour.ORANGE, "orange", "overridden enum type");
  equal(typeof root.embrace.dye, "function", "imported function");

  root.embrace.dye("orange");
  verify("call", "embrace", "dye", ["orange"]);

  Assert.throws(
    () => root.embrace.dye("white"),
    /Invalid enumeration value/,
    "overridden argument type Colour doesn't include 'white'"
  );
});

add_task(async function testLocalAPIImplementation() {
  let countGet2 = 0;
  let countProp3 = 0;
  let countProp3SubFoo = 0;

  let testingApiObj = {
    get PROP1() {
      // PROP1 is a schema-defined constant.
      throw new Error("Unexpected get PROP1");
    },
    get prop2() {
      ++countGet2;
      return "prop2 val";
    },
    get prop3() {
      throw new Error("Unexpected get prop3");
    },
    set prop3(v) {
      // prop3 is a submodule, defined as a function, so the API should not pass
      // through assignment to prop3.
      throw new Error("Unexpected set prop3");
    },
  };
  let submoduleApiObj = {
    get sub_foo() {
      ++countProp3;
      return () => {
        return ++countProp3SubFoo;
      };
    },
  };

  let localWrapper = {
    cloneScope: global,
    shouldInject(ns, name) {
      return name == "testing" || ns == "testing" || ns == "testing.prop3";
    },
    getImplementation(ns, name) {
      Assert.ok(ns == "testing" || ns == "testing.prop3");
      if (ns == "testing.prop3" && name == "sub_foo") {
        // It is fine to use `null` here because we don't call async functions.
        return new LocalAPIImplementation(submoduleApiObj, name, null);
      }
      // It is fine to use `null` here because we don't call async functions.
      return new LocalAPIImplementation(testingApiObj, name, null);
    },
  };

  let root = {};
  Schemas.inject(root, localWrapper);
  Assert.equal(countGet2, 0);
  Assert.equal(countProp3, 0);
  Assert.equal(countProp3SubFoo, 0);

  Assert.equal(root.testing.PROP1, 20);

  Assert.equal(root.testing.prop2, "prop2 val");
  Assert.equal(countGet2, 1);

  Assert.equal(root.testing.prop2, "prop2 val");
  Assert.equal(countGet2, 2);

  info(JSON.stringify(root.testing));
  Assert.equal(root.testing.prop3.sub_foo(), 1);
  Assert.equal(countProp3, 1);
  Assert.equal(countProp3SubFoo, 1);

  Assert.equal(root.testing.prop3.sub_foo(), 2);
  Assert.equal(countProp3, 2);
  Assert.equal(countProp3SubFoo, 2);

  root.testing.prop3.sub_foo = () => {
    return "overwritten";
  };
  Assert.equal(root.testing.prop3.sub_foo(), "overwritten");

  root.testing.prop3 = {
    sub_foo() {
      return "overwritten again";
    },
  };
  Assert.equal(root.testing.prop3.sub_foo(), "overwritten again");
  Assert.equal(countProp3SubFoo, 2);
});

let defaultsJson = [
  {
    namespace: "defaultsJson",

    types: [],

    functions: [
      {
        name: "defaultFoo",
        type: "function",
        parameters: [
          {
            name: "arg",
            type: "object",
            optional: true,
            properties: {
              prop1: { type: "integer", optional: true },
            },
            default: { prop1: 1 },
          },
        ],
        returns: {
          type: "object",
          additionalProperties: true,
        },
      },
    ],
  },
];

add_task(async function testDefaults() {
  let url = "data:," + JSON.stringify(defaultsJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let testingApiObj = {
    defaultFoo: function(arg) {
      if (Object.keys(arg) != "prop1") {
        throw new Error(
          `Received the expected default object, default: ${JSON.stringify(
            arg
          )}`
        );
      }
      arg.newProp = 1;
      return arg;
    },
  };

  let localWrapper = {
    cloneScope: global,
    shouldInject(ns) {
      return true;
    },
    getImplementation(ns, name) {
      return new LocalAPIImplementation(testingApiObj, name, null);
    },
  };

  let root = {};
  Schemas.inject(root, localWrapper);

  deepEqual(root.defaultsJson.defaultFoo(), { prop1: 1, newProp: 1 });
  deepEqual(root.defaultsJson.defaultFoo({ prop1: 2 }), {
    prop1: 2,
    newProp: 1,
  });
  deepEqual(root.defaultsJson.defaultFoo(), { prop1: 1, newProp: 1 });
});

let returnsJson = [
  {
    namespace: "returns",
    types: [
      {
        id: "Widget",
        type: "object",
        properties: {
          size: { type: "integer" },
          colour: { type: "string", optional: true },
        },
      },
    ],
    functions: [
      {
        name: "complete",
        type: "function",
        returns: { $ref: "Widget" },
        parameters: [],
      },
      {
        name: "optional",
        type: "function",
        returns: { $ref: "Widget" },
        parameters: [],
      },
      {
        name: "invalid",
        type: "function",
        returns: { $ref: "Widget" },
        parameters: [],
      },
    ],
  },
];

add_task(async function testReturns() {
  const url = "data:," + JSON.stringify(returnsJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  const apiObject = {
    complete() {
      return { size: 3, colour: "orange" };
    },
    optional() {
      return { size: 4 };
    },
    invalid() {
      return {};
    },
  };

  const localWrapper = {
    cloneScope: global,
    shouldInject(ns) {
      return true;
    },
    getImplementation(ns, name) {
      return new LocalAPIImplementation(apiObject, name, null);
    },
  };

  const root = {};
  Schemas.inject(root, localWrapper);

  deepEqual(root.returns.complete(), { size: 3, colour: "orange" });
  deepEqual(
    root.returns.optional(),
    { size: 4 },
    "Missing optional properties is allowed"
  );

  if (AppConstants.DEBUG) {
    Assert.throws(
      () => root.returns.invalid(),
      /Type error for result value \(Property "size" is required\)/,
      "Should throw for invalid result in DEBUG builds"
    );
  } else {
    deepEqual(
      root.returns.invalid(),
      {},
      "Doesn't throw for invalid result value in release builds"
    );
  }
});

let booleanEnumJson = [
  {
    namespace: "booleanEnum",

    types: [
      {
        id: "enumTrue",
        type: "boolean",
        enum: [true],
      },
    ],
    functions: [
      {
        name: "paramMustBeTrue",
        type: "function",
        parameters: [{ name: "arg", $ref: "enumTrue" }],
      },
    ],
  },
];

add_task(async function testBooleanEnum() {
  let url = "data:," + JSON.stringify(booleanEnumJson);
  Schemas._rootSchema = null;
  await Schemas.load(url);

  let root = {};
  tallied = null;
  Schemas.inject(root, wrapper);
  Assert.equal(tallied, null);

  ok(root.booleanEnum, "namespace exists");
  root.booleanEnum.paramMustBeTrue(true);
  verify("call", "booleanEnum", "paramMustBeTrue", [true]);
  Assert.throws(
    () => root.booleanEnum.paramMustBeTrue(false),
    /Type error for parameter arg \(Invalid value false\) for booleanEnum\.paramMustBeTrue\./,
    "should throw because enum of the type restricts parameter to true"
  );
});
