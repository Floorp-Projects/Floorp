/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the Runtime execution context events

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

add_task(async function() {
  // Open a test page, to prevent debugging the random default page
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Start the CDP server
  await RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

  // Retrieve the chrome-remote-interface library object
  const CDP = await getCDP();

  // Connect to the server
  const client = await CDP({
    target(list) {
      // Ensure debugging the right target, i.e. the one for our test tab.
      return list.find(target => target.url == TEST_URI);
    },
  });
  ok(true, "CDP client has been instantiated");

  const firstContext = await testRuntimeEnable(client);
  await testEvaluate(client, firstContext);
  const secondContext = await testNavigate(client, firstContext);
  await testNavigateBack(client, firstContext, secondContext);
  const thirdContext = await testNavigateViaLocation(client, firstContext);
  await testReload(client, thirdContext);

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
});

async function testRuntimeEnable({ Runtime }) {
  // Enable watching for new execution context
  await Runtime.enable();
  ok(true, "Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context } = await Runtime.executionContextCreated();
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  ok(!!context.auxData.frameId, "The execution context has a frame id set");

  return context;
}

async function testEvaluate({ Runtime }, previousContext) {
  const contextId = previousContext.id;

  const { result } = await Runtime.evaluate({ contextId, expression: "location.href" });
  is(result.value, TEST_URI, "Runtime.evaluate works and is against the test page");
}

async function testNavigate({ Runtime, Page }, previousContext) {
  info("Navigate to a new URL");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  const url = "data:text/html;charset=utf-8,test-page";
  const { frameId } = await Page.navigate({ url });
  ok(true, "A new page has been loaded");
  is(frameId, previousContext.auxData.frameId, "Page.navigate returns the same frameId than executionContextCreated");

  const { executionContextId } = await executionContextDestroyed;
  is(executionContextId, previousContext.id, "The destroyed event reports the previous context id");

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  isnot(previousContext.id, context.id, "The new execution context has a different id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(context.auxData.frameId, frameId, "The execution context frame id is the same " +
    "than the one returned by Page.navigate");

  isnot(executionContextId, context.id, "The destroyed id is different from the " +
    "created one");

  return context;
}

// Navigates back to the previous page.
// This should resurect the original document from the BF Cache and recreate the
// context for it
async function testNavigateBack({ Runtime }, firstContext, previousContext) {
  info("Navigate back to the previous document");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  gBrowser.selectedBrowser.goBack();

  const { context } = await executionContextCreated;
  is(context.id, firstContext.id, "The new execution context should be the same than the first one");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(context.auxData.frameId, firstContext.auxData.frameId, "The execution context frame id is always the same");

  const { executionContextId } = await executionContextDestroyed;
  is(executionContextId, previousContext.id, "The destroyed event reports the previous context id");

  const { result } = await Runtime.evaluate({ contextId: context.id, expression: "location.href" });
  is(result.value, TEST_URI, "Runtime.evaluate works and is against the page we just navigated to");
}

async function testNavigateViaLocation({ Runtime }, previousContext) {
  info("Navigate via window.location and Runtime.evaluate");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  const url2 = "data:text/html;charset=utf-8,test-page-2";
  await Runtime.evaluate({ contextId: previousContext.id, expression: `window.location = '${url2}';` });

  const { executionContextId } = await executionContextDestroyed;
  is(executionContextId, previousContext.id, "The destroyed event reports the previous context id");

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(context.auxData.frameId, previousContext.auxData.frameId, "The execution context frame id is the same " +
    "the one returned by Page.navigate");

  isnot(executionContextId, context.id, "The destroyed id is different from the " +
    "created one");

  return context;
}

async function testReload({ Runtime, Page }, previousContext) {
  info("Test reloading via Page.reload");

  const executionContextDestroyed = Runtime.executionContextDestroyed();
  const executionContextCreated = Runtime.executionContextCreated();

  await Page.reload();

  const { executionContextId } = await executionContextDestroyed;
  is(executionContextId, previousContext.id, "The destroyed event reports the previous context id");

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(context.auxData.frameId, previousContext.auxData.frameId, "The execution context " +
    "frame id is the same one");

  isnot(executionContextId, context.id, "The destroyed id is different from the " +
    "created one");
}
