/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOC = toDataURL("default-test-page");

add_task(async function awaitPromiseInvalidTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  for (const awaitPromise of [null, 1, "foo", [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.evaluate({
        expression: "",
        awaitPromise,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("awaitPromise: boolean value expected"));
  }
});

add_task(async function awaitPromiseResolve({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "Promise.resolve(42)",
    awaitPromise: true,
  });

  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");
});

add_task(async function awaitPromiseReject({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression: "Promise.reject(42)",
    awaitPromise: true,
  });
  // TODO: Implement all values for exceptionDetails (bug 1548480)
  is(
    exceptionDetails.exception.value,
    42,
    "The result is the promise's rejection"
  );
});

add_task(async function awaitPromiseDelayedResolve({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "new Promise(r => setTimeout(() => r(42), 0))",
    awaitPromise: true,
  });
  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");
});

add_task(async function awaitPromiseDelayedReject({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression: "new Promise((_,r) => setTimeout(() => r(42), 0))",
    awaitPromise: true,
  });
  is(
    exceptionDetails.exception.value,
    42,
    "The result is the promise's rejection"
  );
});

add_task(async function awaitPromiseResolveWithoutWait({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "Promise.resolve(42)",
    awaitPromise: false,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.value, "We do not receive any value");
});

add_task(async function awaitPromiseDelayedResolveWithoutWait({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "new Promise(r => setTimeout(() => r(42), 0))",
    awaitPromise: false,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.value, "We do not receive any value");
});

add_task(async function awaitPromiseRejectWithoutWait({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "Promise.reject(42)",
    awaitPromise: false,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.exceptionDetails, "We do not receive any exception");
});

add_task(async function awaitPromiseDelayedRejectWithoutWait({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "new Promise((_,r) => setTimeout(() => r(42), 0))",
    awaitPromise: false,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.exceptionDetails, "We do not receive any exception");
});

add_task(async function contextIdInvalidValue({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

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

add_task(async function returnAsObjectDifferentObjectIds({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const expressions = [{}, "document"];
  for (const expression of expressions) {
    const { result: result1 } = await Runtime.evaluate({
      expression: JSON.stringify(expression),
    });
    const { result: result2 } = await Runtime.evaluate({
      expression: JSON.stringify(expression),
    });
    is(
      result1.objectId,
      result2.objectId,
      `Different object ids returned for ${expression}`
    );
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

  const notSerializableNumbers = {
    number: ["-0", "NaN", "Infinity", "-Infinity"],
    bigint: ["42n"],
  };

  for (const type in notSerializableNumbers) {
    for (const expression of notSerializableNumbers[type]) {
      const { result } = await Runtime.evaluate({ expression });
      Assert.deepEqual(
        result,
        {
          type,
          unserializableValue: expression,
          description: expression,
        },
        `Evaluating unserializable '${expression}' works`
      );
    }
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

add_task(async function returnByValueInvalidTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  for (const returnByValue of [null, 1, "foo", [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.evaluate({
        expression: "",
        returnByValue,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("returnByValue: boolean value expected"));
  }
});

add_task(async function returnByValue({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const values = [
    null,
    42,
    42.0,
    "42",
    true,
    false,
    { foo: true },
    { foo: { bar: 42, str: "str", array: [1, 2, 3] } },
    [42, "42", true],
    [{ foo: true }],
  ];

  for (const value of values) {
    const { result } = await Runtime.evaluate({
      expression: `(${JSON.stringify(value)})`,
      returnByValue: true,
    });

    Assert.deepEqual(
      result,
      {
        type: typeof value,
        value,
        description: value != null ? value.toString() : value,
      },
      `Returned expected value for ${JSON.stringify(value)}`
    );
  }
});

add_task(async function returnByValueNotSerializable({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const notSerializableNumbers = {
    number: ["-0", "NaN", "Infinity", "-Infinity"],
    bigint: ["42n"],
  };

  for (const type in notSerializableNumbers) {
    for (const unserializableValue of notSerializableNumbers[type]) {
      const { result } = await Runtime.evaluate({
        expression: `(${unserializableValue})`,
        returnByValue: true,
      });

      Assert.deepEqual(
        result,
        {
          type,
          unserializableValue,
          description: unserializableValue,
        },
        `Returned expected value for ${JSON.stringify(unserializableValue)}`
      );
    }
  }
});

// Test undefined individually as JSON.stringify doesn't return a string
add_task(async function returnByValueUndefined({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { result } = await Runtime.evaluate({
    expression: "undefined",
    returnByValue: true,
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
