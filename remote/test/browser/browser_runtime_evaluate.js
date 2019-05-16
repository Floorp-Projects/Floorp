/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime execution context events

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

add_task(async function() {
  // Open a test page, to prevent debugging the random default page
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Start the CDP server
  await RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

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
  await testEvaluateInvalidContextId(client, contextId);

  await testCallFunctionOn(client, contextId);
  await testCallFunctionOnInvalidContextId(client, contextId);

  const { Runtime } = client;

  // First test Runtime.evaluate, which accepts an JS expression string.
  // This string may have instructions separated with `;` before ending
  // with a JS value that is returned as a CDP `RemoteObject`.
  function runtimeEvaluate(expression) {
    return Runtime.evaluate({ contextId, expression });
  }

  // Then test Runtime.callFunctionOn, which accepts a JS string, but this
  // time, it has to be a function. In this first test against callFunctionOn,
  // we only assert the returned type and ignore the arguments.
  function callFunctionOn(expression, instruction = false) {
    if (instruction) {
      return Runtime.callFunctionOn({
        executionContextId: contextId,
        functionDeclaration: `() => { ${expression} }`,
      });
    }
    return Runtime.callFunctionOn({
      executionContextId: contextId,
      functionDeclaration: `() => ${expression}`,
    });
  }

  // Finally, run another test against Runtime.callFunctionOn in order to assert
  // the arguments being passed to the executed function.
  async function callFunctionOnArguments(expression, instruction = false) {
    // First evaluate the expression via Runtime.evaluate in order to generate the
    // CDP's `RemoteObject` for the given expression. A previous test already
    // asserted the returned value of Runtime.evaluate, so we can trust this.
    const { result }  = await Runtime.evaluate({ contextId, expression });

    // We then pass this RemoteObject as an argument to Runtime.callFunctionOn.
    return Runtime.callFunctionOn({
      executionContextId: contextId,
      functionDeclaration: `arg => arg`,
      arguments: [result],
    });
  }

  for (const fun of [runtimeEvaluate, callFunctionOn, callFunctionOnArguments]) {
    info("Test " + fun.name);
    await testPrimitiveTypes(fun);
    await testUnserializable(fun);
    await testObjectTypes(fun);

    // Tests involving an instruction (exception throwing, or errors) are not
    // using any argument. So ignore these particular tests.
    if (fun != callFunctionOnArguments) {
      await testThrowError(fun);
      await testThrowValue(fun);
      await testJSError(fun);
    }
  }

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
});

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
  const { result } = await Runtime.evaluate({ contextId, expression: "location.href" });
  is(result.value, TEST_URI, "Runtime.evaluate works and is against the test page");
}

async function testEvaluateInvalidContextId({ Runtime }, contextId) {
  try {
    await Runtime.evaluate({ contextId: -1, expression: "" });
    ok(false, "Evaluate shouldn't pass");
  } catch (e) {
    ok(e.message.includes("Unable to find execution context with id: -1"),
      "Throws with the expected error message");
  }
}

async function testCallFunctionOn({ Runtime }, executionContextId) {
  const { result } = await Runtime.callFunctionOn({ executionContextId, functionDeclaration: "() => location.href" });
  is(result.value, TEST_URI, "Runtime.callFunctionOn works and is against the test page");
}

async function testCallFunctionOnInvalidContextId({ Runtime }, executionContextId) {
  try {
    await Runtime.callFunctionOn({ executionContextId: -1, functionDeclaration: "" });
    ok(false, "callFunctionOn shouldn't pass");
  } catch (e) {
    ok(e.message.includes("Unable to find execution context with id: -1"),
      "Throws with the expected error message");
  }
}

async function testPrimitiveTypes(testFunction) {
  const expressions = [42, "42", true, 4.2];
  for (const expression of expressions) {
    const { result } = await testFunction(JSON.stringify(expression));
    is(result.value, expression, `Evaluating primitive '${expression}' works`);
    is(result.type, typeof(expression), `${expression} type is correct`);
  }

  // undefined doesn't work with JSON.stringify, so test it independently
  let { result } = await testFunction("undefined");
  is(result.value, undefined, "undefined works");
  is(result.type, "undefined", "undefined type is correct");

  // `null` is special as it has its own subtype, is of type 'object' but is returned as
  // a value, without an `objectId` attribute
  ({ result } = await testFunction("null"));
  is(result.value, null, "Evaluating 'null' works");
  is(result.type, "object", "'null' type is correct");
  is(result.subtype, "null", "'null' subtype is correct");
  ok(!result.objectId, "'null' has no objectId");
}

async function testUnserializable(testFunction) {
  const expressions = ["NaN", "-0", "Infinity", "-Infinity"];
  for (const expression of expressions) {
    const { result } = await testFunction(expression);
    is(result.unserializableValue, expression, `Evaluating unserializable '${expression}' works`);
  }
}

async function testObjectTypes(testFunction) {
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
    { expression: "document", type: "object", subtype: "node" },
    { expression: "document.documentElement", type: "object", subtype: "node" },
    { expression: "document.createElement('div')", type: "object", subtype: "node" },
  ];

  for (const { expression, type, subtype } of expressions) {
    const { result } = await testFunction(expression);
    is(result.subtype, subtype, `Evaluating '${expression}' has the expected subtype`);
    is(result.type, type, "The type is correct");
    ok(!!result.objectId, "Got an object id");
  }
}

async function testThrowError(testFunction) {
  const { exceptionDetails } = await testFunction("throw new Error('foo')", true);
  is(exceptionDetails.text, "foo", "Exception message is passed to the client");
}

async function testThrowValue(testFunction) {
  const { exceptionDetails } = await testFunction("throw 'foo'", true);
  is(exceptionDetails.exception.type, "string", "Exception type is correct");
  is(exceptionDetails.exception.value, "foo", "Exception value is passed as a RemoteObject");
}

async function testJSError(testFunction) {
  const { exceptionDetails } = await testFunction("doesNotExists()", true);
  is(exceptionDetails.text, "doesNotExists is not defined", "Exception message is passed to the client");
}
