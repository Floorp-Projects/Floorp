/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOC = toDataURL("default-test-page");

add_task(async function FunctionDeclarationMissing({ client }) {
  const { Runtime } = client;
  await Assert.rejects(
    Runtime.callFunctionOn(),
    err => err.message.includes("functionDeclaration: string value expected"),
    "functionDeclaration: string value expected"
  );
});

add_task(async function functionDeclarationInvalidTypes({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

  for (const functionDeclaration of [null, true, 1, [], {}]) {
    await Assert.rejects(
      Runtime.callFunctionOn({ functionDeclaration, executionContextId }),
      err => err.message.includes("functionDeclaration: string value expected"),
      "functionDeclaration: string value expected"
    );
  }
});

add_task(async function functionDeclarationGetCurrentLocation({ client }) {
  const { Runtime } = client;

  await loadURL(TEST_DOC);
  const { id: executionContextId } = await enableRuntime(client);

  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => location.href",
    executionContextId,
  });
  is(result.value, TEST_DOC, "Works against the test page");
});

add_task(async function argumentsInvalidTypes({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

  for (const args of [null, true, 1, "foo", {}]) {
    await Assert.rejects(
      Runtime.callFunctionOn({
        functionDeclaration: "",
        arguments: args,
        executionContextId,
      }),
      err => err.message.includes("arguments: array value expected"),
      "arguments: array value expected"
    );
  }
});

add_task(async function argumentsPrimitiveTypes({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

  for (const args of [null, true, 1, "foo", {}]) {
    await Assert.rejects(
      Runtime.callFunctionOn({
        functionDeclaration: "",
        arguments: args,
        executionContextId,
      }),
      err => err.message.includes("arguments: array value expected"),
      "arguments: array value expected"
    );
  }
});

add_task(async function executionContextIdNorObjectIdSpecified({ client }) {
  const { Runtime } = client;

  await Assert.rejects(
    Runtime.callFunctionOn({
      functionDeclaration: "",
    }),
    err =>
      err.message.includes(
        "Either objectId or executionContextId must be specified"
      ),
    "Either objectId or executionContextId must be specified"
  );
});

add_task(async function executionContextIdInvalidTypes({ client }) {
  const { Runtime } = client;

  for (const executionContextId of [null, true, "foo", [], {}]) {
    await Assert.rejects(
      Runtime.callFunctionOn({
        functionDeclaration: "",
        executionContextId,
      }),
      err => err.message.includes("executionContextId: number value expected"),
      "executionContextId: number value expected"
    );
  }
});

add_task(async function executionContextIdInvalidValue({ client }) {
  const { Runtime } = client;
  await Assert.rejects(
    Runtime.callFunctionOn({
      functionDeclaration: "",
      executionContextId: -1,
    }),
    err => err.message.includes("Cannot find context with specified id"),
    "Cannot find context with specified id"
  );
});

add_task(async function objectIdInvalidTypes({ client }) {
  const { Runtime } = client;

  for (const objectId of [null, true, 1, [], {}]) {
    await Assert.rejects(
      Runtime.callFunctionOn({ functionDeclaration: "", objectId }),
      err => err.message.includes("objectId: string value expected"),
      "objectId: string value expected"
    );
  }
});

add_task(async function objectId({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

  // First create an object
  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => ({ foo: 42 })",
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, undefined, "The subtype is undefined for objects");
  ok(!!result.objectId, "Got an object id");

  // Then apply a method on this object
  const { result: result2 } = await Runtime.callFunctionOn({
    functionDeclaration: "function () { return this.foo; }",
    executionContextId,
    objectId: result.objectId,
  });

  is(result2.type, "number", "The type is correct");
  is(result2.subtype, undefined, "The subtype is undefined for numbers");
  is(result2.value, 42, "Expected value returned");
});

add_task(async function objectIdArgumentReference({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

  // First create a remote JS object
  const { result } = await Runtime.callFunctionOn({
    functionDeclaration: "() => ({ foo: 1 })",
    executionContextId,
  });

  is(result.type, "object", "The type is correct");
  is(result.subtype, undefined, "The subtype is undefined for objects");
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
  is(result3.subtype, undefined, "The subtype is undefined for objects");
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

add_task(async function exceptionDetailsJavascriptError({ client }) {
  const { Runtime } = client;

  const { id: executionContextId } = await enableRuntime(client);

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

  const { id: executionContextId } = await enableRuntime(client);

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

  const { id: executionContextId } = await enableRuntime(client);

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
