/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOC = toDataURL("default-test-page");

add_task(async function contextIdInvalidValue({ client }) {
  const { Runtime } = client;

  let errorThrown = "";
  try {
    await Runtime.evaluate({ expression: "", contextId: -1 });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(errorThrown.includes("Cannot find context with specified id"));
});

add_task(async function contextIdNotSpecified({ client }) {
  const { Runtime } = client;

  await loadURL(TEST_DOC);
  await enableRuntime(client);

  const { result } = await Runtime.evaluate({ expression: "location.href" });
  is(result.value, TEST_DOC, "Works against the current document");
});

add_task(async function contextIdSpecified({ client }) {
  const { Runtime } = client;

  await loadURL(TEST_DOC);
  const contextId = await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "location.href",
    contextId,
  });
  is(result.value, TEST_DOC, "Works against the targetted document");
});

add_task(async function returnAsObjectTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

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
    {
      expression: "document.createElement('div')",
      type: "object",
      subtype: "node",
    },
  ];

  for (const { expression, type, subtype } of expressions) {
    const { result } = await Runtime.evaluate({ expression });
    is(
      result.subtype,
      subtype,
      `Evaluating '${expression}' has the expected subtype`
    );
    is(result.type, type, "The type is correct");
    ok(!!result.objectId, "Got an object id");
  }
});

add_task(async function returnAsObjectPrimitiveTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const expressions = [42, "42", true, 4.2];
  for (const expression of expressions) {
    const { result } = await Runtime.evaluate({
      expression: JSON.stringify(expression),
    });
    is(result.value, expression, `Evaluating primitive '${expression}' works`);
    is(result.type, typeof expression, `${expression} type is correct`);
  }
});

add_task(async function returnAsObjectNotSerializable({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const expressions = ["-0", "NaN", "Infinity", "-Infinity"];
  for (const expression of expressions) {
    const { result } = await Runtime.evaluate({ expression });
    Assert.deepEqual(
      result,
      {
        unserializableValue: expression,
      },
      `Evaluating unserializable '${expression}' works`
    );
  }
});

// `null` is special as it has its own subtype, is of type 'object'
// but is returned as a value, without an `objectId` attribute
add_task(async function returnAsObjectNull({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "null",
  });
  Assert.deepEqual(
    result,
    {
      type: "object",
      subtype: "null",
      value: null,
    },
    "Null type is correct"
  );
});

// undefined doesn't work with JSON.stringify, so test it independently
add_task(async function returnAsObjectUndefined({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "undefined",
  });
  Assert.deepEqual(
    result,
    {
      type: "undefined",
    },
    "Undefined type is correct"
  );
});

add_task(async function exceptionDetailsJavascriptError({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression: "doesNotExists()",
  });

  Assert.deepEqual(
    exceptionDetails,
    {
      text: "doesNotExists is not defined",
    },
    "Javascript error is passed to the client"
  );
});

add_task(async function exceptionDetailsThrowError({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression: "throw new Error('foo')",
  });

  Assert.deepEqual(
    exceptionDetails,
    {
      text: "foo",
    },
    "Exception details are passed to the client"
  );
});

add_task(async function exceptionDetailsThrowValue({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression: "throw 'foo'",
  });

  Assert.deepEqual(
    exceptionDetails,
    {
      exception: {
        type: "string",
        value: "foo",
      },
    },
    "Exception details are passed as a RemoteObject"
  );
});

async function enableRuntime(client) {
  const { Runtime } = client;

  // Enable watching for new execution context
  await Runtime.enable();
  info("Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await Runtime.executionContextCreated();
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  return context.id;
}
