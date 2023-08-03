/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime remote object

add_task(async function ({ client }) {
  const firstContext = await testRuntimeEnable(client);
  const contextId = firstContext.id;

  await testObjectRelease(client, contextId);
});

async function testRuntimeEnable({ Runtime }) {
  // Enable watching for new execution context
  await Runtime.enable();
  info("Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await Runtime.executionContextCreated();
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  return context;
}

async function testObjectRelease({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({
    contextId,
    expression: "({ foo: 42 })",
  });
  is(result.subtype, undefined, "JS Object has no subtype");
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
  info("Object is released");

  await Assert.rejects(
    Runtime.callFunctionOn({
      executionContextId: contextId,
      functionDeclaration: "() => {}",
      arguments: [{ objectId: result.objectId }],
    }),
    err => err.message.includes("Could not find object with given id"),
    "callFunctionOn throws on released argument"
  );

  await Assert.rejects(
    Runtime.callFunctionOn({
      objectId: result.objectId,
      functionDeclaration: "() => {}",
    }),
    err => err.message.includes("Cannot find context with specified id"),
    "callFunctionOn throws on released target"
  );
}
