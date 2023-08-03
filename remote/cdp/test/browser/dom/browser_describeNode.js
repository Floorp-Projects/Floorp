/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DOC = toDataURL("<div id='content'><p>foo</p><p>bar</p></div>");
const DOC_FRAME = toDataURL(`<iframe src="${DOC}"></iframe>`);

add_task(async function objectIdInvalidTypes({ client }) {
  const { DOM } = client;

  for (const objectId of [null, true, 1, [], {}]) {
    await Assert.rejects(
      DOM.describeNode({ objectId }),
      /objectId: string value expected/,
      `Fails with invalid type: ${objectId}`
    );
  }
});

add_task(async function objectIdUnknownValue({ client }) {
  const { DOM } = client;

  await Assert.rejects(
    DOM.describeNode({ objectId: "foo" }),
    /Could not find object with given id/,
    `Fails with unknown objectId`
  );
});

add_task(async function objectIdIsNotANode({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({
    expression: "[42]",
  });

  await Assert.rejects(
    DOM.describeNode({ objectId: result.objectId }),
    /Object id doesn't reference a Node/,
    `Fails if objectId doesn't reference a DOM node`
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
  is(node.childNodeCount, 2, "Expected number of child nodes found");
  is(node.attributes.length, 2, "Found expected attribute's name and value");
  is(node.attributes[0], "id", "Found expected attribute name");
  is(node.attributes[1], "content", "Found expected attribute value");
  is(node.frameId, frameId, "Found expected frame id");
});

add_task(async function objectIdNoAttributes({ client }) {
  const { DOM, Runtime } = client;

  await Runtime.enable();
  const { result } = await Runtime.evaluate({
    expression: "document",
  });
  const { node } = await DOM.describeNode({
    objectId: result.objectId,
  });

  is(node.attributes, undefined, "No attributes returned");
});

add_task(async function objectIdDiffersForDifferentNodes({ client }) {
  const { DOM, Runtime } = client;

  await loadURL(DOC);

  await Runtime.enable();
  const { result: doc } = await Runtime.evaluate({
    expression: "document",
  });
  const { node: node1 } = await DOM.describeNode({
    objectId: doc.objectId,
  });

  const { result: body } = await Runtime.evaluate({
    expression: `document.getElementById('content')`,
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

add_task(async function frameIdForFrameElement({ client }) {
  const { DOM, Page, Runtime } = client;

  await Page.enable();

  const frameAttached = Page.frameAttached();
  await loadURL(DOC_FRAME);
  const { frameId, parentFrameId } = await frameAttached;

  await Runtime.enable();

  const { result: frameObj } = await Runtime.evaluate({
    expression: "document.getElementsByTagName('iframe')[0]",
  });
  const { node: frame } = await DOM.describeNode({
    objectId: frameObj.objectId,
  });

  is(frame.frameId, frameId, "Reported frameId is from the frame itself");
  isnot(
    frame.frameId,
    parentFrameId,
    "Reported frameId is not the parentFrameId"
  );
});
