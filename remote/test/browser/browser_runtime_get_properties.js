/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime remote object

add_task(async function() {
  const { client } = await setup();

  const firstContext = await testRuntimeEnable(client);
  const contextId = firstContext.id;

  await testGetOwnSimpleProperties(client, contextId);
  await testGetCustomProperty(client, contextId);
  await testGetPrototypeProperties(client, contextId);
  await testGetGetterSetterProperties(client, contextId);

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

async function testGetOwnSimpleProperties({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({
    contextId,
    expression: "({ bool: true, fun() {}, int: 1, object: {}, string: 'foo' })",
  });
  is(result.subtype, null, "JS Object have no subtype");
  is(result.type, "object", "The type is correct");
  ok(!!result.objectId, "Got an object id");

  const { result: result2 } = await Runtime.getProperties({
    objectId: result.objectId,
    ownProperties: true,
  });
  is(
    result2.length,
    5,
    "ownProperties=true allows to iterate only over direct object properties (i.e. ignore prototype)"
  );
  result2.sort((a, b) => a.name > b.name);
  is(result2[0].name, "bool");
  is(result2[0].configurable, true);
  is(result2[0].enumerable, true);
  is(result2[0].writable, true);
  is(result2[0].value.type, "boolean");
  is(result2[0].value.value, true);
  is(result2[0].isOwn, true);

  is(result2[1].name, "fun");
  is(result2[1].configurable, true);
  is(result2[1].enumerable, true);
  is(result2[1].writable, true);
  is(result2[1].value.type, "function");
  ok(!!result2[1].value.objectId);
  is(result2[1].isOwn, true);

  is(result2[2].name, "int");
  is(result2[2].configurable, true);
  is(result2[2].enumerable, true);
  is(result2[2].writable, true);
  is(result2[2].value.type, "number");
  is(result2[2].value.value, 1);
  is(result2[2].isOwn, true);

  is(result2[3].name, "object");
  is(result2[3].configurable, true);
  is(result2[3].enumerable, true);
  is(result2[3].writable, true);
  is(result2[3].value.type, "object");
  ok(!!result2[3].value.objectId);
  is(result2[3].isOwn, true);

  is(result2[4].name, "string");
  is(result2[4].configurable, true);
  is(result2[4].enumerable, true);
  is(result2[4].writable, true);
  is(result2[4].value.type, "string");
  is(result2[4].value.value, "foo");
  is(result2[4].isOwn, true);
}

async function testGetPrototypeProperties({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({
    contextId,
    expression: "({ foo: 42 })",
  });
  is(result.subtype, null, "JS Object have no subtype");
  is(result.type, "object", "The type is correct");
  ok(!!result.objectId, "Got an object id");

  const { result: result2 } = await Runtime.getProperties({
    objectId: result.objectId,
    ownProperties: false,
  });
  ok(result2.length > 1, "We have more properties than just the object one");
  const foo = result2.find(p => p.name == "foo");
  ok(foo, "The object property is described");
  ok(foo.isOwn, "and is reported as 'own' property");

  const toString = result2.find(p => p.name == "toString");
  ok(
    toString,
    "Function from Object's prototype are also described like toString"
  );
  ok(!toString.isOwn, "but are reported as not being an 'own' property");
}

async function testGetGetterSetterProperties({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({
    contextId,
    expression:
      "({ get prop() { return this.x; }, set prop(v) { this.x = v; } })",
  });
  is(result.subtype, null, "JS Object have no subtype");
  is(result.type, "object", "The type is correct");
  ok(!!result.objectId, "Got an object id");

  const { result: result2 } = await Runtime.getProperties({
    objectId: result.objectId,
    ownProperties: true,
  });
  is(result2.length, 1);

  is(result2[0].name, "prop");
  is(result2[0].configurable, true);
  is(result2[0].enumerable, true);
  is(
    result2[0].writable,
    undefined,
    "writable is only set for data properties"
  );

  is(result2[0].get.type, "function");
  ok(!!result2[0].get.objectId);
  is(result2[0].set.type, "function");
  ok(!!result2[0].set.objectId);

  is(result2[0].isOwn, true);

  const { result: result3 } = await Runtime.callFunctionOn({
    executionContextId: contextId,
    functionDeclaration: "(set, get) => { set(42); return get(); }",
    arguments: [
      { objectId: result2[0].set.objectId },
      { objectId: result2[0].get.objectId },
    ],
  });
  is(result3.type, "number", "The type is correct");
  is(result3.subtype, null, "The subtype is null for numbers");
  is(result3.value, 42, "The getter returned the value set by the setter");
}

async function testGetCustomProperty({ Runtime }, contextId) {
  const { result } = await Runtime.evaluate({
    contextId,
    expression: `const obj = {}; Object.defineProperty(obj, "prop", { value: 42 }); obj`,
  });
  is(result.subtype, null, "JS Object have no subtype");
  is(result.type, "object", "The type is correct");
  ok(!!result.objectId, "Got an object id");

  const { result: result2 } = await Runtime.getProperties({
    objectId: result.objectId,
    ownProperties: true,
  });
  is(result2.length, 1, "We only get the one object's property");
  is(result2[0].name, "prop");
  is(result2[0].configurable, false);
  is(result2[0].enumerable, false);
  is(result2[0].writable, false);
  is(result2[0].value.type, "number");
  is(result2[0].value.value, 42);
  is(result2[0].isOwn, true);
}
