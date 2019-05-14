/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime.callFunctionOn
// Also see browser_runtime_evaluate as it covers basic usages of this method.

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
  await testObjectReferences(client, contextId);
  await testExceptions(client, contextId);
  await testReturnByValue(client, contextId);
  await testAwaitPromise(client, contextId);
  await testObjectId(client, contextId);

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

async function testObjectReferences({ Runtime }, contextId) {
  // First create a JS object remotely via Runtime.evaluate
  const { result } = await Runtime.evaluate({ contextId, expression: "({ foo: 1 })" });
  is(result.type, "object", "The type is correct");
  is(result.subtype, null, "The subtype is null for objects");
  ok(!!result.objectId, "Got an object id");

  // Then increment the `foo` attribute of this JS object, while returning this
  // attribute value
  const { result: result2 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "arg => ++arg.foo",
    arguments: [{ objectId: result.objectId }],
  });
  is(result2.type, "number", "The type is correct");
  is(result2.subtype, null, "The subtype is null for numbers");
  is(result2.value, 2, "Updated the existing object and returned the incremented value");

  // Finally, try to pass this JS object and get it back. Ensure that it returns
  // the same object id. Also increment the attribute again.
  const { result: result3 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "arg => { arg.foo++; return arg; }",
    arguments: [{ objectId: result.objectId }],
  });
  is(result3.type, "object", "The type is correct");
  is(result3.subtype, null, "The subtype is null for objects");
  // Remote object are not having unique id. So you may have multiple object ids
  // that reference the same remote object
  ok(!!result3.objectId, "Got an object id");
  isnot(result3.objectId, result.objectId, "The object id is stable");

  // Assert that we can still access this object and that its foo attribute
  // has been incremented. Use the second object id we got from previous call
  // to callFunctionOn.
  const { result: result4 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "arg => arg.foo",
    arguments: [{ objectId: result3.objectId }],
  });
  is(result4.type, "number", "The type is correct");
  is(result4.subtype, null, "The subtype is null for numbers");
  is(result4.value, 3, "Updated the existing object and returned the incremented value");
}

async function testExceptions({ Runtime }, executionContextId) {
  // Test error when evaluating the function
  let { exceptionDetails } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "doesNotExists()",
  });
  is(exceptionDetails.text, "doesNotExists is not defined", "Exception message is passed to the client");

  // Test error when calling the function
  ({ exceptionDetails } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => doesNotExists()",
  }));
  is(exceptionDetails.text, "doesNotExists is not defined", "Exception message is passed to the client");
}

async function testReturnByValue({ Runtime }, executionContextId) {
  const values = [
    42,
    "42",
    42.00,
    true,
    false,
    null,
    { foo: true },
    { foo: { bar: 42, str: "str", array: [1, 2, 3] } },
    [ 42, "42", true ],
    [ { foo: true } ],
  ];
  for (const value of values) {
    const { result } = await Runtime.callFunctionOn({
      executionContextId,
      functionDeclaration: "() => (" + JSON.stringify(value) + ")",
      returnByValue: true,
    });
    Assert.deepEqual(result.value, value, "The returned value is the same than the input value");
  }

  // Test undefined individually as JSON.stringify doesn't return a string
  const { result } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => {}",
    returnByValue: true,
  });
  is(result.value, undefined, "The returned value is undefined");
}

async function testAwaitPromise({ Runtime }, executionContextId) {
  // First assert promise resolution with awaitPromise
  let { result } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => Promise.resolve(42)",
    awaitPromise: true,
  });
  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");

  // Also test promise rejection with awaitPromise
  let { exceptionDetails } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => Promise.reject(42)",
    awaitPromise: true,
  });
  is(exceptionDetails.exception.value, 42, "The result is the promise's rejection");

  // Then check delayed promise resolution
  ({ result } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => new Promise(r => setTimeout(() => r(42), 0))",
    awaitPromise: true,
  }));
  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");

  // And delayed promise rejection
  ({ exceptionDetails } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => new Promise((_,r) => setTimeout(() => r(42), 0))",
    awaitPromise: true,
  }));
  is(exceptionDetails.exception.value, 42, "The result is the promise's rejection");

  // Finally assert promise resolution without awaitPromise
  ({ result } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => Promise.resolve(42)",
    awaitPromise: false,
  }));
  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.value, "We do not receive any value");

  // As well as promise rejection without awaitPromise
  ({ result } = await Runtime.callFunctionOn({
    executionContextId,
    functionDeclaration: "() => Promise.reject(42)",
    awaitPromise: false,
  }));
  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.exceptionDetails, "We do not receive any exception");
}

async function testObjectId({ Runtime }, contextId) {
  // First create an object via Runtime.evaluate
  const { result } = await Runtime.evaluate({ contextId, expression: "({ foo: 42 })" });
  is(result.type, "object", "The type is correct");
  is(result.subtype, null, "The subtype is null for objects");
  ok(!!result.objectId, "Got an object id");

  // Then apply a method on this object
  const { result: result2 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "function () { return this.foo; }",
    objectId: result.objectId,
  });
  is(result2.type, "number", "The type is correct");
  is(result2.subtype, null, "The subtype is null for numbers");
  is(result2.value, 42, "We have a good proof that the function was ran against the target object");
}
