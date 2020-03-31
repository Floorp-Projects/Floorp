/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL("<div id='content'><span>foo</span></div><div>bar</div>");

add_task(async function objectIdInvalidTypes({ client }) {
  const { DOM } = client;

  for (const objectId of [null, true, 1, [], {}]) {
    let errorThrown = "";
    try {
      await DOM.describeNode({ objectId });
    } catch (e) {
      errorThrown = e.message;
    }
    ok(
      errorThrown.match(/objectId: string value expected/),
      `Fails with invalid type: ${objectId}`
    );
  }
});

add_task(async function objectIdUnknownValue({ client }) {
  const { DOM } = client;

  let errorThrown = "";
  try {
    await DOM.describeNode({ objectId: "foo" });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(
    errorThrown.match(/Could not find object with given id/),
    "Fails with unknown objectId"
  );
});

add_task(async function objectIdIsNotANode({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({
    expression: "[42]",
  });

  let errorThrown = "";
  try {
    await DOM.describeNode({ objectId: result.objectId });
  } catch (e) {
    errorThrown = e.message;
  }
  ok(
    errorThrown.match(/Object id doesn't reference a Node/),
    "Fails if objectId doesn't reference a DOM node"
  );
});

add_task(async function objectIdAllProperties({ client }) {
  const { DOM, Page, Runtime } = client;

  await Page.enable();
  const { frameId } = await Page.navigate({ url: DOC });
  await Page.loadEventFired();

  await Runtime.enable();
  const { result } = await Runtime.evaluate({
    expression: `document.getElementById('content')`,
  });
  const { node } = await DOM.describeNode({
    objectId: result.objectId,
  });

  ok(!!node.nodeId, "The node has a node id");
  ok(!!node.backendNodeId, "The node has a backend node id");
  is(node.nodeName, "DIV", "Found expected node name");
  is(node.localName, "div", "Found expected local name");
  is(node.nodeType, 1, "Found expected node type");
  is(node.nodeValue, "", "Found expected node value");
  is(node.childNodeCount, 1, "Expected number of child nodes found");
  is(node.frameId, frameId, "Found expected frame id");
});

add_task(async function objectIdDiffersForDifferentNodes({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result: doc } = await Runtime.evaluate({
    expression: "document",
  });
  const { node: node1 } = await DOM.describeNode({
    objectId: doc.objectId,
  });

  const { result: body } = await Runtime.evaluate({
    expression: `document.body`,
  });
  const { node: node2 } = await DOM.describeNode({
    objectId: body.objectId,
  });

  for (const prop in node1) {
    if (["nodeValue", "frameId"].includes(prop)) {
      is(node1[prop], node2[prop], `Values of ${prop} are equal`);
    } else {
      isnot(node1[prop], node2[prop], `Values of ${prop} are different`);
    }
  }
});

add_task(async function objectIdDoesNotChangeForTheSameNode({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({
    expression: "document",
  });
  const { node: node1 } = await DOM.describeNode({
    objectId: result.objectId,
  });
  const { node: node2 } = await DOM.describeNode({
    objectId: result.objectId,
  });

  for (const prop in node1) {
    is(node1[prop], node2[prop], `Values of ${prop} are equal`);
  }
});
