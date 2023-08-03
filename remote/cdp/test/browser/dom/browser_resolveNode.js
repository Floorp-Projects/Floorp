/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function backendNodeIdInvalidTypes({ client }) {
  const { DOM } = client;

  for (const backendNodeId of [null, true, "foo", [], {}]) {
    await Assert.rejects(
      DOM.resolveNode({ backendNodeId }),
      /backendNodeId: number value expected/,
      `Fails for invalid type: ${backendNodeId}`
    );
  }
});

add_task(async function backendNodeIdInvalidValue({ client }) {
  const { DOM } = client;

  await Assert.rejects(
    DOM.resolveNode({ backendNodeId: -1 }),
    /No node with given id found/,
    "Fails for unknown backendNodeId"
  );
});

add_task(async function backendNodeIdResultProperties({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({ expression: "document" });
  const { node } = await DOM.describeNode({ objectId: result.objectId });

  const { object } = await DOM.resolveNode({
    backendNodeId: node.backendNodeId,
  });

  ok(!!object, "Javascript node object returned");
  is(object.type, result.type, "Expected type returned");
  is(object.subtype, result.subtype, "Expected subtype returned");
  isnot(object.objectId, result.objectId, "Object has been duplicated");
});

add_task(async function executionContextIdInvalidTypes({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({ expression: "document" });
  const { node } = await DOM.describeNode({ objectId: result.objectId });

  for (const executionContextId of [null, true, "foo", [], {}]) {
    await Assert.rejects(
      DOM.resolveNode({
        backendNodeId: node.backendNodeId,
        executionContextId,
      }),
      /executionContextId: integer value expected/,
      `Fails for invalid type: ${executionContextId}`
    );
  }
});

add_task(async function executionContextIdInvalidValue({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({ expression: "document" });
  const { node } = await DOM.describeNode({ objectId: result.objectId });

  await Assert.rejects(
    DOM.resolveNode({
      backendNodeId: node.backendNodeId,
      executionContextId: -1,
    }),
    /Node with given id does not belong to the document/,
    "Fails for unknown executionContextId"
  );
});
