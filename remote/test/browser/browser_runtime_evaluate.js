/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global getCDP */

const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");
const {RemoteAgentError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

// Test the Runtime execution context events

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

add_task(async function() {
  try {
    await testCDP();
  } catch (e) {
    // Display better error message with the server side stacktrace
    // if an error happened on the server side:
    if (e.response) {
      throw RemoteAgentError.fromJSON(e.response);
    } else {
      throw e;
    }
  }
});

async function testCDP() {
  // Open a test page, to prevent debugging the random default page
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Start the CDP server
  RemoteAgent.init();
  RemoteAgent.tabs.start();
  RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

  // Retrieve the chrome-remote-interface library object
  const CDP = await getCDP();

  // Connect to the server
  const client = await CDP({
    target(list) {
      // Ensure debugging the right target, i.e. the one for our test tab.
      return list.find(target => target.url == TEST_URI);
    },
  });
  ok(true, "CDP client has been instantiated");

  const firstContext = await testRuntimeEnable(client);
  const contextId = firstContext.id;
  await testEvaluate(client, contextId);
  await testInvalidContextId(client, contextId);
  await testPrimitiveTypes(client, contextId);
  await testUnserializable(client, contextId);
  await testObjectTypes(client, contextId);
  await testThrowError(client, contextId);
  await testThrowValue(client, contextId);
  await testJSError(client, contextId);

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
}

async function testRuntimeEnable({ Runtime }) {
  // Enable watching for new execution context
  await Runtime.enable();
  ok(true, "Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await Runtime.executionContextCreated();
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  return context;
}

async function testEvaluate({ Runtime }, contextId) {
  let { result } = await Runtime.evaluate({ contextId, expression: "location.href" });
  is(result.value, TEST_URI, "Runtime.evaluate works and is against the test page");
}

async function testInvalidContextId({ Runtime }, contextId) {
  try {
    await Runtime.evaluate({ contextId: -1, expression: "" });
    ok(false, "Evaluate shouldn't pass");
  } catch (e) {
    ok(e.message.includes("Unable to find execution context with id: -1"),
      "Throws with the expected error message");
  }
}

async function testPrimitiveTypes({ Runtime }, contextId) {
  const expressions = [42, "42", true, 4.2];
  for (const expression of expressions) {
    const { result } = await Runtime.evaluate({ contextId, expression: JSON.stringify(expression) });
    is(result.value, expression, `Evaluating primitive '${expression}' works`);
    is(result.type, typeof(expression), `${expression} type is correct`);
  }

  // undefined doesn't work with JSON.stringify, so test it independently
  let { result } = await Runtime.evaluate({ contextId, expression: "undefined" });
  is(result.value, undefined, "undefined works");
  is(result.type, "undefined", "undefined type is correct");

  // `null` is special as it has its own subtype, is of type 'object' but is returned as
  // a value, without an `objectId` attribute
  ({ result } = await Runtime.evaluate({ contextId, expression: "null" }));
  is(result.value, null, "Evaluating 'null' works");
  is(result.type, "object", "'null' type is correct");
  is(result.subtype, "null", "'null' subtype is correct");
  ok(!result.objectId, "'null' has no objectId");
}

async function testUnserializable({ Runtime }, contextId) {
  const expressions = ["NaN", "-0", "Infinity", "-Infinity"];
  for (const expression of expressions) {
    const { result } = await Runtime.evaluate({ contextId, expression });
    is(result.unserializableValue, expression, `Evaluating unserializable '${expression}' works`);
  }
}

async function testObjectTypes({ Runtime }, contextId) {
  const expressions = [
    { expression: "({foo:true})", type: "object", subtype: null },
    { expression: "Symbol('foo')", type: "symbol", subtype: null },
    { expression: "BigInt(42)", type: "bigint", subtype: null },
    { expression: "new Promise(()=>{})", type: "object", subtype: "promise" },
    { expression: "new Int8Array(8)", type: "object", subtype: "typedarray" },
    { expression: "new WeakMap()", type: "object", subtype: "weakmap" },
    { expression: "new WeakSet()", type: "object", subtype: "weakset" },
    { expression: "new Map()", type: "object", subtype: "map" },
    { expression: "new Set()", type: "object", subtype: "set" },
    { expression: "/foo/", type: "object", subtype: "regexp" },
    { expression: "[1, 2]", type: "object", subtype: "array" },
    { expression: "new Proxy({}, {})", type: "object", subtype: "proxy" },
    { expression: "new Date()", type: "object", subtype: "date" },
  ];

  for (const { expression, type, subtype } of expressions) {
    const { result } = await Runtime.evaluate({ contextId, expression });
    is(result.subtype, subtype, `Evaluating '${expression}' has the expected subtype`);
    is(result.type, type, "The type is correct");
    ok(!!result.objectId, "Got an object id");
  }
}

async function testThrowError({ Runtime }, contextId) {
  const { exceptionDetails } = await Runtime.evaluate({ contextId, expression: "throw new Error('foo')" });
  is(exceptionDetails.text, "foo", "Exception message is passed to the client");
}

async function testThrowValue({ Runtime }, contextId) {
  const { exceptionDetails } = await Runtime.evaluate({ contextId, expression: "throw 'foo'" });
  is(exceptionDetails.exception.type, "string", "Exception type is correct");
  is(exceptionDetails.exception.value, "foo", "Exception value is passed as a RemoteObject");
}

async function testJSError({ Runtime }, contextId) {
  const { exceptionDetails } = await Runtime.evaluate({ contextId, expression: "doesNotExists()" });
  is(exceptionDetails.text, "doesNotExists is not defined", "Exception message is passed to the client");
}
