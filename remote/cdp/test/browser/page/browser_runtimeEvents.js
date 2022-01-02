/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Assert the order of Runtime.executionContextDestroyed,
// Page.frameNavigated, and Runtime.executionContextCreated

add_task(async function testCDP({ client }) {
  await loadURL(PAGE_URL);

  const { Page, Runtime } = client;

  const events = [];
  function assertReceivedEvents(expected, message) {
    Assert.deepEqual(events, expected, message);
    // Empty the list of received events
    events.splice(0);
  }
  Page.frameNavigated(() => {
    events.push("frameNavigated");
  });
  Runtime.executionContextCreated(() => {
    events.push("executionContextCreated");
  });
  Runtime.executionContextDestroyed(() => {
    events.push("executionContextDestroyed");
  });

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Page.enable();
  info("Page domain has been enabled");

  const onExecutionContextCreated = Runtime.executionContextCreated();
  await Runtime.enable();
  info("Runtime domain has been enabled");

  // Runtime.enable will dispatch `executionContextCreated` for the existing document
  let { context } = await onExecutionContextCreated;
  ok(!!context.id, `The execution context has an id ${context.id}`);
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  assertReceivedEvents(
    ["executionContextCreated"],
    "Received only executionContextCreated event after Runtime.enable call"
  );

  const { frameTree } = await Page.getFrameTree();
  is(
    frameTree.frame.id,
    context.auxData.frameId,
    "getFrameTree and executionContextCreated refers about the same frame Id"
  );

  const onFrameNavigated = Page.frameNavigated();
  const onExecutionContextDestroyed = Runtime.executionContextDestroyed();
  const onExecutionContextCreated2 = Runtime.executionContextCreated();
  const url = toDataURL("test-page");
  const { frameId } = await Page.navigate({ url });
  info("A new page has been requested");
  ok(frameId, "Page.navigate returned a frameId");
  is(
    frameId,
    frameTree.frame.id,
    "The Page.navigate's frameId is the same than getFrameTree's one"
  );

  const frameNavigated = await onFrameNavigated;
  ok(
    !frameNavigated.frame.parentId,
    "frameNavigated is for the top level document and has a null parentId"
  );
  is(
    frameNavigated.frame.id,
    frameId,
    "frameNavigated id is the same than the one returned by Page.navigate"
  );
  is(
    frameNavigated.frame.name,
    undefined,
    "frameNavigated name isn't implemented yet"
  );
  is(
    frameNavigated.frame.url,
    url,
    "frameNavigated url is the same being given to Page.navigate"
  );

  const { executionContextId } = await onExecutionContextDestroyed;
  ok(executionContextId, "The destroyed event reports an id");
  is(
    executionContextId,
    context.id,
    "The destroyed event is for the first reported execution context"
  );

  ({ context } = await onExecutionContextCreated2);
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(
    context.auxData.frameId,
    frameId,
    "The execution context frame id is the same " +
      "the one returned by Page.navigate"
  );

  isnot(
    executionContextId,
    context.id,
    "The destroyed id is different from the created one"
  );

  assertReceivedEvents(
    ["executionContextDestroyed", "frameNavigated", "executionContextCreated"],
    "Received frameNavigated between the two execution context events during navigation to another URL"
  );
});
