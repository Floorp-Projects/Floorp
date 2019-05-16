/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime remote object

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

  await testObjectRelease(client, contextId);

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

async function testObjectRelease({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({ contextId, expression: "({ foo: 42 })" });
  is(result.subtype, null, "JS Object have no subtype");
  is(result.type, "object", "The type is correct");
  ok(!!result.objectId, "Got an object id");

  const { result: result2 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "obj => JSON.stringify(obj)",
    arguments: [{ objectId: result.objectId }],
  });
  is(result2.type, "string", "The type is correct");
  is(result2.value, JSON.stringify({ foo: 42 }), "Got the object's JSON");

  const { result: result3 } = await Runtime.callFunctionOn({
    objectId: result.objectId,
    functionDeclaration: "function () { return this.foo; }",
  });
  is(result3.type, "number", "The type is correct");
  is(result3.value, 42, "Got the object's foo attribute");

  await Runtime.releaseObject({
    objectId: result.objectId,
  });
  ok(true, "Object is released");

  try {
    await Runtime.callFunctionOn({
      executionContextId: contextId,
      functionDeclaration: "() => {}",
      arguments: [{ objectId: result.objectId }],
    });
    ok(false, "callFunctionOn with a released object as argument should throw");
  } catch (e) {
    ok(e.message.includes("Cannot find object with ID:"), "callFunctionOn throws on released argument");
  }
  try {
    await Runtime.callFunctionOn({
      objectId: result.objectId,
      functionDeclaration: "() => {}",
    });
    ok(false, "callFunctionOn with a released object as target should throw");
  } catch (e) {
    ok(e.message.includes("Unable to get the context for object with id"), "callFunctionOn throws on released target");
  }
}
