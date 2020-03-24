/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOC = toDataURL("default-test-page");

add_task(async function FunctionDeclarationMissing({ client }) {
  const { Runtime } = client;

  let errorThrown = "";
  try {
    await Runtime.callFunctionOn();
  } catch (e) {
    errorThrown = e.message;
  }
  ok(errorThrown.includes("functionDeclaration: string value expected"));
});

add_task(async function functionDeclarationInvalidTypes({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  for (const functionDeclaration of [null, true, 1, [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({ functionDeclaration, executionContextId });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("functionDeclaration: string value expected"));
  }
});

add_task(async function functionDeclarationGetCurrentLocation({ client }) {
  const { Runtime } = client;

  await loadURL(TEST_DOC);
  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => location.href",
    executionContextId,
  });
  is(result.value, TEST_DOC, "Works against the test page");
});

add_task(async function argumentsInvalidTypes({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  for (const args of [null, true, 1, "foo", {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({
        functionDeclaration: "",
        arguments: args,
        executionContextId,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("arguments: array value expected"));
  }
});

add_task(async function argumentsPrimitiveTypes({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  for (const args of [null, true, 1, "foo", {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({
        functionDeclaration: "",
        arguments: args,
        executionContextId,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("arguments: array value expected"));
  }
});

add_task(async function awaitPromiseInvalidTypes({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  for (const awaitPromise of [null, 1, "foo", [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({
        functionDeclaration: "",
        awaitPromise,
        executionContextId,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("awaitPromise: boolean value expected"));
  }
});

add_task(async function awaitPromiseResolve({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => Promise.resolve(42)",
    awaitPromise: true,
    executionContextId,
  });

  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");
});

add_task(async function awaitPromiseDelayedResolve({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => new Promise(r => setTimeout(() => r(42), 0))",
    awaitPromise: true,
    executionContextId,
  });
  is(result.type, "number", "The type is correct");
  is(result.subtype, null, "The subtype is null for numbers");
  is(result.value, 42, "The result is the promise's resolution");
});

add_task(async function awaitPromiseReject({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { exceptionDetails } = await Runtime.callFunctionOn({
    functionDeclaration: "() => Promise.reject(42)",
    awaitPromise: true,
    executionContextId,
  });
  // TODO: Implement all values for exceptionDetails (bug 1548480)
  is(
    exceptionDetails.exception.value,
    42,
    "The result is the promise's rejection"
  );
});

add_task(async function awaitPromiseDelayedReject({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { exceptionDetails } = await Runtime.callFunctionOn({
    functionDeclaration:
      "() => new Promise((_,r) => setTimeout(() => r(42), 0))",
    awaitPromise: true,
    executionContextId,
  });
  is(
    exceptionDetails.exception.value,
    42,
    "The result is the promise's rejection"
  );
});

add_task(async function awaitPromiseResolveWithoutWait({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => Promise.resolve(42)",
    awaitPromise: false,
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.value, "We do not receive any value");
});

add_task(async function awaitPromiseDelayedResolveWithoutWait({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => new Promise(r => setTimeout(() => r(42), 0))",
    awaitPromise: false,
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.value, "We do not receive any value");
});

add_task(async function awaitPromiseRejectWithoutWait({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => Promise.reject(42)",
    awaitPromise: false,
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.exceptionDetails, "We do not receive any exception");
});

add_task(async function awaitPromiseDelayedRejectWithoutWait({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration:
      "() => new Promise((_,r) => setTimeout(() => r(42), 0))",
    awaitPromise: false,
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, "promise", "The subtype is promise");
  ok(!!result.objectId, "We got the object id for the promise");
  ok(!result.exceptionDetails, "We do not receive any exception");
});

add_task(async function executionContextIdNorObjectIdSpecified({ client }) {
  const { Runtime } = client;

  let errorThrown = "";
  try {
    await Runtime.callFunctionOn({
      functionDeclaration: "",
    });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(
    errorThrown.includes(
      "Either objectId or executionContextId must be specified"
    )
  );
});

add_task(async function executionContextIdInvalidTypes({ client }) {
  const { Runtime } = client;

  for (const executionContextId of [null, true, "foo", [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({
        functionDeclaration: "",
        executionContextId,
      });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("executionContextId: number value expected"));
  }
});

add_task(async function executionContextIdInvalidValue({ client }) {
  const { Runtime } = client;

  let errorThrown = "";
  try {
    await Runtime.callFunctionOn({
      functionDeclaration: "",
      executionContextId: -1,
    });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(errorThrown.includes("Cannot find context with specified id"));
});

add_task(async function objectIdInvalidTypes({ client }) {
  const { Runtime } = client;

  for (const objectId of [null, true, 1, [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({ functionDeclaration: "", objectId });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(errorThrown.includes("objectId: string value expected"));
  }
});

add_task(async function objectId({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  // First create an object
  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => ({ foo: 42 })",
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, null, "The subtype is null for objects");
  ok(!!result.objectId, "Got an object id");

  // Then apply a method on this object
  const { result: result2 } = await Runtime.callFunctionOn({
    functionDeclaration: "function () { return this.foo; }",
    executionContextId,
    objectId: result.objectId,
  });

  is(result2.type, "number", "The type is correct");
  is(result2.subtype, null, "The subtype is null for numbers");
  is(result2.value, 42, "Expected value returned");
});

add_task(async function objectIdArgumentReference({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  // First create a remote JS object
  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => ({ foo: 1 })",
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, null, "The subtype is null for objects");
  ok(!!result.objectId, "Got an object id");

  // Then increment the `foo` attribute of this JS object,
  // while returning this attribute value
  const { result: result2 } = await Runtime.callFunctionOn({
    functionDeclaration: "arg => ++arg.foo",
    arguments: [{ objectId: result.objectId }],
    executionContextId,
  });

  Assert.deepEqual(
    result2,
    {
      type: "number",
      value: 2,
    },
    "The result has the expected type and value"
  );

  // Finally, try to pass this JS object and get it back. Ensure that it
  // returns the same object id. Also increment the attribute again.
  const { result: result3 } = await Runtime.callFunctionOn({
    functionDeclaration: "arg => { arg.foo++; return arg; }",
    arguments: [{ objectId: result.objectId }],
    executionContextId,
  });

  is(result3.type, "object", "The type is correct");
  is(result3.subtype, null, "The subtype is null for objects");
  // Remote objects don't have unique ids. So you may have multiple object ids
  // that reference the same remote object
  ok(!!result3.objectId, "Got an object id");
  isnot(result3.objectId, result.objectId, "The object id is different");

  // Assert that we can still access this object and that its foo attribute
  // has been incremented. Use the second object id we got from previous call
  // to callFunctionOn.
  const { result: result4 } = await Runtime.callFunctionOn({
    functionDeclaration: "arg => arg.foo",
    arguments: [{ objectId: result3.objectId }],
    executionContextId,
  });

  Assert.deepEqual(
    result4,
    {
      type: "number",
      value: 3,
    },
    "The result has the expected type and value"
  );
});

add_task(async function returnAsObjectTypes({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

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
    const { result } = await Runtime.callFunctionOn({
      functionDeclaration: `() => ${expression}`,
      executionContextId,
    });
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

  const executionContextId = await enableRuntime(client);

  const expressions = [42, "42", true, 4.2];
  for (const expression of expressions) {
    const { result } = await Runtime.callFunctionOn({
      functionDeclaration: `() => ${JSON.stringify(expression)}`,
      executionContextId,
    });
    is(result.value, expression, `Evaluating primitive '${expression}' works`);
    is(result.type, typeof expression, `${expression} type is correct`);
  }
});

add_task(async function returnAsObjectNotSerializable({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const notSerializableNumbers = {
    number: ["-0", "NaN", "Infinity", "-Infinity"],
    bigint: ["42n"],
  };

  for (const type in notSerializableNumbers) {
    for (const expression of notSerializableNumbers[type]) {
      const { result } = await Runtime.callFunctionOn({
        functionDeclaration: `() => ${expression}`,
        executionContextId,
      });
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

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => null",
    executionContextId,
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

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => undefined",
    executionContextId,
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

  const executionContextId = await enableRuntime(client);

  for (const returnByValue of [null, 1, "foo", [], {}]) {
    let errorThrown = "";
    try {
      await Runtime.callFunctionOn({
        functionDeclaration: "",
        executionContextId,
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

  const executionContextId = await enableRuntime(client);

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
    const { result } = await Runtime.callFunctionOn({
      functionDeclaration: `() => (${JSON.stringify(value)})`,
      executionContextId,
      returnByValue: true,
    });

    Assert.deepEqual(
      result,
      {
        type: typeof value,
        value,
        description: value != null ? value.toString() : value,
      },
      "The returned value is the same than the input value"
    );
  }
});

add_task(async function returnByValueNotSerializable({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const notSerializableNumbers = {
    number: ["-0", "NaN", "Infinity", "-Infinity"],
    bigint: ["42n"],
  };

  for (const type in notSerializableNumbers) {
    for (const unserializableValue of notSerializableNumbers[type]) {
      const { result } = await Runtime.callFunctionOn({
        functionDeclaration: `() => (${unserializableValue})`,
        executionContextId,
        returnByValue: true,
      });

      Assert.deepEqual(
        result,
        {
          type,
          unserializableValue,
          description: unserializableValue,
        },
        "The returned value is the same than the input value"
      );
    }
  }
});

// Test undefined individually as JSON.stringify doesn't return a string
add_task(async function returnByValueUndefined({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => {}",
    executionContextId,
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

add_task(async function returnByValueArguments({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const values = [
    42,
    42.0,
    "42",
    true,
    false,
    null,
    { foo: true },
    { foo: { bar: 42, str: "str", array: [1, 2, 3] } },
    [42, "42", true],
    [{ foo: true }],
  ];

  for (const value of values) {
    const { result } = await Runtime.callFunctionOn({
      functionDeclaration: "a => a",
      arguments: [{ value }],
      executionContextId,
      returnByValue: true,
    });

    Assert.deepEqual(
      result,
      {
        type: typeof value,
        value,
        description: value != null ? value.toString() : value,
      },
      "The returned value is the same than the input value"
    );
  }
});

add_task(async function returnByValueArgumentsNotSerializable({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const notSerializableNumbers = {
    number: ["-0", "NaN", "Infinity", "-Infinity"],
    bigint: ["42n"],
  };

  for (const type in notSerializableNumbers) {
    for (const unserializableValue of notSerializableNumbers[type]) {
      const { result } = await Runtime.callFunctionOn({
        functionDeclaration: "a => a",
        arguments: [{ unserializableValue }],
        executionContextId,
        returnByValue: true,
      });

      Assert.deepEqual(
        result,
        {
          type,
          unserializableValue,
          description: unserializableValue,
        },
        "The returned value is the same than the input value"
      );
    }
  }
});

add_task(async function returnByValueArgumentsSymbol({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  let errorThrown = "";
  try {
    await Runtime.callFunctionOn({
      functionDeclaration: "a => a",
      arguments: [{ unserializableValue: "Symbol('42')" }],
      executionContextId,
      returnByValue: true,
    });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(errorThrown, "Symbol cannot be returned as value");
});

add_task(async function exceptionDetailsJavascriptError({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const { exceptionDetails } = await Runtime.callFunctionOn({
    functionDeclaration: "doesNotExists()",
    executionContextId,
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

  const executionContextId = await enableRuntime(client);

  const { exceptionDetails } = await Runtime.callFunctionOn({
    functionDeclaration: "() => { throw new Error('foo') }",
    executionContextId,
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

  const executionContextId = await enableRuntime(client);

  const { exceptionDetails } = await Runtime.callFunctionOn({
    functionDeclaration: "() => { throw 'foo' }",
    executionContextId,
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
