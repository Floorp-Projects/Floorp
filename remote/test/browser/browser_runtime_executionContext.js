/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global getCDP */

const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");
const {RemoteAgentError} = ChromeUtils.import("chrome://remote/content/Error.jsm");

// Test the Runtime execution context events

const TEST_URI = "data:text/html;charset=utf-8,default-test-page";

add_task(async function() {
  try {
    await testCDP();
  } catch (e) {
    // Display better error message with the server side stacktrace
    // if an error happened on the server side:
    if (e.response) {
      throw RemoteAgentError.fromJSON(e.response);
    } else {
      throw e;
    }
  }
});

async function testCDP() {
  // Open a test page, to prevent debugging the random default page
  await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URI);

  // Start the CDP server
  RemoteAgent.init();
  RemoteAgent.tabs.start();
  RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

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

  const {Page, Runtime} = client;

  // turn on navigation related events, such as DOMContentLoaded et al.
  await Runtime.enable();
  ok(true, "Runtime domain has been enabled");

  // Calling Runtime.enable will emit executionContextCreated for the existing contexts
  const { context: context1 } = await Runtime.executionContextCreated();
  ok(!!context1.id, "The execution context has an id");
  ok(context1.auxData.isDefault, "The execution context is the default one");
  ok(!!context1.auxData.frameId, "The execution context has a frame id set");

  info("Navigate to a new URL");
  const executionContextDestroyed2 = Runtime.executionContextDestroyed();
  const executionContextCreated2 = Runtime.executionContextCreated();

  const url = "data:text/html;charset=utf-8,test-page";
  const { frameId } = await Page.navigate({ url });
  ok(true, "A new page has been loaded");
  is(frameId, context1.auxData.frameId, "Page.navigate returns the same frameId than executionContextCreated");

  let { executionContextId } = await executionContextDestroyed2;
  is(executionContextId, context1.id, "The destroyed event reports the previous context id");

  const { context: context2 } = await executionContextCreated2;
  ok(!!context2.id, "The execution context has an id");
  isnot(context1.id, context2.id, "The new execution context has a different id");
  ok(context2.auxData.isDefault, "The execution context is the default one");
  is(context2.auxData.frameId, frameId, "The execution context frame id is the same " +
    "than the one returned by Page.navigate");

  isnot(executionContextId, context2.id, "The destroyed id is different from the " +
    "created one");

  // Navigates back to the previous page.
  // This should resurect the original document from the BF Cache and recreate the
  // context for it
  info("Navigate back to the previous document");
  const executionContextDestroyed3 = Runtime.executionContextDestroyed();
  const executionContextCreated3 = Runtime.executionContextCreated();
  gBrowser.selectedBrowser.goBack();
  const { context: context3 } = await executionContextCreated3;
  is(context3.id, context1.id, "The new execution context should be the same than the first one");
  ok(context3.auxData.isDefault, "The execution context is the default one");
  is(context3.auxData.frameId, frameId, "The execution context frame id is always the same");

  ({ executionContextId } = await executionContextDestroyed3);
  is(executionContextId, context2.id, "The destroyed event reports the previous context id");
  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
}
