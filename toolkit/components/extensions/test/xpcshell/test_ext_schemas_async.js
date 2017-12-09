"use strict";

Components.utils.import("resource://gre/modules/ExtensionCommon.jsm");
Components.utils.import("resource://gre/modules/Schemas.jsm");

let {BaseContext, LocalAPIImplementation} = ExtensionCommon;

let schemaJson = [
  {
    namespace: "testnamespace",
    types: [{
      id: "Widget",
      type: "object",
      properties: {
        size: {type: "integer"},
        colour: {type: "string", optional: true},
      },
    }],
    functions: [{
      name: "one_required",
      type: "function",
      parameters: [{
        name: "first",
        type: "function",
        parameters: [],
      }],
    }, {
      name: "one_optional",
      type: "function",
      parameters: [{
        name: "first",
        type: "function",
        parameters: [],
        optional: true,
      }],
    }, {
      name: "async_required",
      type: "function",
      async: "first",
      parameters: [{
        name: "first",
        type: "function",
        parameters: [],
      }],
    }, {
      name: "async_optional",
      type: "function",
      async: "first",
      parameters: [{
        name: "first",
        type: "function",
        parameters: [],
        optional: true,
      }],
    }, {
      name: "async_result",
      type: "function",
      async: "callback",
      parameters: [{
        name: "callback",
        type: "function",
        parameters: [{
          name: "widget",
          $ref: "Widget",
        }],
      }],
    }],
  },
];

const global = this;
class StubContext extends BaseContext {
  constructor() {
    let fakeExtension = {id: "test@web.extension"};
    super("testEnv", fakeExtension);
    this.sandbox = Cu.Sandbox(global);
  }

  get cloneScope() {
    return this.sandbox;
  }

  get principal() {
    return Cu.getObjectPrincipal(this.sandbox);
  }
}

let context;

function generateAPIs(extraWrapper, apiObj) {
  context = new StubContext();
  let localWrapper = {
    cloneScope: global,
    shouldInject() {
      return true;
    },
    getImplementation(namespace, name) {
      return new LocalAPIImplementation(apiObj, name, context);
    },
  };
  Object.assign(localWrapper, extraWrapper);

  let root = {};
  Schemas.inject(root, localWrapper);
  return root.testnamespace;
}

add_task(async function testParameterValidation() {
  await Schemas.load("data:," + JSON.stringify(schemaJson));

  let testnamespace;
  function assertThrows(name, ...args) {
    Assert.throws(() => testnamespace[name](...args),
                  /Incorrect argument types/,
                  `Expected testnamespace.${name}(${args.map(String).join(", ")}) to throw.`);
  }
  function assertNoThrows(name, ...args) {
    try {
      testnamespace[name](...args);
    } catch (e) {
      do_print(`testnamespace.${name}(${args.map(String).join(", ")}) unexpectedly threw.`);
      throw new Error(e);
    }
  }
  let cb = () => {};

  for (let isChromeCompat of [true, false]) {
    do_print(`Testing API validation with isChromeCompat=${isChromeCompat}`);
    testnamespace = generateAPIs({
      isChromeCompat,
    }, {
      one_required() {},
      one_optional() {},
      async_required() {},
      async_optional() {},
    });

    assertThrows("one_required");
    assertThrows("one_required", null);
    assertNoThrows("one_required", cb);
    assertThrows("one_required", cb, null);
    assertThrows("one_required", cb, cb);

    assertNoThrows("one_optional");
    assertNoThrows("one_optional", null);
    assertNoThrows("one_optional", cb);
    assertThrows("one_optional", cb, null);
    assertThrows("one_optional", cb, cb);

    // Schema-based validation happens before an async method is called, so
    // errors should be thrown synchronously.

    // The parameter was declared as required, but there was also an "async"
    // attribute with the same value as the parameter name, so the callback
    // parameter is actually optional.
    assertNoThrows("async_required");
    assertNoThrows("async_required", null);
    assertNoThrows("async_required", cb);
    assertThrows("async_required", cb, null);
    assertThrows("async_required", cb, cb);

    assertNoThrows("async_optional");
    assertNoThrows("async_optional", null);
    assertNoThrows("async_optional", cb);
    assertThrows("async_optional", cb, null);
    assertThrows("async_optional", cb, cb);
  }
});

