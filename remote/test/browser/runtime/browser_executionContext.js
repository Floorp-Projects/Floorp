/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime execution context events

const TEST_DOC = toDataURL("default-test-page");

add_task(async function(client) {
  await loadURL(TEST_DOC);

  const context1 = await testRuntimeEnable(client);
  await testEvaluate(client, context1);
  const context2 = await testNavigate(client, context1);
  const context3 = await testNavigateBack(client, context1, context2);
  const context4 = await testNavigateViaLocation(client, context3);
  await testReload(client, context4);
});

add_task(async function testRuntimeNotEnabled({ Runtime }) {
  await Runtime.disable();
  const sentinel = "timeout resolved";
  const created = Runtime.executionContextCreated().then(() => {
    return "executionContextCreated";
  });
  const destroyed = Runtime.executionContextDestroyed().then(() => {
    return "executionContextCreated";
  });
  const timeout = timeoutPromise(1000).then(() => {
    return sentinel;
  });
  await loadURL(TEST_DOC);
  const result = await Promise.race([created, destroyed, timeout]);
  is(result, sentinel, "No Runtime events emitted while Runtime is disabled");
});

async function testRuntimeEnable({ Runtime }) {
  const contextCreated = Runtime.executionContextCreated();
  // Enable watching for new execution context
  await Runtime.enable();
  info("Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await contextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(!!context.origin, "The execution context has an origin");
  is(context.name, "", "The default execution context is named ''");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");
  is(context.auxData.type, "default", "Execution context has 'default' type");

  return context;
}

async function testEvaluate({ Runtime }, previousContext) {
  const contextId = previousContext.id;

  const { result } = await Runtime.evaluate({
    contextId,
    expression: "location.href",
  });
  is(
    result.value,
    TEST_DOC,
    "Runtime.evaluate works and is against the test page"
  );
}

async function testNavigate({ Runtime, Page }, previousContext) {
  info("Navigate to a new URL");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  const { frameId } = await Page.navigate({ url: toDataURL("test-page") });
  info("A new page has been loaded");
  is(
    frameId,
    previousContext.auxData.frameId,
    "Page.navigate returns the same frameId as executionContextCreated"
  );

  const { executionContextId } = await executionContextDestroyed;
  is(
    executionContextId,
    previousContext.id,
    "The destroyed event reports the previous context id"
  );

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  isnot(
    previousContext.id,
    context.id,
    "The new execution context has a different id"
  );
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(
    context.auxData.frameId,
    frameId,
    "The execution context frame id is the same " +
      "than the one returned by Page.navigate"
  );
  is(context.auxData.type, "default", "Execution context has 'default' type");
  ok(!!context.origin, "The execution context has an origin");
  is(context.name, "", "The default execution context is named ''");

  isnot(
    executionContextId,
    context.id,
    "The destroyed id is different from the created one"
  );

  return context;
}

// Navigates back to the previous page.
// This should resurrect the original document from the BF Cache and recreate the
// context for it
async function testNavigateBack({ Runtime }, firstContext, previousContext) {
  info("Navigate back to the previous document");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  gBrowser.selectedBrowser.goBack();

  const { context } = await executionContextCreated;
  ok(!!context.origin, "The execution context has an origin");
  is(
    context.origin,
    firstContext.origin,
    "The new execution context should have the same origin as the first."
  );
  isnot(
    context.id,
    firstContext.id,
    "The new execution context should have a different id"
  );
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(
    context.auxData.frameId,
    firstContext.auxData.frameId,
    "The execution context frame id is always the same"
  );
  is(context.auxData.type, "default", "Execution context has 'default' type");
  is(context.name, "", "The default execution context is named ''");

  const { executionContextId } = await executionContextDestroyed;
  is(
    executionContextId,
    previousContext.id,
    "The destroyed event reports the previous context id"
  );

  const { result } = await Runtime.evaluate({
    contextId: context.id,
    expression: "location.href",
  });
  is(
    result.value,
    TEST_DOC,
    "Runtime.evaluate works and is against the page we just navigated to"
  );

  return context;
}

async function testNavigateViaLocation({ Runtime }, previousContext) {
  info("Navigate via window.location and Runtime.evaluate");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  const url2 = toDataURL("test-page-2");
  await Runtime.evaluate({
    contextId: previousContext.id,
    expression: `window.location = '${url2}';`,
  });

  const { executionContextId } = await executionContextDestroyed;
  is(
    executionContextId,
    previousContext.id,
    "The destroyed event reports the previous context id"
  );

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(
    context.auxData.frameId,
    previousContext.auxData.frameId,
    "The execution context frame id is the same " +
      "the one returned by Page.navigate"
  );
  is(context.auxData.type, "default", "Execution context has 'default' type");
  ok(!!context.origin, "The execution context has an origin");
  is(context.name, "", "The default execution context is named ''");

  isnot(
    executionContextId,
    context.id,
    "The destroyed id is different from the created one"
  );

  return context;
}

async function testReload({ Runtime, Page }, previousContext) {
  info("Test reloading via Page.reload");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  await Page.reload();

  const { executionContextId } = await executionContextDestroyed;
  is(
    executionContextId,
    previousContext.id,
    "The destroyed event reports the previous context id"
  );

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(
    context.auxData.frameId,
    previousContext.auxData.frameId,
    "The execution context frame id is the same one"
  );
  is(context.auxData.type, "default", "Execution context has 'default' type");
  ok(!!context.origin, "The execution context has an origin");
  is(context.name, "", "The default execution context is named ''");

  isnot(
    executionContextId,
    context.id,
    "The destroyed id is different from the created one"
  );
}
