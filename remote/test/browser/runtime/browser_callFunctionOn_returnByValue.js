/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
    {
      expression: "document",
      type: "object",
      subtype: "node",
      className: "HTMLDocument",
      description: "#document",
    },
    {
      expression: `{{
        const div = document.createElement('div');
        div.id = "foo";
        return div;
      }}`,
      type: "object",
      subtype: "node",
      className: "HTMLDivElement",
      description: "div#foo",
    },
  ];

  for (const entry of expressions) {
    const { expression, type, subtype, className, description } = entry;

    const { result } = await Runtime.callFunctionOn({
      functionDeclaration: `() => ${expression}`,
      executionContextId,
    });

    is(result.type, type, "The type is correct");
    is(result.subtype, subtype, "The subtype is correct");
    ok(!!result.objectId, "Got an object id");
    if (className) {
      is(result.className, className, "The className is correct");
    }
    if (description) {
      is(result.description, description, "The description is correct");
    }
  }
});

add_task(async function returnAsObjectDifferentObjectIds({ client }) {
  const { Runtime } = client;

  const executionContextId = await enableRuntime(client);

  const expressions = [{}, "document"];
  for (const expression of expressions) {
    const { result: result1 } = await Runtime.callFunctionOn({
      functionDeclaration: `() => ${JSON.stringify(expression)}`,
      executionContextId,
    });
    const { result: result2 } = await Runtime.callFunctionOn({
      functionDeclaration: `() => ${JSON.stringify(expression)}`,
      executionContextId,
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