add_task(async function testCheckAsyncResults() {
  await Schemas.load("data:," + JSON.stringify(schemaJson));

  const complete = generateAPIs({}, {
    async_result: async () => ({size: 5, colour: "green"}),
  });

  const optional = generateAPIs({}, {
    async_result: async () => ({size: 6}),
  });

  const invalid = generateAPIs({}, {
    async_result: async () => ({}),
  });

  deepEqual(await complete.async_result(), {size: 5, colour: "green"});

  deepEqual(await optional.async_result(), {size: 6},
            "Missing optional properties is allowed");

  if (AppConstants.DEBUG) {
    await Assert.rejects(
      invalid.async_result(),
      `Type error for widget value (Property "size" is required)`,
      "Should throw for invalid callback argument in DEBUG builds");
  } else {
    deepEqual(await invalid.async_result(), {},
              "Invalid callback argument doesn't throw in release builds");
  }
});

add_task(async function testAsyncResults() {
  await Schemas.load("data:," + JSON.stringify(schemaJson));
  function runWithCallback(func) {
    do_print(`Calling testnamespace.${func.name}, expecting callback with result`);
    return new Promise(resolve => {
      let result = "uninitialized value";
      let returnValue = func(reply => {
        result = reply;
        resolve(result);
      });
      // When a callback is given, the return value must be missing.
      do_check_eq(returnValue, undefined);
      // Callback must be called asynchronously.
      do_check_eq(result, "uninitialized value");
    });
  }

  function runFailCallback(func) {
    do_print(`Calling testnamespace.${func.name}, expecting callback with error`);
    return new Promise(resolve => {
      func(reply => {
        do_check_eq(reply, undefined);
        resolve(context.lastError.message); // eslint-disable-line no-undef
      });
    });
  }

  for (let isChromeCompat of [true, false]) {
    do_print(`Testing API invocation with isChromeCompat=${isChromeCompat}`);
    let testnamespace = generateAPIs({
      isChromeCompat,
    }, {
      async_required(cb) {
        do_check_eq(cb, undefined);
        return Promise.resolve(1);
      },
      async_optional(cb) {
        do_check_eq(cb, undefined);
        return Promise.resolve(2);
      },
    });
    if (!isChromeCompat) { // No promises for chrome.
      do_print("testnamespace.async_required should be a Promise");
      let promise = testnamespace.async_required();
      do_check_true(promise instanceof context.cloneScope.Promise);
      do_check_eq(await promise, 1);

      do_print("testnamespace.async_optional should be a Promise");
      promise = testnamespace.async_optional();
      do_check_true(promise instanceof context.cloneScope.Promise);
      do_check_eq(await promise, 2);
    }

    do_check_eq(await runWithCallback(testnamespace.async_required), 1);
    do_check_eq(await runWithCallback(testnamespace.async_optional), 2);

    let otherSandbox = Cu.Sandbox(null, {});
    let errorFactories = [
      msg => { throw new context.cloneScope.Error(msg); },
      msg => context.cloneScope.Promise.reject({message: msg}),
      msg => Cu.evalInSandbox(`throw new Error("${msg}")`, otherSandbox),
      msg => Cu.evalInSandbox(`Promise.reject({message: "${msg}"})`, otherSandbox),
    ];
    for (let makeError of errorFactories) {
      do_print(`Testing callback/promise with error caused by: ${makeError}`);
      testnamespace = generateAPIs({
        isChromeCompat,
      }, {
        async_required() { return makeError("ONE"); },
        async_optional() { return makeError("TWO"); },
      });

      if (!isChromeCompat) { // No promises for chrome.
        await Assert.rejects(
          testnamespace.async_required(), /ONE/,
          "should reject testnamespace.async_required()").catch(() => {});
        await Assert.rejects(
          testnamespace.async_optional(), /TWO/,
          "should reject testnamespace.async_optional()").catch(() => {});
      }

      do_check_eq(await runFailCallback(testnamespace.async_required), "ONE");
      do_check_eq(await runFailCallback(testnamespace.async_optional), "TWO");
    }
  }
});
