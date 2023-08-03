/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOC = toDataURL("default-test-page");

add_task(async function contextIdInvalidValue({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  await Assert.rejects(
    Runtime.evaluate({ expression: "", contextId: -1 }),
    err => err.message.includes("Cannot find context with specified id"),
    "Cannot find context with specified id"
  );
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
  const { id: contextId } = await enableRuntime(client);

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
    { expression: "({foo:true})", type: "object", subtype: undefined },
    { expression: "Symbol('foo')", type: "symbol", subtype: undefined },
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
      expression: `(() => {{
        const div = document.createElement('div');
        div.id = "foo";
        return div;
      }})()`,
      type: "object",
      subtype: "node",
      className: "HTMLDivElement",
      description: "div#foo",
    },
  ];

  for (const entry of expressions) {
    const { expression, type, subtype, className, description } = entry;

    const { result } = await Runtime.evaluate({ expression });

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
