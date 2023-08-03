/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function awaitPromiseInvalidTypes({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  for (const awaitPromise of [null, 1, "foo", [], {}]) {
    await Assert.rejects(
      Runtime.evaluate({
        expression: "",
        awaitPromise,
      }),
      err => err.message.includes("awaitPromise: boolean value expected"),
      "awaitPromise: boolean value expected"
    );
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
  is(result.subtype, undefined, "The subtype is undefined for numbers");
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
  is(result.subtype, undefined, "The subtype is undefined for numbers");
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

add_task(async function awaitPromiseDelayedRejectError({ client }) {
  const { Runtime } = client;

  await enableRuntime(client);

  const { exceptionDetails } = await Runtime.evaluate({
    expression:
      "new Promise((_,r) => setTimeout(() => r(new Error('foo')), 0))",
    awaitPromise: true,
  });

  Assert.deepEqual(
    exceptionDetails,
    {
      text: "foo",
    },
    "Exception details are passed to the client"
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
