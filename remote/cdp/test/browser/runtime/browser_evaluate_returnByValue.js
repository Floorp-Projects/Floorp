/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function returnByValueInvalidTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  for (const returnByValue of [null, 1, "foo", [], {}]) {
    await Assert.rejects(
      Runtime.evaluate({
        expression: "",
        returnByValue,
      }),
      err => err.message.includes("returnByValue: boolean value expected"),
      "returnByValue: boolean value expected"
    );
  }
});

add_task(async function returnByValueCyclicValue({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const expressions = ["const b = { a: 1}; b.b = b; b", "window"];

  for (const expression of expressions) {
    await Assert.rejects(
      Runtime.evaluate({
        expression,
        returnByValue: true,
      }),
      err => err.message.includes("Object reference chain is too long"),
      "Object reference chain is too long"
    );
  }
});

add_task(async function returnByValueNotPossible({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const expressions = ["Symbol(42)", "[Symbol(42)]", "{a: Symbol(42)}"];

  for (const expression of expressions) {
    await Assert.rejects(
      Runtime.evaluate({
        expression,
        returnByValue: true,
      }),
      err => err.message.includes("Object couldn't be returned by value"),
      "Object couldn't be returned by value"
    );
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
