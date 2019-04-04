/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global getCDP */

const {RemoteAgent} = ChromeUtils.import("chrome://remote/content/RemoteAgent.jsm");

// Test the Target domain

add_task(async function() {
  try {
    await testCDP();
  } catch (e) {
    // Display better error message with the server side stacktrace
    // if an error happened on the server side:
    if (e.response) {
      throw new Error("CDP Exception:\n" + e.response + "\n");
    } else {
      throw e;
    }
  }
});

async function testCDP() {
  // Start the CDP server
  RemoteAgent.init();
  RemoteAgent.tabs.start();
  RemoteAgent.listen(Services.io.newURI("http://localhost:9222"));

  // Retrieve the chrome-remote-interface library object
  const CDP = await getCDP();

  // Connect to the server
  const {webSocketDebuggerUrl} = await CDP.Version();
  const client = await CDP({"target": webSocketDebuggerUrl});
  ok(true, "CDP client has been instantiated");

  const {Target} = client;
  ok("Target" in client, "Target domain is available");

  const targetCreatedForAlreadyOpenedTab = Target.targetCreated();

  // Instruct the server to fire Target.targetCreated events
  Target.setDiscoverTargets({ discover: true });

  // Calling `setDiscoverTargets` will dispatch `targetCreated` event for all
  // already opened tabs
  await targetCreatedForAlreadyOpenedTab;

  // Create a new target so that the test runs against a fresh new tab
  const targetCreated = Target.targetCreated();
  const {targetId} = await Target.createTarget();
  ok(true, "Target created");
  ok(!!targetId, "createTarget returns a non-empty target id");
  const {targetInfo} = await targetCreated;
  is(targetId, targetInfo.targetId, "createTarget and targetCreated refers to the same target id");
  is(targetInfo.type, "page", "The target is a page");

  const attachedToTarget = Target.attachedToTarget();
  const {sessionId} = await Target.attachToTarget({ targetId });
  ok(true, "Target attached");

  const attachedEvent = await attachedToTarget;
  ok(true, "Received Target.attachToTarget event");
  is(attachedEvent.sessionId, sessionId, "attachedToTarget and attachToTarget returns the same session id");
  is(attachedEvent.targetInfo.type, "page", "attachedToTarget creates a tab by default");

  await client.close();
  ok(true, "The client is closed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  await RemoteAgent.close();
}
