"use strict";

Components.utils.import("resource://gre/modules/Schemas.jsm");
Components.utils.import("resource://gre/modules/BrowserUtils.jsm");

let json = [
  {namespace: "testing",

   properties: {
     PROP1: {value: 20},
     prop2: {type: "string"},
   },

   types: [
     {
       id: "type1",
       type: "string",
       "enum": ["value1", "value2", "value3"],
     },

     {
       id: "type2",
       type: "object",
       properties: {
         prop1: {type: "integer"},
         prop2: {type: "array", items: {"$ref": "type1"}},
       },
     },

     {
       id: "basetype1",
       type: "object",
       properties: {
         prop1: {type: "string"},
       },
     },

     {
       id: "basetype2",
       choices: [
         {type: "integer"},
       ],
     },

     {
       $extend: "basetype1",
       properties: {
         prop2: {type: "string"},
       },
     },

     {
       $extend: "basetype2",
       choices: [
         {type: "string"},
       ],
     },
   ],

   functions: [
     {
       name: "foo",
       type: "function",
       parameters: [
         {name: "arg1", type: "integer", optional: true},
         {name: "arg2", type: "boolean", optional: true},
       ],
     },

     {
       name: "bar",
       type: "function",
       parameters: [
         {name: "arg1", type: "integer", optional: true},
         {name: "arg2", type: "boolean"},
       ],
     },

     {
       name: "baz",
       type: "function",
       parameters: [
         {name: "arg1", type: "object", properties: {
           prop1: {type: "string"},
           prop2: {type: "integer", optional: true},
           prop3: {type: "integer", unsupported: true},
         }},
       ],
     },

     {
       name: "qux",
       type: "function",
       parameters: [
         {name: "arg1", "$ref": "type1"},
       ],
     },

     {
       name: "quack",
       type: "function",
       parameters: [
         {name: "arg1", "$ref": "type2"},
       ],
     },

     {
       name: "quora",
       type: "function",
       parameters: [
         {name: "arg1", type: "function"},
       ],
     },

     {
       name: "quileute",
       type: "function",
       parameters: [
         {name: "arg1", type: "integer", optional: true},
         {name: "arg2", type: "integer"},
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
         {name: "obj", type: "object", properties: [], additionalProperties: {type: "integer"}},
       ],
     },

     {
       name: "quasar",
       type: "function",
       parameters: [
         {name: "abc", type: "object", properties: {
           func: {type: "function", parameters: [
             {name: "x", type: "integer"},
           ]},
         }},
       ],
     },

     {
       name: "quosimodo",
       type: "function",
       parameters: [
         {name: "xyz", type: "object", additionalProperties: {type: "any"}},
       ],
     },

     {
       name: "patternprop",
       type: "function",
       parameters: [
         {
           name: "obj",
           type: "object",
           properties: {"prop1": {type: "string", pattern: "^\\d+$"}},
           patternProperties: {
             "(?i)^prop\\d+$": {type: "string"},
             "^foo\\d+$": {type: "string"},
           },
         },
       ],
     },

     {
       name: "pattern",
       type: "function",
       parameters: [
         {name: "arg", type: "string", pattern: "(?i)^[0-9a-f]+$"},
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
             url: {type: "string", "format": "url", "optional": true},
             relativeUrl: {type: "string", "format": "relativeUrl", "optional": true},
             strictRelativeUrl: {type: "string", "format": "strictRelativeUrl", "optional": true},
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
                           required: {type: "integer"},
                           optional: {type: "string", optional: true},
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
       name: "extended1",
       type: "function",
       parameters: [
         {name: "val", $ref: "basetype1"},
       ],
     },

     {
       name: "extended2",
       type: "function",
       parameters: [
         {name: "val", $ref: "basetype2"},
       ],
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
       extraParameters: [{
         name: "filter",
         type: "integer",
       }],
     },
   ],
  },
];

let tallied = null;

function tally(kind, ns, name, args) {
  tallied = [kind, ns, name, args];
}

function verify(...args) {
  do_check_eq(JSON.stringify(tallied), JSON.stringify(args));
  tallied = null;
}

let talliedErrors = [];

function checkErrors(errors) {
  do_check_eq(talliedErrors.length, errors.length, "Got expected number of errors");
  for (let [i, error] of errors.entries()) {
    do_check_true(i in talliedErrors && talliedErrors[i].includes(error),
                  `${JSON.stringify(error)} is a substring of error ${JSON.stringify(talliedErrors[i])}`);
  }

  talliedErrors.length = 0;
}

let wrapper = {
  url: "moz-extension://b66e3509-cdb3-44f6-8eb8-c8b39b3a1d27/",

  checkLoadURL(url) {
    return !url.startsWith("chrome:");
  },

  logError(message) {
    talliedErrors.push(message);
  },

  callFunction(ns, name, args) {
    tally("call", ns, name, args);
  },

  getProperty(ns, name) {
    tally("get", ns, name);
  },

  setProperty(ns, name, value) {
    tally("set", ns, name, value);
  },

  addListener(ns, name, listener, args) {
    tally("addListener", ns, name, [listener, args]);
  },
  removeListener(ns, name, listener) {
    tally("removeListener", ns, name, [listener]);
  },
  hasListener(ns, name, listener) {
    tally("hasListener", ns, name, [listener]);
  },
};

add_task(function* () {
  let url = "data:," + JSON.stringify(json);
  let uri = BrowserUtils.makeURI(url);
  yield Schemas.load(uri);

  let root = {};
  Schemas.inject(root, wrapper);

  do_check_eq(root.testing.PROP1, 20, "simple value property");
  do_check_eq(root.testing.type1.VALUE1, "value1", "enum type");
  do_check_eq(root.testing.type1.VALUE2, "value2", "enum type");

  root.testing.foo(11, true);
  verify("call", "testing", "foo", [11, true]);

  root.testing.foo(true);
  verify("call", "testing", "foo", [null, true]);

  root.testing.foo(null, true);
  verify("call", "testing", "foo", [null, true]);

  root.testing.foo(undefined, true);
  verify("call", "testing", "foo", [null, true]);

  root.testing.foo(11);
  verify("call", "testing", "foo", [11, null]);

  Assert.throws(() => root.testing.bar(11),
                /Incorrect argument types/,
                "should throw without required arg");

  Assert.throws(() => root.testing.bar(11, true, 10),
                /Incorrect argument types/,
                "should throw with too many arguments");

  root.testing.bar(true);
  verify("call", "testing", "bar", [null, true]);

  root.testing.baz({ prop1: "hello", prop2: 22 });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: 22 }]);

  root.testing.baz({ prop1: "hello" });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: null }]);

  root.testing.baz({ prop1: "hello", prop2: null });
  verify("call", "testing", "baz", [{ prop1: "hello", prop2: null }]);

  Assert.throws(() => root.testing.baz({ prop2: 12 }),
                /Property "prop1" is required/,
                "should throw without required property");

  Assert.throws(() => root.testing.baz({ prop1: "hi", prop3: 12 }),
                /Property "prop3" is unsupported by Firefox/,
                "should throw with unsupported property");

  Assert.throws(() => root.testing.baz({ prop1: "hi", prop4: 12 }),
                /Unexpected property "prop4"/,
                "should throw with unexpected property");

  Assert.throws(() => root.testing.baz({ prop1: 12 }),
                /Expected string instead of 12/,
                "should throw with wrong type");

  root.testing.qux("value2");
  verify("call", "testing", "qux", ["value2"]);

  Assert.throws(() => root.testing.qux("value4"),
                /Invalid enumeration value "value4"/,
                "should throw for invalid enum value");

  root.testing.quack({prop1: 12, prop2: ["value1", "value3"]});
  verify("call", "testing", "quack", [{prop1: 12, prop2: ["value1", "value3"]}]);

  Assert.throws(() => root.testing.quack({prop1: 12, prop2: ["value1", "value3", "value4"]}),
                /Invalid enumeration value "value4"/,
                "should throw for invalid array type");

  function f() {}
  root.testing.quora(f);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["call", "testing", "quora"]));
  do_check_eq(tallied[3][0], f);
  tallied = null;

  let g = () => 0;
  root.testing.quora(g);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["call", "testing", "quora"]));
  do_check_eq(tallied[3][0], g);
  tallied = null;

  root.testing.quileute(10);
  verify("call", "testing", "quileute", [null, 10]);

  Assert.throws(() => root.testing.queets(),
                /queets is not a function/,
                "should throw for unsupported functions");

  root.testing.quintuplets({a: 10, b: 20, c: 30});
  verify("call", "testing", "quintuplets", [{a: 10, b: 20, c: 30}]);

  Assert.throws(() => root.testing.quintuplets({a: 10, b: 20, c: 30, d: "hi"}),
                /Expected integer instead of "hi"/,
                "should throw for wrong additionalProperties type");

  root.testing.quasar({func: f});
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["call", "testing", "quasar"]));
  do_check_eq(tallied[3][0].func, f);
  tallied = null;

  root.testing.quosimodo({a: 10, b: 20, c: 30});
  verify("call", "testing", "quosimodo", [{a: 10, b: 20, c: 30}]);
  tallied = null;

  Assert.throws(() => root.testing.quosimodo(10),
                /Incorrect argument types/,
                "should throw for wrong type");

  root.testing.patternprop({prop1: "12", prop2: "42", Prop3: "43", foo1: "x"});
  verify("call", "testing", "patternprop", [{prop1: "12", prop2: "42", Prop3: "43", foo1: "x"}]);
  tallied = null;

  root.testing.patternprop({prop1: "12"});
  verify("call", "testing", "patternprop", [{prop1: "12"}]);
  tallied = null;

  Assert.throws(() => root.testing.patternprop({prop1: "12", foo1: null}),
                /Expected string instead of null/,
                "should throw for wrong property type");

  Assert.throws(() => root.testing.patternprop({prop1: "xx", prop2: "yy"}),
                /String "xx" must match \/\^\\d\+\$\//,
                "should throw for wrong property type");

  Assert.throws(() => root.testing.patternprop({prop1: "12", prop2: 42}),
                /Expected string instead of 42/,
                "should throw for wrong property type");

  Assert.throws(() => root.testing.patternprop({prop1: "12", prop2: null}),
                /Expected string instead of null/,
                "should throw for wrong property type");

  Assert.throws(() => root.testing.patternprop({prop1: "12", propx: "42"}),
                /Unexpected property "propx"/,
                "should throw for unexpected property");

  Assert.throws(() => root.testing.patternprop({prop1: "12", Foo1: "x"}),
                /Unexpected property "Foo1"/,
                "should throw for unexpected property");

  root.testing.pattern("DEADbeef");
  verify("call", "testing", "pattern", ["DEADbeef"]);
  tallied = null;

  Assert.throws(() => root.testing.pattern("DEADcow"),
                /String "DEADcow" must match \/\^\[0-9a-f\]\+\$\/i/,
                "should throw for non-match");

  root.testing.format({url: "http://foo/bar",
                       relativeUrl: "http://foo/bar"});
  verify("call", "testing", "format", [{url: "http://foo/bar",
                                        relativeUrl: "http://foo/bar",
                                        strictRelativeUrl: null}]);
  tallied = null;

  root.testing.format({relativeUrl: "foo.html", strictRelativeUrl: "foo.html"});
  verify("call", "testing", "format", [{url: null,
                                        relativeUrl: `${wrapper.url}foo.html`,
                                        strictRelativeUrl: `${wrapper.url}foo.html`}]);
  tallied = null;

  for (let format of ["url", "relativeUrl"]) {
    Assert.throws(() => root.testing.format({[format]: "chrome://foo/content/"}),
                  /Access denied/,
                  "should throw for access denied");
  }

  for (let url of ["//foo.html", "http://foo/bar.html"]) {
    Assert.throws(() => root.testing.format({strictRelativeUrl: url}),
                  /must be a relative URL/,
                  "should throw for non-relative URL");
  }

  root.testing.deep({foo: {bar: [{baz: {required: 12, optional: "42"}}]}});
  verify("call", "testing", "deep", [{foo: {bar: [{baz: {required: 12, optional: "42"}}]}}]);
  tallied = null;

  Assert.throws(() => root.testing.deep({foo: {bar: [{baz: {optional: "42"}}]}}),
                /Type error for parameter arg \(Error processing foo\.bar\.0\.baz: Property "required" is required\) for testing\.deep/,
                "should throw with the correct object path");

  Assert.throws(() => root.testing.deep({foo: {bar: [{baz: {required: 12, optional: 42}}]}}),
                /Type error for parameter arg \(Error processing foo\.bar\.0\.baz\.optional: Expected string instead of 42\) for testing\.deep/,
                "should throw with the correct object path");

  root.testing.onFoo.addListener(f);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["addListener", "testing", "onFoo"]));
  do_check_eq(tallied[3][0], f);
  do_check_eq(JSON.stringify(tallied[3][1]), JSON.stringify([]));
  tallied = null;

  root.testing.onFoo.removeListener(f);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["removeListener", "testing", "onFoo"]));
  do_check_eq(tallied[3][0], f);
  tallied = null;

  root.testing.onFoo.hasListener(f);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["hasListener", "testing", "onFoo"]));
  do_check_eq(tallied[3][0], f);
  tallied = null;

  Assert.throws(() => root.testing.onFoo.addListener(10),
                /Invalid listener/,
                "addListener with non-function should throw");

  root.testing.onBar.addListener(f, 10);
  do_check_eq(JSON.stringify(tallied.slice(0, -1)), JSON.stringify(["addListener", "testing", "onBar"]));
  do_check_eq(tallied[3][0], f);
  do_check_eq(JSON.stringify(tallied[3][1]), JSON.stringify([10]));
  tallied = null;

  Assert.throws(() => root.testing.onBar.addListener(f, "hi"),
                /Incorrect argument types/,
                "addListener with wrong extra parameter should throw");

  let target = {prop1: 12, prop2: ["value1", "value3"]};
  let proxy = new Proxy(target, {});
  Assert.throws(() => root.testing.quack(proxy),
                /Expected a plain JavaScript object, got a Proxy/,
                "should throw when passing a Proxy");

  if (Symbol.toStringTag) {
    let target = {prop1: 12, prop2: ["value1", "value3"]};
    target[Symbol.toStringTag] = () => "[object Object]";
    let proxy = new Proxy(target, {});
    Assert.throws(() => root.testing.quack(proxy),
                  /Expected a plain JavaScript object, got a Proxy/,
                  "should throw when passing a Proxy");
  }


  root.testing.extended1({prop1: "foo", prop2: "bar"});
  verify("call", "testing", "extended1", [{prop1: "foo", prop2: "bar"}]);
  tallied = null;

  Assert.throws(() => root.testing.extended1({prop1: "foo", prop2: 12}),
                /Expected string instead of 12/,
                "should throw for wrong property type");

  Assert.throws(() => root.testing.extended1({prop1: "foo"}),
                /Property "prop2" is required/,
                "should throw for missing property");

  Assert.throws(() => root.testing.extended1({prop1: "foo", prop2: "bar", prop3: "xxx"}),
                /Unexpected property "prop3"/,
                "should throw for extra property");


  root.testing.extended2("foo");
  verify("call", "testing", "extended2", ["foo"]);
  tallied = null;

  root.testing.extended2(12);
  verify("call", "testing", "extended2", [12]);
  tallied = null;

  Assert.throws(() => root.testing.extended2(true),
                /Incorrect argument types/,
                "should throw for wrong argument type");
});

let deprecatedJson = [
  {namespace: "deprecated",

   properties: {
     accessor: {
       type: "string",
       writable: true,
       deprecated: "This is not the property you are looking for",
     },
   },

   types: [
     {
       "id": "Type",
       "type": "string",
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
       parameters: [
       ],
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

add_task(function* testDeprecation() {
  let url = "data:," + JSON.stringify(deprecatedJson);
  let uri = BrowserUtils.makeURI(url);
  yield Schemas.load(uri);

  let root = {};
  Schemas.inject(root, wrapper);

  talliedErrors.length = 0;


  root.deprecated.property({foo: "bar", xxx: "any", yyy: "property"});
  verify("call", "deprecated", "property", [{foo: "bar", xxx: "any", yyy: "property"}]);
  checkErrors([
    "Error processing xxx: Unknown property",
    "Error processing yyy: Unknown property",
  ]);

  root.deprecated.value(12);
  verify("call", "deprecated", "value", [12]);
  checkErrors([]);

  root.deprecated.value("12");
  verify("call", "deprecated", "value", ["12"]);
  checkErrors(["Please use an integer, not \"12\""]);

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
});
