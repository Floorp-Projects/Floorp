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

  const executionContextCreated = Runtime.executionContextCreated();

  const url = "data:text/html;charset=utf-8,test-page";
  const { frameId } = await Page.navigate({ url  });
  ok(true, "A new page has been loaded");
  ok(frameId, "Page.navigate returned a frameId");

  const { context } = await executionContextCreated;
  ok(!!context.id, "The execution context has an id");
  ok(context.auxData.isDefault, "The execution context is the default one");
  is(context.auxData.frameId, frameId, "The execution context frame id is the same " +
    "the one returned by Page.navigate");

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
}
